#pragma once

#include "base.h"
#include "table.h"

namespace NYaFF::NReflect {

YAFF_LAYOUT_BEGIN(TAnyTable) {
public:
    TAnyTable(const TAnyTable&) = delete;
    TAnyTable& operator=(const TAnyTable&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(ETableLayout tl, TFieldId id, T dflt, const TFieldResolverFunc* rslv = nullptr) const {
        switch (tl) {
            case ETableLayout::TABLE_LAYOUT_FIXED:
                return AsFixedTable()->ReadValueUnsafe<T>((*rslv)(id), dflt);
            case ETableLayout::TABLE_LAYOUT_SPARSE:
                return AsSparseTable()->ReadValue<T>(id, dflt);
            case ETableLayout::TABLE_LAYOUT_FLAT:
            case ETableLayout::TABLE_LAYOUT_DYNAMIC:
                return ReadValueDispatch(id, dflt, rslv);
            default:
                YAFF_THROW("unknown layout");
        }
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(ETableLayout tl, TFieldId id, const TFieldResolverFunc* rslv = nullptr) const {
        switch (tl) {
            case ETableLayout::TABLE_LAYOUT_FIXED:
                return AsFixedTable()->ReadLayoutUnsafe<T>((*rslv)(id), nullptr);
            case ETableLayout::TABLE_LAYOUT_SPARSE:
                return AsSparseTable()->ReadLayout<T>(id, nullptr);
            case ETableLayout::TABLE_LAYOUT_FLAT:
            case ETableLayout::TABLE_LAYOUT_DYNAMIC:
                return ReadLayoutDispatch<T>(id, rslv);
            default:
                YAFF_THROW("unknown layout");
        }
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(ETableLayout tl, const TFieldId id, const TFieldResolverFunc* rslv = nullptr) const {
        switch (tl) {
            case ETableLayout::TABLE_LAYOUT_FIXED:
                return AsFixedTable()->ReadPresenceUnsafe<T>((*rslv)(id));
            case ETableLayout::TABLE_LAYOUT_SPARSE:
                return AsSparseTable()->ReadPresence<T>(id);
            case ETableLayout::TABLE_LAYOUT_FLAT:
            case ETableLayout::TABLE_LAYOUT_DYNAMIC:
                return ReadPresenceDispatch<T>(id, rslv);
            default:
                YAFF_THROW("unknown layout");
        }
    }

private:
    TAnyTable() noexcept = default;

    template <typename T>
    YAFF_PURE T ReadValueDispatch(const TFieldId id, const T dflt, const TFieldResolverFunc* rslv = nullptr) const {
        const TFieldId typedLimit = NYaFF::ReadValue<TFieldId>(Table());
        if (TFlatTable<void>::ToTypedLimit(id) < typedLimit) {
            return AsFlatTable()->ReadValueUnsafe<T>((*rslv)(id), dflt);
        }
        return AsDynamicTable()->ReadValueDispatch<T>(typedLimit, id, dflt);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutDispatch(TFieldId id, const TFieldResolverFunc* rslv = nullptr) const {
        const TFieldId typedLimit = NYaFF::ReadValue<TFieldId>(Table());
        if (TFlatTable<void>::ToTypedLimit(id) < typedLimit) {
            return AsFlatTable()->ReadLayoutUnsafe<T>((*rslv)(id), nullptr);
        }
        return AsDynamicTable()->ReadLayoutDispatch<T>(typedLimit, id, nullptr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceDispatch(const TFieldId id, const TFieldResolverFunc* rslv = nullptr) const {
        const TFieldId typedLimit = NYaFF::ReadValue<TFieldId>(Table());
        if (TFlatTable<void>::ToTypedLimit(id) < typedLimit) {
            return AsFlatTable()->ReadPresenceUnsafe<T>(typedLimit, id, (*rslv)(id));
        }
        return AsDynamicTable()->ReadPresenceDispatch<T>(typedLimit, id);
    }

    YAFF_PURE const std::byte* Table() const {
        return reinterpret_cast<const std::byte*>(this);
    }

    YAFF_PURE const TFixedTable<void>* AsFixedTable() const {
        return NYaFF::ReadLayout<TFixedTable<void>>(Table());
    }

    YAFF_PURE const TFlatTable<void>* AsFlatTable() const {
        return NYaFF::ReadLayout<TFlatTable<void>>(Table());
    }

    YAFF_PURE const TSparseTable* AsSparseTable() const {
        return NYaFF::ReadLayout<TSparseTable>(Table());
    }

    YAFF_PURE const TDynamicTable<void>* AsDynamicTable() const {
        return NYaFF::ReadLayout<TDynamicTable<void>>(Table());
    }
};
YAFF_LAYOUT_END

}  // namespace NYaFF::NReflect
