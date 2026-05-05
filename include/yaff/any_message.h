#pragma once

#include "base.h"
#include "message.h"

namespace NYaFF::NReflect {

YAFF_LAYOUT_BEGIN(TAnyMessage) {
public:
    TAnyMessage(const TAnyMessage&) = delete;
    TAnyMessage& operator=(const TAnyMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(EMessageLayout tl, TFieldId id, T dflt, const TFieldResolverFunc* rslv = nullptr) const {
        switch (tl) {
            case EMessageLayout::MESSAGE_LAYOUT_FIXED:
                return AsFixedMessage()->ReadValueUnsafe<T>((*rslv)(id), dflt);
            case EMessageLayout::MESSAGE_LAYOUT_SPARSE:
                return AsSparseMessage()->ReadValue<T>(id, dflt);
            case EMessageLayout::MESSAGE_LAYOUT_FLAT:
            case EMessageLayout::MESSAGE_LAYOUT_DYNAMIC:
                return ReadValueDispatch(id, dflt, rslv);
            default:
                YAFF_THROW("unknown layout");
        }
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(EMessageLayout tl, TFieldId id, const TFieldResolverFunc* rslv = nullptr) const {
        switch (tl) {
            case EMessageLayout::MESSAGE_LAYOUT_FIXED:
                return AsFixedMessage()->ReadLayoutUnsafe<T>((*rslv)(id), nullptr);
            case EMessageLayout::MESSAGE_LAYOUT_SPARSE:
                return AsSparseMessage()->ReadLayout<T>(id, nullptr);
            case EMessageLayout::MESSAGE_LAYOUT_FLAT:
            case EMessageLayout::MESSAGE_LAYOUT_DYNAMIC:
                return ReadLayoutDispatch<T>(id, rslv);
            default:
                YAFF_THROW("unknown layout");
        }
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(EMessageLayout tl, const TFieldId id, const TFieldResolverFunc* rslv = nullptr) const {
        switch (tl) {
            case EMessageLayout::MESSAGE_LAYOUT_FIXED:
                return AsFixedMessage()->ReadPresenceUnsafe<T>((*rslv)(id));
            case EMessageLayout::MESSAGE_LAYOUT_SPARSE:
                return AsSparseMessage()->ReadPresence<T>(id);
            case EMessageLayout::MESSAGE_LAYOUT_FLAT:
            case EMessageLayout::MESSAGE_LAYOUT_DYNAMIC:
                return ReadPresenceDispatch<T>(id, rslv);
            default:
                YAFF_THROW("unknown layout");
        }
    }

private:
    TAnyMessage() noexcept = default;

    template <typename T>
    YAFF_PURE T ReadValueDispatch(const TFieldId id, const T dflt, const TFieldResolverFunc* rslv = nullptr) const {
        const TFieldId typedLimit = NYaFF::ReadValue<TFieldId>(Message());
        if (TFlatMessage<void>::ToTypedLimit(id) < typedLimit) {
            return AsFlatMessage()->ReadValueUnsafe<T>((*rslv)(id), dflt);
        }
        return AsDynamicMessage()->ReadValueDispatch<T>(typedLimit, id, dflt);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutDispatch(TFieldId id, const TFieldResolverFunc* rslv = nullptr) const {
        const TFieldId typedLimit = NYaFF::ReadValue<TFieldId>(Message());
        if (TFlatMessage<void>::ToTypedLimit(id) < typedLimit) {
            return AsFlatMessage()->ReadLayoutUnsafe<T>((*rslv)(id), nullptr);
        }
        return AsDynamicMessage()->ReadLayoutDispatch<T>(typedLimit, id, nullptr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceDispatch(const TFieldId id, const TFieldResolverFunc* rslv = nullptr) const {
        const TFieldId typedLimit = NYaFF::ReadValue<TFieldId>(Message());
        if (TFlatMessage<void>::ToTypedLimit(id) < typedLimit) {
            return AsFlatMessage()->ReadPresenceUnsafe<T>(typedLimit, id, (*rslv)(id));
        }
        return AsDynamicMessage()->ReadPresenceDispatch<T>(typedLimit, id);
    }

    YAFF_PURE const std::byte* Message() const {
        return reinterpret_cast<const std::byte*>(this);
    }

    YAFF_PURE const TFixedMessage<void>* AsFixedMessage() const {
        return NYaFF::ReadLayout<TFixedMessage<void>>(Message());
    }

    YAFF_PURE const TFlatMessage<void>* AsFlatMessage() const {
        return NYaFF::ReadLayout<TFlatMessage<void>>(Message());
    }

    YAFF_PURE const TSparseMessage* AsSparseMessage() const {
        return NYaFF::ReadLayout<TSparseMessage>(Message());
    }

    YAFF_PURE const TDynamicMessage<void>* AsDynamicMessage() const {
        return NYaFF::ReadLayout<TDynamicMessage<void>>(Message());
    }
};
YAFF_LAYOUT_END

}  // namespace NYaFF::NReflect
