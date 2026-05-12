#pragma once

#include <string_view>

#include "base.h"

namespace yaff {

template <typename T>
class ArrayIterator;

template <typename T>
class StructArrayIterator;

template <typename T>
YAFF_LAYOUT_BEGIN(BaseArray) {
public:
    using size_type = uint32_t;
    using difference_type = std::ptrdiff_t;

public:
    BaseArray(const BaseArray&) = delete;
    BaseArray& operator=(const BaseArray&) = delete;

    /*
        This section only works correctly with byte vector specification.
        Formally, alignment and strict aliasing rules are not followed for other types,
        so accessing something through Data() is undefined behavior.

        However, due to the packed and may_alias features (check YAFF_LAYOUT_BEGIN),
        everything works correctly in known cases.
    */

    YAFF_PURE const auto* Data() const noexcept {
        return reinterpret_cast<const typename T::stored_type*>(Bytes());
    }

    YAFF_PURE const auto* data() const noexcept {
        return Data();
    }

    /*
        End of section of byte vector functions;
    */

    YAFF_PURE size_type Size() const noexcept {
        return Size_;
    }

    YAFF_PURE size_type size() const noexcept {
        return Size();
    }

    YAFF_PURE bool Empty() const noexcept {
        return Size_ == 0;
    }

    YAFF_PURE bool empty() const noexcept {
        return Empty();
    }

    auto begin() const noexcept {
        return typename T::const_iterator(*static_cast<const T*>(this), 0);
    }

    auto end() const noexcept {
        return typename T::const_iterator(*static_cast<const T*>(this), Size_);
    }

    /*
        This section of functions only works for a sorted vector, otherwise the behavior is undefined.
    */

    template <typename K>
    auto find(const K& key) const {
        const auto b = begin(), e = end();
        const auto f = std::lower_bound(b, e, key, [](const typename T::value_type& e, const K& k) { return e < k; });
        return (f != e && !(key < *f)) ? f : e;
    }

    template <typename K>
    auto equal_range(const K& key) const {
        const auto b = begin(), e = end();
        const auto lowercomp = [](const typename T::value_type& e, const K& k) { return e < k; };
        const auto uppercomp = [](const K& k, const typename T::value_type& e) { return k < e; };
        return std::make_pair(std::lower_bound(b, e, key, lowercomp), std::upper_bound(b, e, key, uppercomp));
    }

    template <typename K>
    size_t count(const K& key) const {
        return find(key) != end();
    }

    template <typename K>
    bool contains(const K& key) const {
        return find(key) != end();
    }

    /*
        End of section of sorted vector functions;
    */

    static const T& Default() noexcept {
        static constexpr std::byte DEFAULT_VECTOR[sizeof(T) + 1] = {};
        return *ReadLayout<T>(DEFAULT_VECTOR);
    }

protected:
    BaseArray() noexcept = default;

    YAFF_PURE const std::byte* Bytes() const noexcept {
        return reinterpret_cast<const std::byte*>(this + 1);
    }

private:
    size_type Size_;
};
YAFF_LAYOUT_END

template <typename T>
YAFF_LAYOUT_BEGIN(Array) : public BaseArray<Array<T>> {
    using Base = BaseArray<Array<T>>;

public:
    using Base::Base;

    using value_type = T;
    using stored_type = T;
    using const_reference = T;
    using const_pointer = void;
    using const_iterator = ArrayIterator<Array<T>>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    YAFF_PURE const_reference Get(Base::size_type i) const noexcept {
        return ReadValue<T>(Base::Bytes() + i * sizeof(T));
    }

    YAFF_PURE const_reference operator[](Base::size_type i) const noexcept {
        return Get(i);
    }
};
YAFF_LAYOUT_END

template <typename T>
YAFF_LAYOUT_BEGIN(Array<InternalOffset<T>>) : public BaseArray<Array<InternalOffset<T>>> {
    using Base = BaseArray<Array<InternalOffset<T>>>;

public:
    using Base::Base;

    using value_type = T;
    using stored_type = InternalOffset<T>;
    using const_reference = const value_type&;
    using const_pointer = const value_type*;
    using const_iterator = StructArrayIterator<Array<InternalOffset<T>>>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    YAFF_PURE const_reference Get(Base::size_type i) const noexcept {
        const auto offset = ReadValue<InternalOffset<T>>(Base::Bytes() + i * sizeof(InternalOffset<T>));
        return *ResolveOffset<T>(Base::Bytes(), offset.O);
    }

    YAFF_PURE const_reference operator[](Base::size_type i) const noexcept {
        return Get(i);
    }
};
YAFF_LAYOUT_END

template <typename T>
YAFF_LAYOUT_BEGIN(Array<InlineOffset<T>>) : public BaseArray<Array<InlineOffset<T>>> {
    using Base = BaseArray<Array<InlineOffset<T>>>;

public:
    using Base::Base;

    using value_type = T;
    using stored_type = InlineOffset<T>;
    using const_reference = const value_type&;
    using const_pointer = const value_type*;
    using const_iterator = StructArrayIterator<Array<InlineOffset<T>>>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    YAFF_PURE const_reference Get(Base::size_type i) const noexcept {
        return *ResolveOffset<T>(Base::Bytes(), T::MetaType::LIMIT * i);
    }

    YAFF_PURE const_reference operator[](Base::size_type i) const noexcept {
        return Get(i);
    }
};
YAFF_LAYOUT_END

YAFF_LAYOUT_BEGIN(String) : public BaseArray<String> {
    using Base = BaseArray<String>;

public:
    using Base::Base;

    using value_type = char;
    using stored_type = char;
    using const_reference = const value_type&;
    using const_pointer = const value_type*;
    using const_iterator = ArrayIterator<String>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    YAFF_PURE const_reference Get(Base::size_type i) const noexcept {
        return Base::Data()[i];
    }

    YAFF_PURE const_reference operator[](Base::size_type i) const noexcept {
        return Get(i);
    }

    std::string_view AsStringView() const noexcept {
        return {Base::Data(), Base::Size()};
    }

    operator std::string_view() const noexcept {
        return AsStringView();
    }

    template <std::convertible_to<std::string_view> K>
    friend std::strong_ordering operator<=>(const String& lhs, const K& rhs) noexcept {
        return lhs.AsStringView() <=> static_cast<std::string_view>(rhs);
    }

    template <std::convertible_to<std::string_view> K>
    friend bool operator==(const String& lhs, const K& rhs) noexcept {
        return lhs.AsStringView() == static_cast<std::string_view>(rhs);
    }

    friend std::ostream& operator<<(std::ostream& os, const String& str) {
        return os << static_cast<std::string_view>(str);
    }
};
YAFF_LAYOUT_END

template <typename T, typename D>
class BaseArrayIterator {
public:
    // N.B.: Formally, before C++20, this class can not be a real random_access_iterator,
    // since reference is just T for scalar types. But due to the constancy of the iterator,
    // this works correctly in known cases.
    //
    // For C++20+, there is a proper iterator_concept;
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;

