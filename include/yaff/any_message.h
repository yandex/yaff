#pragma once

#include "base.h"
#include "message.h"

namespace yaff::reflect {

YAFF_LAYOUT_BEGIN(AnyMessage) {
public:
    AnyMessage(const AnyMessage&) = delete;
    AnyMessage& operator=(const AnyMessage&) = delete;

    template <typename T>
    YAFF_PURE T ReadValue(MessageLayout tl, FieldId id, T dflt, const FieldResolver* rslv = nullptr) const {
        switch (tl) {
            case MessageLayout::MESSAGE_LAYOUT_FIXED:
                return AsFixedMessage()->ReadValueUnsafe<T>((*rslv)(id, nullptr), dflt);
            case MessageLayout::MESSAGE_LAYOUT_SPARSE:
                return AsSparseMessage()->ReadValue<T>(id, dflt);
            case MessageLayout::MESSAGE_LAYOUT_FLAT:
            case MessageLayout::MESSAGE_LAYOUT_DYNAMIC:
                return ReadValueDispatch(id, dflt, rslv);
            default:
                YAFF_THROW("unknown layout");
        }
    }

    template <typename T>
    YAFF_PURE const T* ReadLayout(MessageLayout tl, FieldId id, const FieldResolver* rslv = nullptr) const {
        switch (tl) {
            case MessageLayout::MESSAGE_LAYOUT_FIXED:
                return AsFixedMessage()->ReadLayoutUnsafe<T>((*rslv)(id, nullptr), nullptr);
            case MessageLayout::MESSAGE_LAYOUT_SPARSE:
                return AsSparseMessage()->ReadLayout<T>(id, nullptr);
            case MessageLayout::MESSAGE_LAYOUT_FLAT:
            case MessageLayout::MESSAGE_LAYOUT_DYNAMIC:
                return ReadLayoutDispatch<T>(id, rslv);
            default:
                YAFF_THROW("unknown layout");
        }
    }

    template <typename T>
    YAFF_PURE bool ReadPresence(MessageLayout tl, const FieldId id, const FieldResolver* rslv = nullptr) const {
        switch (tl) {
            case MessageLayout::MESSAGE_LAYOUT_FIXED:
                return AsFixedMessage()->ReadPresenceUnsafe<T>((*rslv)(id, nullptr));
            case MessageLayout::MESSAGE_LAYOUT_SPARSE:
                return AsSparseMessage()->ReadPresence<T>(id);
            case MessageLayout::MESSAGE_LAYOUT_FLAT:
            case MessageLayout::MESSAGE_LAYOUT_DYNAMIC:
                return ReadPresenceDispatch<T>(id, rslv);
            default:
                YAFF_THROW("unknown layout");
        }
    }

private:
    AnyMessage() noexcept = default;

    YAFF_PURE CorrectionResolver MakeCorrectionResolver(const FieldId tl) const {
        return [tl, m = AsFlatMessage()](const FieldId id) {
            return (FlatMessage<void>::IsImplicit(tl) ? m->ResolveImplicitSize(id) : m->ResolveExplicitSize(id));
        };
    }

    template <typename T>
    YAFF_PURE T ReadValueDispatch(const FieldId id, const T dflt, const FieldResolver* rslv) const {
        const FieldId tl = yaff::ReadValue<FieldId>(Message());
        if (FlatMessage<void>::ToTypedLimit(id) < tl) {
            const CorrectionResolver corr = MakeCorrectionResolver(tl);
            return AsFlatMessage()->ReadValueUnsafe<T>((*rslv)(id, &corr), dflt);
        }
        return AsDynamicMessage()->ReadValueDispatch<T>(tl, id, dflt);
    }

    template <typename T>
    YAFF_PURE const T* ReadLayoutDispatch(const FieldId id, const FieldResolver* rslv) const {
        const FieldId tl = yaff::ReadValue<FieldId>(Message());
        if (FlatMessage<void>::ToTypedLimit(id) < tl) {
            const CorrectionResolver corr = MakeCorrectionResolver(tl);
            return AsFlatMessage()->ReadLayoutUnsafe<T>((*rslv)(id, &corr), nullptr);
        }
        return AsDynamicMessage()->ReadLayoutDispatch<T>(tl, id, nullptr);
    }

    template <typename T>
    YAFF_PURE bool ReadPresenceDispatch(const FieldId id, const FieldResolver* rslv) const {
        const FieldId tl = yaff::ReadValue<FieldId>(Message());
        if (FlatMessage<void>::ToTypedLimit(id) < tl) {
            const CorrectionResolver corr = MakeCorrectionResolver(tl);
            return AsFlatMessage()->ReadPresenceUnsafe<T>(tl, id, (*rslv)(id, &corr));
        }
        return AsDynamicMessage()->ReadPresenceDispatch<T>(tl, id);
    }

    YAFF_PURE const std::byte* Message() const {
        return reinterpret_cast<const std::byte*>(this);
    }

    YAFF_PURE const FixedMessage<void>* AsFixedMessage() const {
        return yaff::ReadLayout<FixedMessage<void>>(Message());
    }

    YAFF_PURE const FlatMessage<void>* AsFlatMessage() const {
        return yaff::ReadLayout<FlatMessage<void>>(Message());
    }

    YAFF_PURE const SparseMessage* AsSparseMessage() const {
        return yaff::ReadLayout<SparseMessage>(Message());
    }

    YAFF_PURE const DynamicMessage<void>* AsDynamicMessage() const {
        return yaff::ReadLayout<DynamicMessage<void>>(Message());
    }
};
YAFF_LAYOUT_END

}  // namespace yaff::reflect
