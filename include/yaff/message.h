#pragma once

#include "base.h"

namespace yaff {

namespace reflect {

struct AnyMessage;

}

template <typename M>
YAFF_LAYOUT_BEGIN(FixedMessage) {
public:
    using MetaType = M;

public:
    FixedMessage(const FixedMessage&) = delete;
    FixedMessage& operator=(const FixedMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const FieldId id, const T defaultVal) const noexcept {
        return ReadValueUnsafe<T>(ResolveField(id), defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const FieldId id, const T* defaultPtr = nullptr) const noexcept {
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
    YAFF_PURE bool ReadPresence(const FieldId id) const noexcept {
        return ReadPresenceUnsafe<T>(ResolveField(id));
    }

    static const FixedMessage<M>& Default() noexcept {
        return *yaff::ReadLayout<FixedMessage<M>>(DEFAULT_MESSAGE);
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

    FixedMessage() noexcept = default;

private:
    friend struct reflect::AnyMessage;

    inline static constexpr FieldOffset ResolveField(const FieldId id) {
        return M::FLAT_OFFSETS[id - 1];
    }

    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    template <typename T>
    YAFF_PURE T ReadValueUnsafe(const FieldOffset offset, const T defaultVal) const noexcept {
        return XorDef(yaff::ReadValue<T>(Message() + offset), defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutUnsafe(const FieldOffset offset, const T* defaultPtr = nullptr) const noexcept {
        const Offset pointer = yaff::ReadValue<Offset>(Message() + offset);
        return ResolveNullableOffset<T>(Message(), pointer, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceUnsafe(const FieldOffset offset) const noexcept {
        using TPresenceType = typename std::conditional_t<std::is_scalar<T>::value, T, Offset>;
        return yaff::ReadValue<TPresenceType>(Message() + offset) != 0;
    }

    std::byte Data_[MetaLimit()];
};
YAFF_LAYOUT_END

template <typename T>
concept CFixedMessage = std::is_base_of<FixedMessage<typename T::MetaType>, T>::value;

template <typename M>
YAFF_LAYOUT_BEGIN(FlatMessage) {
public:
    using MetaType = M;

public:
    FlatMessage(const FlatMessage&) = delete;
    FlatMessage& operator=(const FlatMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const FieldId id, const T defaultVal) const noexcept {
        const FieldId tl = TypedLimit_;
        return ToTypedLimit(id) < tl ? ReadValueUnsafe<T>(ResolveField(tl, id), defaultVal) : defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const FieldId id, const T* defaultPtr = nullptr) const noexcept {
        const FieldId tl = TypedLimit_;
        return ToTypedLimit(id) < tl ? ReadLayoutUnsafe<T>(ResolveField(tl, id), defaultPtr) : defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const FieldId id) const noexcept {
        const FieldId tl = TypedLimit_;
        return ToTypedLimit(id) < tl && ReadPresenceUnsafe<T>(tl, id, ResolveField(tl, id));
    }

    static const FlatMessage<M>& Default() noexcept {
        return *yaff::ReadLayout<FlatMessage<M>>(DEFAULT_MESSAGE);
    }

protected:
    inline static constexpr std::byte DEFAULT_MESSAGE[] = {std::byte{0x03}, std::byte{0x80}};

    FlatMessage() noexcept = default;

private:
    friend struct reflect::AnyMessage;

    template <typename T>
    friend struct DynamicMessage;

    // N.B.: It relies on the fact that the inline part of the data
    // can only be 1, 4, or 8 in size due to the underlying protobuf types.
    inline static constexpr std::array<FieldOffset, 4> CORRECTION_DICTIONARY = {0, 1, 4, 8};

    inline static constexpr FieldId ToTypedLimit(const FieldId id) noexcept {
        return ((id << 0x2) | 0x8003);
    }

    inline static constexpr bool IsFlat(const FieldId id) noexcept {
        return id & 0x8000;
    }

    inline static constexpr bool IsImplicit(const FieldId id) noexcept {
        return (id & 0x1) == 0;
    }

    inline static constexpr bool IsSized(const FieldId id) noexcept {
        return id & 0x2;
    }

    inline static constexpr bool IsStatic(const FieldId id) noexcept {
        return M::STATIC_FLAGS[id - 1];
    }

    inline static constexpr FieldOffset ResolveStaticField(const FieldId id) noexcept {
        return M::FLAT_OFFSETS[id - 1];
    }

    inline static constexpr size_t CalculateDeletedIndex(const FieldId id) noexcept {
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
        FieldOffset correction = 0;                          \
        const size_t end = CalculateDeletedIndex(id);        \
        for (size_t i = 0; i < end; ++i) {                   \
            const FieldId del = M::DELETED_IDS[i] - 1;       \
            correction += CORRECTION_DICTIONARY[expression]; \
        }                                                    \
        return correction;                                   \
    } while (0)

    YAFF_PURE FieldOffset ResolveImplicitCorrection(const FieldId id) const noexcept {
        YAFF_RETURN_CORRECTION((yaff::ReadValue<uint8_t>(Message() - (del >> 2) - 1) >> ((del & 3) << 1)) & 3);
    }

    YAFF_PURE FieldOffset ResolveExplicitCorrection(const FieldId id) const noexcept {
        YAFF_RETURN_CORRECTION(
            (YAFF_BSWAP16(yaff::ReadValue<uint16_t>(Message() - ((del * 3) >> 3) - 2)) >> (((del * 3) & 7) + 1)) & 3);
    }

#undef YAFF_RETURN_CORRECTION

    YAFF_PURE FieldOffset ResolveCorrection(const FieldId tl, const FieldId id) const noexcept {
        return YAFF_LIKELY(IsImplicit(tl)) ? ResolveImplicitCorrection(id) : ResolveExplicitCorrection(id);
    }

    YAFF_PURE FieldOffset ResolveQuasiStaticField(const FieldId tl, const FieldId id) const noexcept {
        // N.B.: We don't need an explicit dynamic check on IsSized here,
        // as our compilation rules ensure that the message can only have gaps
        // only when LAYOUT_FLAT is used as a dynamic alternative,
        // and in this mode, we always write Sized data.
        return ResolveStaticField(id) + ResolveCorrection(tl, id);
    }

    YAFF_PURE FieldOffset ResolveField(const FieldId tl, const FieldId id) const noexcept {
        // N.B.: Since the id is known during compilation when reading specific fields,
        // this branching is performed at compile-time:
        // * for static fields, ResolveField can be replaced with a constant from M::FLAT_OFFSETS[id - 1];
        // * for quasi-static fields, it can be replaced by a similar constant plus a dynamic correction.
        return IsStatic(id) ? ResolveStaticField(id) : ResolveQuasiStaticField(tl, id);
    }

    template <typename T>
    YAFF_PURE T ReadValueUnsafe(const FieldOffset offset, const T defaultVal) const noexcept {
        return XorDef(yaff::ReadValue<T>(Fields() + offset), defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutUnsafe(const FieldOffset offset, const T* defaultPtr = nullptr) const noexcept {
        const Offset pointer = yaff::ReadValue<Offset>(Fields() + offset);
        return ResolveNullableOffset<T>(Message(), pointer, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceUnsafe(const FieldId tl, const FieldId id, const FieldOffset offset) const noexcept {
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
    YAFF_PURE bool ReadExplicitPresenceUnsafe(const FieldId id) const noexcept {
        return yaff::ReadValue<uint8_t>(Message() - ((id - 1) >> 3) - 1) & (static_cast<uint8_t>(1) << ((id - 1) & 7));
    }

    template <typename T>
    YAFF_PURE bool ReadImplicitPresenceUnsafe(const FieldOffset offset) const noexcept {
        using TPresenceType = typename std::conditional_t<std::is_scalar<T>::value, T, Offset>;
        return yaff::ReadValue<TPresenceType>(Fields() + offset) != 0;
    }

    FieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename T>
concept CFlatMessage = std::is_base_of<FlatMessage<typename T::MetaType>, T>::value;

YAFF_LAYOUT_BEGIN(SparseMessage) {
public:
    SparseMessage(const SparseMessage&) = delete;
    SparseMessage& operator=(const SparseMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const FieldId id, const T defaultVal) const noexcept {
        return ToTypedLimit(id) < TypedLimit_ ? ReadValueUnsafe<T>(ResolveField(id), defaultVal) : defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const FieldId id, const T* defaultPtr) const noexcept {
        return ToTypedLimit(id) < TypedLimit_ ? ReadLayoutUnsafe<T>(ResolveField(id), defaultPtr) : defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const FieldId id) const noexcept {
        return ToTypedLimit(id) < TypedLimit_ && ReadPresenceUnsafe<T>(ResolveField(id));
    }

    static const SparseMessage& Default() noexcept {
        return *yaff::ReadLayout<SparseMessage>(DEFAULT_MESSAGE);
    }

protected:
    inline static constexpr std::byte DEFAULT_MESSAGE[] = {std::byte{0x3}, std::byte{0x0}};

    SparseMessage() noexcept = default;

private:
    friend struct reflect::AnyMessage;

    template <typename T>
    friend struct DynamicMessage;

    inline static constexpr FieldId ToTypedLimit(const FieldId id) noexcept {
        return ((id << 0x2) | 0x3);
    }

    inline static constexpr FieldId TINY_OFFSET_MAX_ID = 0x20;
    inline static constexpr FieldOffset SPARSE_META_OFFSET = sizeof(SignedOffset);

    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    YAFF_PURE const std::byte* ReadMeta() const noexcept {
        const SignedOffset offset = yaff::ReadValue<SignedOffset>(Message() - SPARSE_META_OFFSET);
        return ResolveOffset<std::byte>(Message(), offset);
    }

    YAFF_PURE FieldOffset ReadTinyOffset(const FieldId id) const noexcept {
        const std::byte* meta = ReadMeta();
        return yaff::ReadValue<uint8_t>(meta - id);
    }

    YAFF_PURE FieldOffset ReadOffset(const FieldId id) const noexcept {
        const std::byte* meta = ReadMeta();
        return yaff::ReadValue<uint16_t>(meta - (id << 0x1) + (TINY_OFFSET_MAX_ID - 0x1));
    }

    YAFF_PURE FieldOffset ResolveField(const FieldId id) const noexcept {
        // N.B.: Since the id is known during compilation when reading specific fields,
        // this branching is performed at compile-time and does not affect runtime operations.
        return id < TINY_OFFSET_MAX_ID ? ReadTinyOffset(id) : ReadOffset(id);
    }

    template <typename T>
    YAFF_PURE T ReadValueUnsafe(const FieldOffset offset, const T defaultVal) const noexcept {
        return offset ? yaff::ReadValue<T>(Message() + offset) : defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutUnsafe(const FieldOffset offset, const T* defaultPtr) const noexcept {
        return offset ? ResolveOffset<T>(Message(), yaff::ReadValue<Offset>(Message() + offset)) : defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceUnsafe(const FieldOffset offset) const noexcept {
        return offset != 0;
    }

    FieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename T>
concept CSparseMessage = std::is_base_of<SparseMessage, T>::value;

template <typename M>
YAFF_LAYOUT_BEGIN(DynamicMessage) {
public:
    using MetaType = M;

public:
    DynamicMessage(const DynamicMessage&) = delete;
    DynamicMessage& operator=(const DynamicMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const FieldId id, const T defaultVal) const noexcept {
        const FieldId tl = TypedLimit_;
        if (YAFF_LIKELY(FlatMessage<M>::ToTypedLimit(id) < tl)) {
            return AsFlatMessage()->template ReadValueUnsafe<T>(AsFlatMessage()->ResolveField(tl, id), defaultVal);
        }
        return ReadValueDispatch<T>(tl, id, defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const FieldId id, const T* defaultPtr = nullptr) const noexcept {
        const FieldId tl = TypedLimit_;
        if (YAFF_LIKELY(FlatMessage<M>::ToTypedLimit(id) < tl)) {
            return AsFlatMessage()->template ReadLayoutUnsafe<T>(AsFlatMessage()->ResolveField(tl, id), defaultPtr);
        }
        return ReadLayoutDispatch<T>(tl, id, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const FieldId id) const noexcept {
        const FieldId tl = TypedLimit_;
        if (YAFF_LIKELY(FlatMessage<M>::ToTypedLimit(id) < tl)) {
            return AsFlatMessage()->template ReadPresenceUnsafe<T>(tl, id, AsFlatMessage()->ResolveField(tl, id));
        }
        return ReadPresenceDispatch<T>(tl, id);
    }

    static const DynamicMessage<M>& Default() noexcept {
        return *yaff::ReadLayout<DynamicMessage<M>>(DEFAULT_MESSAGE);
    }

protected:
    inline static constexpr std::byte DEFAULT_MESSAGE[] = {std::byte{0x03}, std::byte{0x80}};

    DynamicMessage() noexcept = default;

private:
    friend struct reflect::AnyMessage;

    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    template <typename T>
    YAFF_PURE T ReadValueDispatch(const FieldId tl, const FieldId id, const T defaultVal) const noexcept {
        if (YAFF_UNLIKELY(FlatMessage<M>::IsFlat(tl))) {
            return defaultVal;
        }
        if (YAFF_LIKELY(SparseMessage::ToTypedLimit(id) < tl)) {
            return AsSparseMessage()->template ReadValueUnsafe<T>(AsSparseMessage()->ResolveField(id), defaultVal);
        }
        return defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutDispatch(const FieldId tl, const FieldId id, const T* defaultPtr = nullptr)
        const noexcept {
        if (YAFF_UNLIKELY(FlatMessage<M>::IsFlat(tl))) {
            return defaultPtr;
        }
        if (YAFF_LIKELY(SparseMessage::ToTypedLimit(id) < tl)) {
            return AsSparseMessage()->template ReadLayoutUnsafe<T>(AsSparseMessage()->ResolveField(id), defaultPtr);
        }
        return defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceDispatch(const FieldId tl, const FieldId id) const noexcept {
        if (YAFF_UNLIKELY(FlatMessage<M>::IsFlat(tl))) {
            return false;
        }
        if (YAFF_LIKELY(SparseMessage::ToTypedLimit(id) < tl)) {
            return AsSparseMessage()->template ReadPresenceUnsafe<T>(AsSparseMessage()->ResolveField(id));
        }
        return false;
    }

    YAFF_PURE const FlatMessage<M>* AsFlatMessage() const noexcept {
        return yaff::ReadLayout<FlatMessage<M>>(Message());
    }

    YAFF_PURE const SparseMessage* AsSparseMessage() const noexcept {
        return yaff::ReadLayout<SparseMessage>(Message());
    }

    FieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename T>
concept CDynamicMessage = std::is_base_of<DynamicMessage<typename T::MetaType>, T>::value;

template <typename T>
concept CMessage = CFixedMessage<T> or CFlatMessage<T> or CSparseMessage<T> or CDynamicMessage<T>;

}  // namespace yaff
