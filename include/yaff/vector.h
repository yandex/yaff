#pragma once

#include <string_view>

#include "base.h"

namespace NYaFF {

template <typename T>
class TVectorIterator;

template <typename T>
class TStructVectorIterator;

template <typename T>
YAFF_LAYOUT_BEGIN(TBaseVector) {
public:
    using size_type = uint32_t;
    using difference_type = std::ptrdiff_t;

public:
    TBaseVector(const TBaseVector&) = delete;
    TBaseVector& operator=(const TBaseVector&) = delete;

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
    TBaseVector() noexcept = default;

    YAFF_PURE const std::byte* Bytes() const noexcept {
        return reinterpret_cast<const std::byte*>(this + 1);
    }

private:
    size_type Size_;
};
YAFF_LAYOUT_END

template <typename T>
YAFF_LAYOUT_BEGIN(TVector) : public TBaseVector<TVector<T>> {
    using TBase = TBaseVector<TVector<T>>;

public:
    using TBase::TBase;

    using value_type = T;
    using stored_type = T;
    using const_reference = T;
    using const_pointer = void;
    using const_iterator = TVectorIterator<TVector<T>>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    YAFF_PURE const_reference Get(TBase::size_type i) const noexcept {
        return ReadValue<T>(TBase::Bytes() + i * sizeof(T));
    }

    YAFF_PURE const_reference operator[](TBase::size_type i) const noexcept {
        return Get(i);
    }
};
YAFF_LAYOUT_END

template <typename T>
YAFF_LAYOUT_BEGIN(TVector<TInternalOffset<T>>) : public TBaseVector<TVector<TInternalOffset<T>>> {
    using TBase = TBaseVector<TVector<TInternalOffset<T>>>;

public:
    using TBase::TBase;

    using value_type = T;
    using stored_type = TInternalOffset<T>;
    using const_reference = const value_type&;
    using const_pointer = const value_type*;
    using const_iterator = TStructVectorIterator<TVector<TInternalOffset<T>>>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    YAFF_PURE const_reference Get(TBase::size_type i) const noexcept {
        const auto offset = ReadValue<TInternalOffset<T>>(TBase::Bytes() + i * sizeof(TInternalOffset<T>));
        return *ResolveOffset<T>(TBase::Bytes(), offset.O);
    }

    YAFF_PURE const_reference operator[](TBase::size_type i) const noexcept {
        return Get(i);
    }
};
YAFF_LAYOUT_END

template <typename T>
YAFF_LAYOUT_BEGIN(TVector<TInlineOffset<T>>) : public TBaseVector<TVector<TInlineOffset<T>>> {
    using TBase = TBaseVector<TVector<TInlineOffset<T>>>;

public:
    using TBase::TBase;

    using value_type = T;
    using stored_type = TInlineOffset<T>;
    using const_reference = const value_type&;
    using const_pointer = const value_type*;
    using const_iterator = TStructVectorIterator<TVector<TInlineOffset<T>>>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    YAFF_PURE const_reference Get(TBase::size_type i) const noexcept {
        return *ResolveOffset<T>(TBase::Bytes(), T::TMetaType::LIMIT * i);
    }

    YAFF_PURE const_reference operator[](TBase::size_type i) const noexcept {
        return Get(i);
    }
};
YAFF_LAYOUT_END

YAFF_LAYOUT_BEGIN(TString) : public TBaseVector<TString> {
    using TBase = TBaseVector<TString>;

public:
    using TBase::TBase;

    using value_type = char;
    using stored_type = char;
    using const_reference = const value_type&;
    using const_pointer = const value_type*;
    using const_iterator = TVectorIterator<TString>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

public:
    YAFF_PURE const_reference Get(TBase::size_type i) const noexcept {
        return TBase::Data()[i];
    }

    YAFF_PURE const_reference operator[](TBase::size_type i) const noexcept {
        return Get(i);
    }

    std::string_view AsStringView() const noexcept {
        return {TBase::Data(), TBase::Size()};
    }

    operator std::string_view() const noexcept {
        return AsStringView();
    }

    template <std::convertible_to<std::string_view> K>
    friend std::strong_ordering operator<=>(const TString& lhs, const K& rhs) noexcept {
        return lhs.AsStringView() <=> static_cast<std::string_view>(rhs);
    }

    template <std::convertible_to<std::string_view> K>
    friend bool operator==(const TString& lhs, const K& rhs) noexcept {
        return lhs.AsStringView() == static_cast<std::string_view>(rhs);
    }

    friend std::ostream& operator<<(std::ostream& os, const TString& str) {
        return os << static_cast<std::string_view>(str);
    }
};
YAFF_LAYOUT_END

template <typename T, typename D>
class TBaseVectorIterator {
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

    TBaseVectorIterator() noexcept : Vec_(&T::Default()), Cur_(0) {
    }

    TBaseVectorIterator(const T& vec, T::size_type cur = 0) noexcept : Vec_(&vec), Cur_(cur) {
    }

    TBaseVectorIterator(const TBaseVectorIterator&) noexcept = default;
    TBaseVectorIterator& operator=(const TBaseVectorIterator&) noexcept = default;

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
class TVectorIterator : public TBaseVectorIterator<T, TVectorIterator<T>> {
    using TBase = TBaseVectorIterator<T, TVectorIterator<T>>;

public:
    using TBase::TBase;
};

template <typename T>
class TStructVectorIterator : public TBaseVectorIterator<T, TStructVectorIterator<T>> {
    using TBase = TBaseVectorIterator<T, TStructVectorIterator<T>>;

public:
    using TBase::TBase;

    YAFF_PURE TBase::pointer operator->() const noexcept {
        return &TBase::operator*();
    }
};

}  // namespace NYaFF