    using difference_type = std::ptrdiff_t;
    using value_type = typename T::value_type;
    using reference = typename T::const_reference;
    using pointer = typename T::const_pointer;

    BaseArrayIterator() noexcept : Vec_(&T::Default()), Cur_(0) {
    }

    BaseArrayIterator(const T& vec, T::size_type cur = 0) noexcept : Vec_(&vec), Cur_(cur) {
    }

    BaseArrayIterator(const BaseArrayIterator&) noexcept = default;
    BaseArrayIterator& operator=(const BaseArrayIterator&) noexcept = default;

    friend bool operator==(const D& lhs, const D& rhs) noexcept {
        return lhs.Cur_ == rhs.Cur_;
    }

    friend std::strong_ordering operator<=>(const D& lhs, const D& rhs) noexcept {
        return lhs.Cur_ <=> rhs.Cur_;
    }

    friend difference_type operator-(const D& lhs, const D& rhs) noexcept {
        return static_cast<difference_type>(lhs.Cur_) - static_cast<difference_type>(rhs.Cur_);
    }

    D& operator++() noexcept {
        ++Cur_;
        return AsDerived();
    }

    D operator++(int) noexcept {
        D tmp(AsDerived());
        ++Cur_;
        return tmp;
    }

    D& operator+=(difference_type n) noexcept {
        Cur_ = static_cast<T::size_type>(Cur_ + n);
        return AsDerived();
    }

    D operator+(difference_type n) const noexcept {
        D tmp(AsDerived());
        tmp += n;
        return tmp;
    }

    friend D operator+(difference_type n, const D& it) noexcept {
        return it + n;
    }

    D& operator--() noexcept {
        --Cur_;
        return AsDerived();
    }

    D operator--(int) noexcept {
        D tmp(AsDerived());
        --Cur_;
        return tmp;
    }

    D operator-(difference_type n) const noexcept {
        return AsDerived() + (-n);
    }

    D& operator-=(difference_type n) noexcept {
        return AsDerived() += (-n);
    }

    YAFF_PURE reference operator*() const noexcept {
        return Vec_->Get(Cur_);
    }

    YAFF_PURE reference operator[](difference_type n) const noexcept {
        return Vec_->Get(static_cast<T::size_type>(Cur_ + n));
    }

private:
    D& AsDerived() noexcept {
        return *static_cast<D*>(this);
    }

    const D& AsDerived() const noexcept {
        return *static_cast<const D*>(this);
    }

    const T* Vec_;
    T::size_type Cur_;
};

template <typename T>
class ArrayIterator : public BaseArrayIterator<T, ArrayIterator<T>> {
    using Base = BaseArrayIterator<T, ArrayIterator<T>>;

public:
    using Base::Base;
};

template <typename T>
class StructArrayIterator : public BaseArrayIterator<T, StructArrayIterator<T>> {
    using Base = BaseArrayIterator<T, StructArrayIterator<T>>;

public:
    using Base::Base;

    YAFF_PURE Base::pointer operator->() const noexcept {
        return &Base::operator*();
    }
};

}  // namespace yaff
