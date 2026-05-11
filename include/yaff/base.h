#pragma once

#include <assert.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <functional>
#include <type_traits>
#include <vector>

#define YAFF_PRIVATE_STRINGIFY(X) #X
#define YAFF_STRINGIFY(X) YAFF_PRIVATE_STRINGIFY(X)

#if defined(__clang__) || defined(__GNUC__)
#define YAFF_PURE [[gnu::pure]]
#else
#define YAFF_PURE
#endif

// TODO: support standard likely, unlikely;
#if defined(__clang__) || defined(__GNUC__)
#define YAFF_LIKELY(Cond) __builtin_expect(!!(Cond), 1)
#define YAFF_UNLIKELY(Cond) __builtin_expect(!!(Cond), 0)
#else
#define YAFF_LIKELY(Cond) (Cond)
#define YAFF_UNLIKELY(Cond) (Cond)
#endif

// TODO: support different strategies for unaligned memory access, e.g. packed structs;
#if defined(__clang__) || defined(__GNUC__)
#define YAFF_MEMCPY __builtin_memcpy
#else
#define YAFF_MEMCPY std::memcpy
#endif

#if defined(__clang__) || defined(__GNUC__)
#define YAFF_BSWAP16(x) __builtin_bswap16(static_cast<uint16_t>(x))
#elif defined(_MSC_VER)
#define YAFF_BSWAP16(x) _byteswap_ushort(static_cast<uint16_t>(x))
#else
#define YAFF_BSWAP16(x) ((static_cast<uint16_t>(x) << 8) | (static_cast<uint16_t>(x) >> 8))
#endif

// TODO: think about may_alias hacks for msvc;
#if defined(_MSC_VER)
#define YAFF_LAYOUT_BEGIN(Name) __pragma(pack(push, 1)) struct Name
#define YAFF_LAYOUT_END __pragma(pack(pop))
#else
#define YAFF_LAYOUT_BEGIN(Name) struct [[gnu::packed, gnu::may_alias]] Name
#define YAFF_LAYOUT_END
#endif

#define YAFF_ASSERT(Cond) assert(Cond)

#define YAFF_THROW(Msg)                                                                         \
    do {                                                                                        \
        static constexpr const char* msg = __FILE__ ":" YAFF_STRINGIFY(__LINE__) " : " Msg "'"; \
        throw std::runtime_error(msg);                                                          \
    } while (false)

#define YAFF_REQUIRE(Cond)                                                \
    do {                                                                  \
        if (YAFF_UNLIKELY(!(Cond))) {                                     \
            YAFF_THROW("Condition violated: '" YAFF_STRINGIFY(Cond) "'"); \
        }                                                                 \
    } while (false)

