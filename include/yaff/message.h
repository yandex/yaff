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
        return ReadValueUnsafe<T>(M::ResolveField(id), defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const TFieldId id, const T* defaultPtr = nullptr) const noexcept {
        return ReadLayoutUnsafe<T>(M::ResolveField(id), defaultPtr);
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
        return ReadPresenceUnsafe<T>(M::ResolveField(id));
    }

    static const TFixedMessage<M>& Default() noexcept {
        return *NYaFF::ReadLayout<TFixedMessage<M>>(DEFAULT_MESSAGE);
    }

protected:
    static constexpr size_t MetaLimit() noexcept {
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
        return ToTypedLimit(id) < TypedLimit_ ? ReadValueUnsafe<T>(M::ResolveField(id), defaultVal) : defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const TFieldId id, const T* defaultPtr = nullptr) const noexcept {
        return ToTypedLimit(id) < TypedLimit_ ? ReadLayoutUnsafe<T>(M::ResolveField(id), defaultPtr) : defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const TFieldId id) const noexcept {
        const TFieldId typedLimit = TypedLimit_;
        return ToTypedLimit(id) < typedLimit && ReadPresenceUnsafe<T>(typedLimit, id, M::ResolveField(id));
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

    inline static constexpr TFieldId ToTypedLimit(const TFieldId id) noexcept {
        return ((id << 0x2) | 0x8003);
    }

    inline static constexpr bool IsExplicit(const TFieldId id) noexcept {
        return id & 0x1;
    }

    YAFF_PURE const std::byte* Fields() const noexcept {
        return reinterpret_cast<const std::byte*>(this + 1);
    }

    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
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
        return YAFF_UNLIKELY(IsExplicit(tl)) ? ReadExplicitPresenceUnsafe<T>(id)
                                             : ReadImplicitPresenceUnsafe<T>(offset);
    }

    template <typename T>
    YAFF_PURE bool ReadExplicitPresenceUnsafe(const TFieldId id) const noexcept {
        return static_cast<std::uint8_t>(Message()[-((id - 0x1) >> 0x3) - 0x1]) &
               (static_cast<std::uint8_t>(0x1) << ((id - 0x1) & 0x7));
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
struct TDynamicMessage;

template <>
YAFF_LAYOUT_BEGIN(TDynamicMessage<void>) {
public:
    using TMetaType = void;

public:
    TDynamicMessage(const TDynamicMessage&) = delete;
    TDynamicMessage& operator=(const TDynamicMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const TFieldId id, const T defaultVal) const noexcept {
        return ReadValueDispatch<T>(TypedLimit_, id, defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const TFieldId id, const T* defaultPtr = nullptr) const noexcept {
        return ReadLayoutDispatch<T>(TypedLimit_, id, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const TFieldId id) const noexcept {
        return ReadPresenceDispatch<T>(TypedLimit_, id);
    }

    static const TDynamicMessage<void>& Default() noexcept {
        return *NYaFF::ReadLayout<TDynamicMessage<void>>(DEFAULT_MESSAGE);
    }

protected:
    inline static constexpr std::byte DEFAULT_MESSAGE[] = {std::byte{0x3}, std::byte{0x0}};

    TDynamicMessage() noexcept = default;

private:
    friend struct NReflect::TAnyMessage;

    template <typename T>
    friend struct TDynamicMessage;

    inline static constexpr bool IsFlat(const TFieldId id) noexcept {
        return id & 0x8000;
    }

    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    template <typename T>
    YAFF_PURE T ReadValueDispatch(const TFieldId typedLimit, const TFieldId id, const T defaultVal) const noexcept {
        if (YAFF_UNLIKELY(IsFlat(typedLimit))) {
            return defaultVal;
        }
        if (YAFF_LIKELY(TSparseMessage::ToTypedLimit(id) < typedLimit)) {
            return AsSparseMessage()->ReadValueUnsafe<T>(AsSparseMessage()->ResolveField(id), defaultVal);
        }
        return defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutDispatch(const TFieldId typedLimit, const TFieldId id, const T* defaultPtr = nullptr)
        const noexcept {
        if (YAFF_UNLIKELY(IsFlat(typedLimit))) {
            return defaultPtr;
        }
        if (YAFF_LIKELY(TSparseMessage::ToTypedLimit(id) < typedLimit)) {
            return AsSparseMessage()->ReadLayoutUnsafe<T>(AsSparseMessage()->ResolveField(id), defaultPtr);
        }
        return defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceDispatch(const TFieldId typedLimit, const TFieldId id) const noexcept {
        if (YAFF_UNLIKELY(IsFlat(typedLimit))) {
            return false;
        }
        if (YAFF_LIKELY(TSparseMessage::ToTypedLimit(id) < typedLimit)) {
            return AsSparseMessage()->ReadPresenceUnsafe<T>(AsSparseMessage()->ResolveField(id));
        }
        return false;
    }

    YAFF_PURE const TSparseMessage* AsSparseMessage() const noexcept {
        return NYaFF::ReadLayout<TSparseMessage>(Message());
    }

    TFieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename M>
YAFF_LAYOUT_BEGIN(TDynamicMessage) {
public:
    using TMetaType = M;

public:
    TDynamicMessage(const TDynamicMessage&) = delete;
    TDynamicMessage& operator=(const TDynamicMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const TFieldId id, const T defaultVal) const noexcept {
        const TFieldId typedLimit = TypedLimit_;
        if (YAFF_LIKELY(TFlatMessage<M>::ToTypedLimit(id) < typedLimit)) {
            return AsFlatMessage()->template ReadValueUnsafe<T>(M::ResolveField(id), defaultVal);
        }
        return AsDynamicMessage()->template ReadValueDispatch<T>(typedLimit, id, defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const TFieldId id, const T* defaultPtr = nullptr) const noexcept {
        const TFieldId typedLimit = TypedLimit_;
        if (YAFF_LIKELY(TFlatMessage<M>::ToTypedLimit(id) < typedLimit)) {
            return AsFlatMessage()->template ReadLayoutUnsafe<T>(M::ResolveField(id), defaultPtr);
        }
        return AsDynamicMessage()->template ReadLayoutDispatch<T>(typedLimit, id, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const TFieldId id) const noexcept {
        const TFieldId typedLimit = TypedLimit_;
        if (YAFF_LIKELY(TFlatMessage<M>::ToTypedLimit(id) < typedLimit)) {
            return AsFlatMessage()->template ReadPresenceUnsafe<T>(typedLimit, id, M::ResolveField(id));
        }
        return AsDynamicMessage()->template ReadPresenceDispatch<T>(typedLimit, id);
    }

    static const TDynamicMessage<M>& Default() noexcept {
        return *NYaFF::ReadLayout<TDynamicMessage<M>>(DEFAULT_MESSAGE);
    }

protected:
    inline static constexpr std::byte DEFAULT_MESSAGE[] = {std::byte{0x03}, std::byte{0x80}};

    TDynamicMessage() noexcept = default;

private:
    YAFF_PURE const std::byte* Message() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    YAFF_PURE const TFlatMessage<M>* AsFlatMessage() const noexcept {
        return NYaFF::ReadLayout<TFlatMessage<M>>(Message());
    }

    YAFF_PURE const TDynamicMessage<void>* AsDynamicMessage() const noexcept {
        return NYaFF::ReadLayout<TDynamicMessage<void>>(Message());
    }

    TFieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename T>
concept CDynamicMessage = std::is_base_of<TDynamicMessage<typename T::TMetaType>, T>::value;

template <typename T>
concept CMessage = CFixedMessage<T> or CFlatMessage<T> or CSparseMessage<T> or CDynamicMessage<T>;

}  // namespace NYaFF
