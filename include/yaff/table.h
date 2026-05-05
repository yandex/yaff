#pragma once

#include "base.h"

namespace NYaFF {

namespace NReflect {

struct TAnyTable;

}

template <typename M>
YAFF_LAYOUT_BEGIN(TFixedTable) {
public:
    using TMetaType = M;

public:
    TFixedTable(const TFixedTable&) = delete;
    TFixedTable& operator=(const TFixedTable&) = delete;

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

    static const TFixedTable<M>& Default() noexcept {
        return *NYaFF::ReadLayout<TFixedTable<M>>(DEFAULT_TABLE);
    }

protected:
    static constexpr size_t MetaLimit() noexcept {
        if constexpr (not std::is_void_v<M>) {
            return M::LIMIT;
        } else {
            return static_cast<size_t>(1);
        }
    }

    inline static constexpr std::byte DEFAULT_TABLE[MetaLimit()] = {};

    TFixedTable() noexcept = default;

private:
    friend struct NReflect::TAnyTable;

    YAFF_PURE const std::byte* Table() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    template <typename T>
    YAFF_PURE T ReadValueUnsafe(const TFieldOffset offset, const T defaultVal) const noexcept {
        return XorDef(NYaFF::ReadValue<T>(Table() + offset), defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutUnsafe(const TFieldOffset offset, const T* defaultPtr = nullptr) const noexcept {
        const TOffset pointer = NYaFF::ReadValue<TOffset>(Table() + offset);
        return ResolveNullableOffset<T>(Table(), pointer, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceUnsafe(const TFieldOffset offset) const noexcept {
        using TPresenceType = typename std::conditional_t<std::is_scalar<T>::value, T, TOffset>;
        return NYaFF::ReadValue<TPresenceType>(Table() + offset) != 0;
    }

    std::byte Data_[MetaLimit()];
};
YAFF_LAYOUT_END

template <typename T>
concept CFixedTable = std::is_base_of<TFixedTable<typename T::TMetaType>, T>::value;

template <typename M>
YAFF_LAYOUT_BEGIN(TFlatTable) {
public:
    using TMetaType = M;

public:
    TFlatTable(const TFlatTable&) = delete;
    TFlatTable& operator=(const TFlatTable&) = delete;

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

    static const TFlatTable<M>& Default() noexcept {
        return *NYaFF::ReadLayout<TFlatTable<M>>(DEFAULT_TABLE);
    }

protected:
    inline static constexpr std::byte DEFAULT_TABLE[] = {std::byte{0x03}, std::byte{0x80}};

    TFlatTable() noexcept = default;

private:
    friend struct NReflect::TAnyTable;

    template <typename T>
    friend struct TDynamicTable;

    inline static constexpr TFieldId ToTypedLimit(const TFieldId id) noexcept {
        return ((id << 0x2) | 0x8003);
    }

    inline static constexpr bool IsExplicit(const TFieldId id) noexcept {
        return id & 0x1;
    }

    YAFF_PURE const std::byte* Fields() const noexcept {
        return reinterpret_cast<const std::byte*>(this + 1);
    }

    YAFF_PURE const std::byte* Table() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    template <typename T>
    YAFF_PURE T ReadValueUnsafe(const TFieldOffset offset, const T defaultVal) const noexcept {
        return XorDef(NYaFF::ReadValue<T>(Fields() + offset), defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutUnsafe(const TFieldOffset offset, const T* defaultPtr = nullptr) const noexcept {
        const TOffset pointer = NYaFF::ReadValue<TOffset>(Fields() + offset);
        return ResolveNullableOffset<T>(Table(), pointer, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceUnsafe(const TFieldId tl, const TFieldId id, const TFieldOffset offset) const noexcept {
        return YAFF_UNLIKELY(IsExplicit(tl)) ? ReadExplicitPresenceUnsafe<T>(id)
                                             : ReadImplicitPresenceUnsafe<T>(offset);
    }

    template <typename T>
    YAFF_PURE bool ReadExplicitPresenceUnsafe(const TFieldId id) const noexcept {
        return static_cast<std::uint8_t>(Table()[-((id - 0x1) >> 0x3) - 0x1]) &
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
concept CFlatTable = std::is_base_of<TFlatTable<typename T::TMetaType>, T>::value;

YAFF_LAYOUT_BEGIN(TSparseTable) {
public:
    TSparseTable(const TSparseTable&) = delete;
    TSparseTable& operator=(const TSparseTable&) = delete;

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

    static const TSparseTable& Default() noexcept {
        return *NYaFF::ReadLayout<TSparseTable>(DEFAULT_TABLE);
    }

protected:
    inline static constexpr std::byte DEFAULT_TABLE[] = {std::byte{0x3}, std::byte{0x0}};

    TSparseTable() noexcept = default;

private:
    friend struct NReflect::TAnyTable;

    template <typename T>
    friend struct TDynamicTable;

    inline static constexpr TFieldId ToTypedLimit(const TFieldId id) noexcept {
        return ((id << 0x2) | 0x3);
    }

    inline static constexpr bool IsSparse(const TFieldId id) noexcept {
        return (id & 0x3) == 0x1;
    }

    inline static constexpr TFieldId TINY_OFFSET_MAX_ID = 0x20;

    YAFF_PURE const std::byte* Table() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    YAFF_PURE const std::byte* Data() const noexcept {
        return reinterpret_cast<const std::byte*>(this + 1);
    }

    YAFF_PURE const char* ReadMeta() const noexcept {
        return ResolveOffset<char>(Table(), NYaFF::ReadValue<TSignedOffset>(Data()));
    }

    YAFF_PURE TFieldOffset ReadTinyOffset(const TFieldId id) const noexcept {
        const char* meta = ReadMeta();
        return NYaFF::ReadValue<uint8_t>(meta + (id - 0x1));
    }

    YAFF_PURE TFieldOffset ReadOffset(const TFieldId id) const noexcept {
        const char* meta = ReadMeta();
        return NYaFF::ReadValue<uint16_t>(meta + (id << 0x1) - (TINY_OFFSET_MAX_ID + 0x1));
    }

    YAFF_PURE TFieldOffset ResolveField(const TFieldId id) const noexcept {
        // N.B.: Since the id is known during compilation when reading specific fields,
        // this branching is performed at compile-time and does not affect runtime operations.
        return id < TINY_OFFSET_MAX_ID ? ReadTinyOffset(id) : ReadOffset(id);
    }

    template <typename T>
    YAFF_PURE T ReadValueUnsafe(const TFieldOffset offset, const T defaultVal) const noexcept {
        return offset ? NYaFF::ReadValue<T>(Table() + offset) : defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutUnsafe(const TFieldOffset offset, const T* defaultPtr) const noexcept {
        return offset ? ResolveOffset<T>(Table(), NYaFF::ReadValue<TOffset>(Table() + offset)) : defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceUnsafe(const TFieldOffset offset) const noexcept {
        return offset != 0;
    }

    TFieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename T>
concept CSparseTable = std::is_base_of<TSparseTable, T>::value;

template <typename M>
struct TDynamicTable;

template <>
YAFF_LAYOUT_BEGIN(TDynamicTable<void>) {
public:
    using TMetaType = void;

public:
    TDynamicTable(const TDynamicTable&) = delete;
    TDynamicTable& operator=(const TDynamicTable&) = delete;

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

    static const TDynamicTable<void>& Default() noexcept {
        return *NYaFF::ReadLayout<TDynamicTable<void>>(DEFAULT_TABLE);
    }

protected:
    inline static constexpr std::byte DEFAULT_TABLE[] = {std::byte{0x3}, std::byte{0x0}};

    TDynamicTable() noexcept = default;

private:
    friend struct NReflect::TAnyTable;

    template <typename T>
    friend struct TDynamicTable;

    inline static constexpr bool IsFlat(const TFieldId id) noexcept {
        return id & 0x8000;
    }

    YAFF_PURE const std::byte* Table() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    template <typename T>
    YAFF_PURE T ReadValueDispatch(const TFieldId typedLimit, const TFieldId id, const T defaultVal) const noexcept {
        if (YAFF_UNLIKELY(IsFlat(typedLimit))) {
            return defaultVal;
        }
        if (YAFF_LIKELY(TSparseTable::ToTypedLimit(id) < typedLimit)) {
            return AsSparseTable()->ReadValueUnsafe<T>(AsSparseTable()->ResolveField(id), defaultVal);
        }
        return defaultVal;
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutDispatch(const TFieldId typedLimit, const TFieldId id, const T* defaultPtr = nullptr)
        const noexcept {
        if (YAFF_UNLIKELY(IsFlat(typedLimit))) {
            return defaultPtr;
        }
        if (YAFF_LIKELY(TSparseTable::ToTypedLimit(id) < typedLimit)) {
            return AsSparseTable()->ReadLayoutUnsafe<T>(AsSparseTable()->ResolveField(id), defaultPtr);
        }
        return defaultPtr;
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceDispatch(const TFieldId typedLimit, const TFieldId id) const noexcept {
        if (YAFF_UNLIKELY(IsFlat(typedLimit))) {
            return false;
        }
        if (YAFF_LIKELY(TSparseTable::ToTypedLimit(id) < typedLimit)) {
            return AsSparseTable()->ReadPresenceUnsafe<T>(AsSparseTable()->ResolveField(id));
        }
        return false;
    }

    YAFF_PURE const TSparseTable* AsSparseTable() const noexcept {
        return NYaFF::ReadLayout<TSparseTable>(Table());
    }

    TFieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename M>
YAFF_LAYOUT_BEGIN(TDynamicTable) {
public:
    using TMetaType = M;

public:
    TDynamicTable(const TDynamicTable&) = delete;
    TDynamicTable& operator=(const TDynamicTable&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(const TFieldId id, const T defaultVal) const noexcept {
        const TFieldId typedLimit = TypedLimit_;
        if (YAFF_LIKELY(TFlatTable<M>::ToTypedLimit(id) < typedLimit)) {
            return AsFlatTable()->template ReadValueUnsafe<T>(M::ResolveField(id), defaultVal);
        }
        return AsDynamicTable()->template ReadValueDispatch<T>(typedLimit, id, defaultVal);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(const TFieldId id, const T* defaultPtr = nullptr) const noexcept {
        const TFieldId typedLimit = TypedLimit_;
        if (YAFF_LIKELY(TFlatTable<M>::ToTypedLimit(id) < typedLimit)) {
            return AsFlatTable()->template ReadLayoutUnsafe<T>(M::ResolveField(id), defaultPtr);
        }
        return AsDynamicTable()->template ReadLayoutDispatch<T>(typedLimit, id, defaultPtr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(const TFieldId id) const noexcept {
        const TFieldId typedLimit = TypedLimit_;
        if (YAFF_LIKELY(TFlatTable<M>::ToTypedLimit(id) < typedLimit)) {
            return AsFlatTable()->template ReadPresenceUnsafe<T>(typedLimit, id, M::ResolveField(id));
        }
        return AsDynamicTable()->template ReadPresenceDispatch<T>(typedLimit, id);
    }

    static const TDynamicTable<M>& Default() noexcept {
        return *NYaFF::ReadLayout<TDynamicTable<M>>(DEFAULT_TABLE);
    }

protected:
    inline static constexpr std::byte DEFAULT_TABLE[] = {std::byte{0x03}, std::byte{0x80}};

    TDynamicTable() noexcept = default;

private:
    YAFF_PURE const std::byte* Table() const noexcept {
        return reinterpret_cast<const std::byte*>(this);
    }

    YAFF_PURE const TFlatTable<M>* AsFlatTable() const noexcept {
        return NYaFF::ReadLayout<TFlatTable<M>>(Table());
    }

    YAFF_PURE const TDynamicTable<void>* AsDynamicTable() const noexcept {
        return NYaFF::ReadLayout<TDynamicTable<void>>(Table());
    }

    TFieldId TypedLimit_;
};
YAFF_LAYOUT_END

template <typename T>
concept CDynamicTable = std::is_base_of<TDynamicTable<typename T::TMetaType>, T>::value;

template <typename T>
concept CTable = CFixedTable<T> or CFlatTable<T> or CSparseTable<T> or CDynamicTable<T>;

}  // namespace NYaFF