namespace NYaFF {

enum class EType : int32_t {
    TYPE_NONE = 0,
    TYPE_BOOL = 1,
    TYPE_INT32 = 2,
    TYPE_UINT32 = 3,
    TYPE_INT64 = 4,
    TYPE_UINT64 = 5,
    TYPE_FLOAT = 6,
    TYPE_DOUBLE = 7,
    TYPE_ENUM = 8,
    TYPE_STRING = 9,
    TYPE_VECTOR = 10,
    TYPE_MESSAGE = 11,
};

enum class EMessageLayout : int32_t {
    MESSAGE_LAYOUT_UNKNOWN = 0,
    MESSAGE_LAYOUT_FIXED = 1,
    MESSAGE_LAYOUT_FLAT = 2,
    MESSAGE_LAYOUT_SPARSE = 3,
    MESSAGE_LAYOUT_DYNAMIC = 4,
};

enum class EPresence : int32_t {
    PRESENCE_NONE = 0,
    PRESENCE_IMPLICIT = 1,
    PRESENCE_EXPLICIT = 2,
};

template <typename T>
inline constexpr T XorDef(T v, T d) noexcept {
    return v ^ d;
}

// Float specializations with type punning for proper xor;
template <>
inline constexpr float XorDef(float v, float d) noexcept {
    return std::bit_cast<float>(std::bit_cast<uint32_t>(v) ^ std::bit_cast<uint32_t>(d));
}

template <>
inline constexpr double XorDef(double v, double d) noexcept {
    return std::bit_cast<double>(std::bit_cast<uint64_t>(v) ^ std::bit_cast<uint64_t>(d));
}

template <typename T>
inline constexpr bool IsEqual(T a, T b) noexcept {
    return a == b;
}

// Float specializations with proper comparison;
template <>
inline bool IsEqual(float a, float b) noexcept {
    return std::fabs(a - b) <= (std::min(std::fabs(a), std::fabs(b)) * std::numeric_limits<float>::epsilon());
}

template <>
inline bool IsEqual(double a, double b) noexcept {
    return std::fabs(a - b) <= (std::min(std::fabs(a), std::fabs(b)) * std::numeric_limits<double>::epsilon());
}

using TOffset = uint32_t;
using TSignedOffset = int32_t;

inline TOffset ToCheckedOffset(uint64_t v) {
    YAFF_REQUIRE(v <= ((1ULL << 31ULL) - 1ULL));
    return static_cast<TOffset>(v);
}

inline TSignedOffset ToCheckedSignedOffset(int64_t v) {
    YAFF_REQUIRE(std::numeric_limits<int32_t>::min() <= v && v <= std::numeric_limits<int32_t>::max());
    return static_cast<TSignedOffset>(v);
}

// N.B.: You can work with the data obtained through this call only
// if T was declared using YAFF_LAYOUT_BEGIN, otherwise the behavior may be undefined.
//
// There is no explicit check here, as the type of T may be incomplete at the time of ReadLayout,
// but since this is an internal function, we are fine with the lack of verification.
template <typename T>
YAFF_PURE inline const T* ReadLayout(const void* ptr) noexcept {
    return reinterpret_cast<const T*>(ptr);
}

template <typename T>
YAFF_PURE inline T ReadValue(const void* ptr) noexcept {
    T value;
    YAFF_MEMCPY(&value, ptr, sizeof(T));
    return value;
}

template <typename T>
YAFF_PURE inline T ReadValue(const void* ptr, const void** next) noexcept {
    *next = reinterpret_cast<const std::byte*>(ptr) + sizeof(T);
    return ReadValue<T>(ptr);
}

template <typename T>
inline void WriteValue(void* ptr, T value) noexcept {
    YAFF_MEMCPY(ptr, &value, sizeof(T));
}

template <typename T, typename O = TOffset>
YAFF_PURE inline const T* ResolveOffset(const void* obj, const O offset) noexcept {
    return ReadLayout<T>(reinterpret_cast<const std::byte*>(obj) + offset);
}

template <typename T, typename O = TOffset>
YAFF_PURE inline const T* ResolveNullableOffset(const void* obj, const O offset, const T* defaultPtr) noexcept {
    if (YAFF_LIKELY(offset != 0)) {
        return ReadLayout<T>(reinterpret_cast<const std::byte*>(obj) + offset);
    }
    return defaultPtr;
}

using TFieldId = uint16_t;
using TFieldOffset = uint16_t;
using TFieldResolverFunc = std::function<TFieldOffset(const TFieldId)>;

template <typename T, typename U>
struct TWrittenOffset {
    using TTargetType = T;
    using TUnderlying = U;

    constexpr TWrittenOffset() noexcept : O(0) {
    }

    constexpr TWrittenOffset(U o) noexcept : O(o) {
    }

    constexpr bool IsNull() const noexcept {
        return !O;
    }

    U O;
};

template <typename T>
concept CWrittenOffset = std::is_base_of_v<TWrittenOffset<typename T::TTargetType, typename T::TUnderlying>, T>;

template <typename T = void>
struct TInlineOffset : public TWrittenOffset<T, TOffset> {
    using TWrittenOffset<T, TOffset>::TWrittenOffset;
};

template <typename T = void>
struct TInternalOffset : public TWrittenOffset<T, TOffset> {
    using TWrittenOffset<T, TOffset>::TWrittenOffset;

    // Any TInternalOffset can be interpreted as TInlineOffset in the higher composite structure,
    // since TBuilder is composite structure too.
    constexpr operator TInlineOffset<T>() const noexcept {
        return {TWrittenOffset<T, TOffset>::O};
    }
};

template <typename M>
inline std::vector<typename M::key_type> SortedKeys(const M& map) {
    std::vector<typename M::key_type> keys;
    keys.reserve(map.size());
    for (const auto& [k, v] : map) {
        keys.emplace_back(k);
    }
    std::stable_sort(keys.begin(), keys.end());
    return keys;
}

template <typename M, typename K>
inline auto FindKey(M& map, const K& key) {
    auto it = map.find(key);
    return (it != map.end() ? &it->second : nullptr);
}

template <typename T>
YAFF_PURE inline const T& ReadRoot(const void* buf) noexcept {
    if (YAFF_UNLIKELY(!buf)) {
        return T::Default();
    }
    return *ResolveOffset<T>(buf, ReadValue<TOffset>(buf));
}

template <typename T>
YAFF_PURE inline const T& ReadDirect(const void* buf) noexcept {
    if (YAFF_UNLIKELY(!buf)) {
        return T::Default();
    }
    return *ReadLayout<T>(buf);
}

}  // namespace NYaFF
