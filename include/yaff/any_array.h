#pragma once

#include "base.h"

namespace yaff::reflect {

YAFF_LAYOUT_BEGIN(AnyArray) {
public:
    AnyArray(const AnyArray&) = delete;
    AnyArray& operator=(const AnyArray&) = delete;

    YAFF_PURE uint32_t Size() const noexcept {
        return Size_;
    }

    template <typename T>
    YAFF_PURE T GetValue(uint32_t i) const noexcept {
        return GetAt<T>(i);
    }

    template <typename T>
    YAFF_PURE const T* GetLayout(uint32_t i) const noexcept {
        return ResolveOffset<T>(Bytes(), GetAt<Offset>(i));
    }

    template <typename T>
    YAFF_PURE const T* GetLayout(uint32_t i, size_t element) const noexcept {
        return ResolveOffset<T>(Bytes(), element * i);
    }

private:
    AnyArray() noexcept = default;

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

}  // namespace yaff::reflect
