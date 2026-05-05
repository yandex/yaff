#pragma once

#include "base.h"

namespace NYaFF::NReflect {

YAFF_LAYOUT_BEGIN(TAnyVector) {
public:
    TAnyVector(const TAnyVector&) = delete;
    TAnyVector& operator=(const TAnyVector&) = delete;

    YAFF_PURE uint32_t Size() const noexcept {
        return Size_;
    }

    template <typename T>
    YAFF_PURE T GetValue(uint32_t i) const noexcept {
        return GetAt<T>(i);
    }

    template <typename T>
    YAFF_PURE const T* GetLayout(uint32_t i) const noexcept {
        return ResolveOffset<T>(Bytes(), GetAt<TOffset>(i));
    }

    template <typename T>
    YAFF_PURE const T* GetLayout(uint32_t i, size_t element) const noexcept {
        return ResolveOffset<T>(Bytes(), element * i);
    }

private:
    TAnyVector() noexcept = default;

    YAFF_PURE const std::byte* Bytes() const noexcept {
        return reinterpret_cast<const std::byte*>(this + 1);
    }

    template <typename T>
    YAFF_PURE T GetAt(uint32_t i) const noexcept {
        return ReadValue<T>(Bytes() + i * sizeof(T));
    }

    uint32_t Size_;
};
YAFF_LAYOUT_END

}  // namespace NYaFF::NReflect
