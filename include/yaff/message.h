#pragma once

#include "base.h"

namespace NYaFF {

namespace NReflect {

struct TAnyMessage;

}

template <typename M>
YAFF_LAYOUT_BEGIN(TFixedMessage) {
public:
    using TMetaType = M;

public:
    TFixedMessage(const TFixedMessage&) = delete;
    TFixedMessage& operator=(const TFixedMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const TFieldId id, const T defaultVal) const noexcept {
        return ReadValueUnsafe<T>(ResolveField(id), defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const TFieldId id, const T* defaultPtr = nullptr) const noexcept {
        return ReadLayoutUnsafe<T>(ResolveField(id), defaultPtr);
    }

    // N.B.: Fixed layout always works through the implicit presence semantics for scalars and strings (and, of course,
    // repeated and maps) and through the explicit presence for embedded messages. Oneof is not supported by fixed
    // layout, so this case is not considered.
    //
    // Since the default scalar value written in YaFF is always 0 due to xor with the default value,
    // and any initialized embedded message is written through a non-zero offset,
    // checking for non-zero values is sufficient to ensure the semantics described above.
    template <typename T>
    YAFF_PURE bool ReadPresence(const TFieldId id) const noexcept {
        return ReadPresenceUnsafe<T>(ResolveField(id));
    }

    static const TFixedMessage<M>& Default() noexcept {
        return *NYaFF::ReadLayout<TFixedMessage<M>>(DEFAULT_MESSAGE);
    }

protected:
    inline static constexpr size_t MetaLimit() noexcept {
        if constexpr (not std::is_void_v<M>) {
            return M::LIMIT;
        } else {
            return static_cast<size_t>(1);
        }
    }

    inline static constexpr std::byte DEFAULT_MESSAGE[MetaLimit()] = {};

    TFixedMessage() noexcept = default;

private:
    friend struct NReflect::TAnyMessage;

    inline static constexpr TFieldOffset ResolveField(const TFieldId id) {
        return M::FLAT_OFFSETS[id - 1];
    }

    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    template <typename T>
    YAFF_PURE T ReadValueUnsafe(const TFieldOffset offset, const T defaultVal) const noexcept {
        return XorDef(NYaFF::ReadValue<T>(Message() + offset), defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutUnsafe(const TFieldOffset offset, const T* defaultPtr = nullptr) const noexcept {
        const TOffset pointer = NYaFF::ReadValue<TOffset>(Message() + offset);
        return ResolveNullableOffset<T>(Message(), pointer, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceUnsafe(const TFieldOffset offset) const noexcept {
        using TPresenceType = typename std::conditional_t<std::is_scalar<T>::value, T, TOffset>;
        return NYaFF::ReadValue<TPresenceType>(Message() + offset) != 0;
    }

    std::byte Data_[MetaLimit()];
};
YAFF_LAYOUT_END

template <typename T>
concept CFixedMessage = std::is_base_of<TFixedMessage<typename T::TMetaType>, T>::value;

template <typename M>
YAFF_LAYOUT_BEGIN(TFlatMessage) {
public:
    using TMetaType = M;

public:
    TFlatMessage(const TFlatMessage&) = delete;
    TFlatMessage& operator=(const TFlatMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const TFieldId id, const T defaultVal) const noexcept {
        const TFieldId tl = TypedLimit_;
        return ToTypedLimit(id) < tl ? ReadValueUnsafe<T>(ResolveField(tl, id), defaultVal) : defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const TFieldId id, const T* defaultPtr = nullptr) const noexcept {
        const TFieldId tl = TypedLimit_;
        return ToTypedLimit(id) < tl ? ReadLayoutUnsafe<T>(ResolveField(tl, id), defaultPtr) : defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const TFieldId id) const noexcept {
        const TFieldId tl = TypedLimit_;
        return ToTypedLimit(id) < tl && ReadPresenceUnsafe<T>(tl, id, ResolveField(tl, id));
    }

    static const TFlatMessage<M>& Default() noexcept {
        return *NYaFF::ReadLayout<TFlatMessage<M>>(DEFAULT_MESSAGE);
    }

protected:
    inline static constexpr std::byte DEFAULT_MESSAGE[] = {std::byte{0x03}, std::byte{0x80}};

    TFlatMessage() noexcept = default;

private:
    friend struct NReflect::TAnyMessage;

    template <typename T>
    friend struct TDynamicMessage;

    // N.B.: It relies on the fact that the inline part of the data
    // can only be 1, 4, or 8 in size due to the underlying protobuf types.
    inline static constexpr std::array<TFieldOffset, 4> CORRECTION_DICTIONARY = {0, 1, 4, 8};

    inline static constexpr TFieldId ToTypedLimit(const TFieldId id) noexcept {
        return ((id << 0x2) | 0x8003);
    }

    inline static constexpr bool IsFlat(const TFieldId id) noexcept {
        return id & 0x8000;
    }

    inline static constexpr bool IsImplicit(const TFieldId id) noexcept {
        return (id & 0x1) == 0;
    }

    inline static constexpr bool IsSized(const TFieldId id) noexcept {
        return id & 0x2;
    }

    inline static constexpr bool IsStatic(const TFieldId id) noexcept {
        return M::STATIC_FLAGS[id - 1];
    }

    inline static constexpr TFieldOffset ResolveStaticField(const TFieldId id) noexcept {
        return M::FLAT_OFFSETS[id - 1];
    }

    inline static constexpr size_t CalculateDeletedIndex(const TFieldId id) noexcept {
        const auto idx = std::lower_bound(M::DELETED_IDS.begin(), M::DELETED_IDS.end(), id);
        return std::distance(M::DELETED_IDS.begin(), idx);
    }

    YAFF_PURE const std::byte* Fields() const noexcept {
        return reinterpret_cast<const std::byte*>(this + 1);
    }

    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

// N.B.: The calculation of correction relies on compiler optimizations.
// The optimization is based on two assumptions:
//  * field id and static metadata are known at compile time;
//  * this code is activated for backward compatibility during the first removals
//    of fields in a dense schema, so the number of skip corrections is low.
// Therefore, loop unrolling and reads at known compile-time offsets are expected,
// along with a small number of fast arithmetic operations.
//
// A macro approach is used to explicitly branch outside the loop,
// as we cannot rely on the optimizer's pass at this point.
//
// TODO: For a large number of skipped fields, an approach that calculates the correction by counting
// the number of ones in the field's bitmask is more efficient.
// Since we know the number of skips statically,
// we can perform static dispatching in future.
//
// TODO: The current correction is calculated by reading from the static CORRECTION_DICTIONARY.
// It might be worth trying an alternative that uses arithmetic:
// (meta & 1) + ((meta >> 1) << 2) + 3 * ((meta & 1) & (meta >> 1));
#define YAFF_RETURN_CORRECTION(expression)                   \
    do {                                                     \
        TFieldOffset correction = 0;                         \
        const size_t end = CalculateDeletedIndex(id);        \
        for (size_t i = 0; i < end; ++i) {                   \
            const TFieldId del = M::DELETED_IDS[i] - 1;      \
            correction += CORRECTION_DICTIONARY[expression]; \
        }                                                    \
        return correction;                                   \
    } while (0)

    YAFF_PURE TFieldOffset ResolveImplicitCorrection(const TFieldId id) const noexcept {
        YAFF_RETURN_CORRECTION((NYaFF::ReadValue<uint8_t>(Message() - (del >> 2) - 1) >> ((del & 3) << 1)) & 3);
    }

    YAFF_PURE TFieldOffset ResolveExplicitCorrection(const TFieldId id) const noexcept {
        YAFF_RETURN_CORRECTION(
            (YAFF_BSWAP16(NYaFF::ReadValue<uint16_t>(Message() - ((del * 3) >> 3) - 2)) >> (((del * 3) & 7) + 1)) & 3);
    }

#undef YAFF_RETURN_CORRECTION

    YAFF_PURE TFieldOffset ResolveCorrection(const TFieldId tl, const TFieldId id) const noexcept {
        return YAFF_LIKELY(IsImplicit(tl)) ? ResolveImplicitCorrection(id) : ResolveExplicitCorrection(id);
    }

    YAFF_PURE TFieldOffset ResolveQuasiStaticField(const TFieldId tl, const TFieldId id) const noexcept {
        // N.B.: We don't need an explicit dynamic check on IsSized here,
        // as our compilation rules ensure that the message can only have gaps
        // only when LAYOUT_FLAT is used as a dynamic alternative,
        // and in this mode, we always write Sized data.
        return ResolveStaticField(id) + ResolveCorrection(tl, id);
    }

    YAFF_PURE TFieldOffset ResolveField(const TFieldId tl, const TFieldId id) const noexcept {
        // N.B.: Since the id is known during compilation when reading specific fields,
        // this branching is performed at compile-time:
        // * for static fields, ResolveField can be replaced with a constant from M::FLAT_OFFSETS[id - 1];
        // * for quasi-static fields, it can be replaced by a similar constant plus a dynamic correction.
        return IsStatic(id) ? ResolveStaticField(id) : ResolveQuasiStaticField(tl, id);
    }

    template <typename T>
    YAFF_PURE T ReadValueUnsafe(const TFieldOffset offset, const T defaultVal) const noexcept {
        return XorDef(NYaFF::ReadValue<T>(Fields() + offset), defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutUnsafe(const TFieldOffset offset, const T* defaultPtr = nullptr) const noexcept {
        const TOffset pointer = NYaFF::ReadValue<TOffset>(Fields() + offset);
        return ResolveNullableOffset<T>(Message(), pointer, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceUnsafe(const TFieldId tl, const TFieldId id, const TFieldOffset offset) const noexcept {
        if (YAFF_LIKELY(IsImplicit(tl))) {
            return ReadImplicitPresenceUnsafe<T>(offset);
        }
        // N.B.: Since the id is known during compilation when reading specific fields,
        // all arithmetic expressions result in reading a single byte at compile time known offset
        // and testing a single bit at compile time known index.
        //
        // TODO: It is possible to make a branchless implementation based on arithmetic with IsSized(tl),
        // but in this case, we no longer understand the byte index at compile time.
        return YAFF_LIKELY(IsSized(tl)) ? ReadExplicitPresenceUnsafe<T>(3 * id - 2) : ReadExplicitPresenceUnsafe<T>(id);
    }

    template <typename T>
    YAFF_PURE bool ReadExplicitPresenceUnsafe(const TFieldId id) const noexcept {
        return NYaFF::ReadValue<uint8_t>(Message() - ((id - 1) >> 3) - 1) & (static_cast<uint8_t>(1) << ((id - 1) & 7));
    }

    template <typename T>
    YAFF_PURE bool ReadImplicitPresenceUnsafe(const TFieldOffset offset) const noexcept {
        using TPresenceType = typename std::conditional_t<std::is_scalar<T>::value, T, TOffset>;
        return NYaFF::ReadValue<TPresenceType>(Fields() + offset) != 0;
    }

    TFieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename T>
concept CFlatMessage = std::is_base_of<TFlatMessage<typename T::TMetaType>, T>::value;

YAFF_LAYOUT_BEGIN(TSparseMessage) {
public:
    TSparseMessage(const TSparseMessage&) = delete;
    TSparseMessage& operator=(const TSparseMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const TFieldId id, const T defaultVal) const noexcept {
        return ToTypedLimit(id) < TypedLimit_ ? ReadValueUnsafe<T>(ResolveField(id), defaultVal) : defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const TFieldId id, const T* defaultPtr) const noexcept {
        return ToTypedLimit(id) < TypedLimit_ ? ReadLayoutUnsafe<T>(ResolveField(id), defaultPtr) : defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const TFieldId id) const noexcept {
        return ToTypedLimit(id) < TypedLimit_ && ReadPresenceUnsafe<T>(ResolveField(id));
    }

    static const TSparseMessage& Default() noexcept {
        return *NYaFF::ReadLayout<TSparseMessage>(DEFAULT_MESSAGE);
    }

protected:
    inline static constexpr std::byte DEFAULT_MESSAGE[] = {std::byte{0x3}, std::byte{0x0}};

    TSparseMessage() noexcept = default;

private:
    friend struct NReflect::TAnyMessage;

    template <typename T>
    friend struct TDynamicMessage;

    inline static constexpr TFieldId ToTypedLimit(const TFieldId id) noexcept {
        return ((id << 0x2) | 0x3);
    }

    inline static constexpr TFieldId TINY_OFFSET_MAX_ID = 0x20;
    inline static constexpr TFieldOffset SPARSE_META_OFFSET = sizeof(TSignedOffset);

    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    YAFF_PURE const std::byte* ReadMeta() const noexcept {
        const TSignedOffset offset = NYaFF::ReadValue<TSignedOffset>(Message() - SPARSE_META_OFFSET);
        return ResolveOffset<std::byte>(Message(), offset);
    }

    YAFF_PURE TFieldOffset ReadTinyOffset(const TFieldId id) const noexcept {
        const std::byte* meta = ReadMeta();
        return NYaFF::ReadValue<uint8_t>(meta - id);
    }

    YAFF_PURE TFieldOffset ReadOffset(const TFieldId id) const noexcept {
        const std::byte* meta = ReadMeta();
        return NYaFF::ReadValue<uint16_t>(meta - (id << 0x1) + (TINY_OFFSET_MAX_ID - 0x1));
    }

    YAFF_PURE TFieldOffset ResolveField(const TFieldId id) const noexcept {
        // N.B.: Since the id is known during compilation when reading specific fields,
        // this branching is performed at compile-time and does not affect runtime operations.
        return id < TINY_OFFSET_MAX_ID ? ReadTinyOffset(id) : ReadOffset(id);
    }

    template <typename T>
    YAFF_PURE T ReadValueUnsafe(const TFieldOffset offset, const T defaultVal) const noexcept {
        return offset ? NYaFF::ReadValue<T>(Message() + offset) : defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutUnsafe(const TFieldOffset offset, const T* defaultPtr) const noexcept {
        return offset ? ResolveOffset<T>(Message(), NYaFF::ReadValue<TOffset>(Message() + offset)) : defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceUnsafe(const TFieldOffset offset) const noexcept {
        return offset != 0;
    }

    TFieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename T>
concept CSparseMessage = std::is_base_of<TSparseMessage, T>::value;

template <typename M>
YAFF_LAYOUT_BEGIN(TDynamicMessage) {
public:
    using TMetaType = M;

public:
    TDynamicMessage(const TDynamicMessage&) = delete;
    TDynamicMessage& operator=(const TDynamicMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const TFieldId id, const T defaultVal) const noexcept {
        const TFieldId tl = TypedLimit_;
        if (YAFF_LIKELY(TFlatMessage<M>::ToTypedLimit(id) < tl)) {
            return AsFlatMessage()->template ReadValueUnsafe<T>(AsFlatMessage()->ResolveField(tl, id), defaultVal);
        }
        return ReadValueDispatch<T>(tl, id, defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const TFieldId id, const T* defaultPtr = nullptr) const noexcept {
        const TFieldId tl = TypedLimit_;
        if (YAFF_LIKELY(TFlatMessage<M>::ToTypedLimit(id) < tl)) {
            return AsFlatMessage()->template ReadLayoutUnsafe<T>(AsFlatMessage()->ResolveField(tl, id), defaultPtr);
        }
        return ReadLayoutDispatch<T>(tl, id, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const TFieldId id) const noexcept {
        const TFieldId tl = TypedLimit_;
        if (YAFF_LIKELY(TFlatMessage<M>::ToTypedLimit(id) < tl)) {
            return AsFlatMessage()->template ReadPresenceUnsafe<T>(tl, id, AsFlatMessage()->ResolveField(tl, id));
        }
        return ReadPresenceDispatch<T>(tl, id);
    }

    static const TDynamicMessage<M>& Default() noexcept {
        return *NYaFF::ReadLayout<TDynamicMessage<M>>(DEFAULT_MESSAGE);
    }

protected:
    inline static constexpr std::byte DEFAULT_MESSAGE[] = {std::byte{0x03}, std::byte{0x80}};

    TDynamicMessage() noexcept = default;

private:
    friend struct NReflect::TAnyMessage;

    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    template <typename T>
    YAFF_PURE T ReadValueDispatch(const TFieldId tl, const TFieldId id, const T defaultVal) const noexcept {
        if (YAFF_UNLIKELY(TFlatMessage<M>::IsFlat(tl))) {
            return defaultVal;
        }
        if (YAFF_LIKELY(TSparseMessage::ToTypedLimit(id) < tl)) {
            return AsSparseMessage()->template ReadValueUnsafe<T>(AsSparseMessage()->ResolveField(id), defaultVal);
        }
        return defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutDispatch(const TFieldId tl, const TFieldId id, const T* defaultPtr = nullptr)
        const noexcept {
        if (YAFF_UNLIKELY(TFlatMessage<M>::IsFlat(tl))) {
            return defaultPtr;
        }
        if (YAFF_LIKELY(TSparseMessage::ToTypedLimit(id) < tl)) {
            return AsSparseMessage()->template ReadLayoutUnsafe<T>(AsSparseMessage()->ResolveField(id), defaultPtr);
        }
        return defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceDispatch(const TFieldId tl, const TFieldId id) const noexcept {
        if (YAFF_UNLIKELY(TFlatMessage<M>::IsFlat(tl))) {
            return false;
        }
        if (YAFF_LIKELY(TSparseMessage::ToTypedLimit(id) < tl)) {
            return AsSparseMessage()->template ReadPresenceUnsafe<T>(AsSparseMessage()->ResolveField(id));
        }
        return false;
    }

    YAFF_PURE const TFlatMessage<M>* AsFlatMessage() const noexcept {
        return NYaFF::ReadLayout<TFlatMessage<M>>(Message());
    }

    YAFF_PURE const TSparseMessage* AsSparseMessage() const noexcept {
        return NYaFF::ReadLayout<TSparseMessage>(Message());
    }

    TFieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename T>
concept CDynamicMessage = std::is_base_of<TDynamicMessage<typename T::TMetaType>, T>::value;

template <typename T>
concept CMessage = CFixedMessage<T> or CFlatMessage<T> or CSparseMessage<T> or CDynamicMessage<T>;

}  // namespace NYaFF
