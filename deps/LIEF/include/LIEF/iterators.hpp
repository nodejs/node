/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 * Copyright 2017 - 2021, NVIDIA CORPORATION. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_ITERATORS_H
#define LIEF_ITERATORS_H
#include <cmath>
#include <cstddef>
#include <cassert>
#include <iterator>
#include <functional>
#include <algorithm>
#include <type_traits>
#include <vector>

namespace LIEF {

template<class T>
using decay_t = typename std::decay<T>::type;

template<class T>
using add_const_t = typename std::add_const<T>::type;

template<class T>
using remove_const_t = typename std::remove_const<T>::type;

template< class T >
using add_lvalue_reference_t = typename std::add_lvalue_reference<T>::type;


/// Iterator which returns reference on container's values
template<class T, typename U = typename decay_t<T>::value_type,
         class ITERATOR_T = typename decay_t<T>::iterator>
class ref_iterator {
  public:
  using iterator_category = std::bidirectional_iterator_tag;
  using value_type = decay_t<U>;
  using difference_type = ptrdiff_t;
  using pointer = typename std::remove_pointer<U>::type*;
  using reference = typename std::remove_pointer<U>::type&;

  using container_type = T;          // e.g. std::vector<Section*>&
  using DT_VAL         = U;          // e.g. Section*
  using DT             = decay_t<T>; // e.g. std::vector<Section>
  using ref_t          = typename ref_iterator::reference;
  using pointer_t      = typename ref_iterator::pointer;

  ref_iterator(T container) :
    container_{std::forward<T>(container)}
  {
    it_ = std::begin(container_);
  }

  ref_iterator(const ref_iterator& copy) :
    container_{copy.container_},
    it_{std::begin(container_)},
    distance_{copy.distance_}
  {
    std::advance(it_, distance_);
  }


  ref_iterator& operator=(ref_iterator other) {
    swap(other);
    return *this;
  }

  void swap(ref_iterator& other) noexcept {
    std::swap(const_cast<add_lvalue_reference_t<remove_const_t<DT>>>(container_),
              const_cast<add_lvalue_reference_t<remove_const_t<DT>>>(other.container_));
    std::swap(it_, other.it_);
    std::swap(distance_, other.distance_);
  }


  ref_iterator& operator++() {
    it_ = std::next(it_);
    distance_++;
    return *this;
  }

  ref_iterator operator++(int) {
    ref_iterator retval = *this;
    ++(*this);
    return retval;
  }

  ref_iterator& operator--() {
    if (it_ != std::begin(container_)) {
      it_ = std::prev(it_);
      distance_--;
    }
    return *this;
  }

  ref_iterator operator--(int) {
    ref_iterator retval = *this;
    --(*this);
    return retval;
  }


  ref_iterator& operator+=(const typename ref_iterator::difference_type& movement) {
    std::advance(it_, movement);
    distance_ += movement;
    return *this;
  }


  ref_iterator& operator-=(const typename ref_iterator::difference_type& movement) {
    return (*this) += -movement;
  }


  typename std::enable_if<!std::is_const<ref_t>::value, remove_const_t<ref_t>>::type
  operator[](size_t n) {
    return const_cast<remove_const_t<ref_t>>(static_cast<const ref_iterator*>(this)->operator[](n));
  }


  add_const_t<ref_t> operator[](size_t n) const {
    assert(n < size() && "integrity error: out of bound");

    auto* no_const_this = const_cast<ref_iterator*>(this);

    typename ref_iterator::difference_type saved_dist = std::distance(std::begin(no_const_this->container_), no_const_this->it_);
    no_const_this->it_ = std::begin(no_const_this->container_);
    std::advance(no_const_this->it_, n);

    auto&& v = const_cast<add_const_t<ref_t>>(no_const_this->operator*());

    no_const_this->it_ = std::begin(no_const_this->container_);
    std::advance(no_const_this->it_, saved_dist);

    return v;
  }

  ref_iterator operator+(typename ref_iterator::difference_type n) const {
    ref_iterator tmp = *this;
    return tmp += n;
  }


  ref_iterator operator-(typename ref_iterator::difference_type n) const {
    ref_iterator tmp = *this;
    return tmp -= n;
  }


  typename ref_iterator::difference_type operator-(const ref_iterator& rhs) const {
    return distance_ - rhs.distance_;
  }

  bool operator<(const ref_iterator& rhs) const {
    return (rhs - *this) > 0;
  }


  bool operator>(const ref_iterator& rhs) const {
    return rhs < *this;
  }


  bool operator>=(const ref_iterator& rhs) const {
    return !(*this < rhs);
  }


  bool operator<=(const ref_iterator& rhs) const {
    return !(*this > rhs);
  }

  ref_iterator begin() const {
    return container_;
  }

  ref_iterator cbegin() const {
    return begin();
  }

  ref_iterator end()  const {
    ref_iterator it = ref_iterator{container_};
    it.it_ = std::end(it.container_);
    it.distance_ = it.size();
    return it;
  }

  ref_iterator cend() const {
    return end();
  }

  bool operator==(const ref_iterator& other) const {
    return (size() == other.size() && distance_ == other.distance_);
  }

  bool operator!=(const ref_iterator& other) const {
    return !(*this == other);
  }

  size_t size() const {
    return container_.size();
  }

  bool empty() const {
    return container_.empty();
  }

  typename std::enable_if<!std::is_const<ref_t>::value, remove_const_t<ref_t>>::type
  operator*() {
    return const_cast<remove_const_t<ref_t>>(static_cast<const ref_iterator*>(this)->operator*());
  }

  template<typename V = DT_VAL>
  typename std::enable_if<std::is_pointer<V>::value, add_const_t<ref_t>>::type
  operator*() const {
    assert(*it_ && "integrity error: nullptr");
    return const_cast<add_const_t<ref_t>>(static_cast<ref_t>(**it_));
  }

  template<typename V = DT_VAL>
  typename std::enable_if<!std::is_pointer<V>::value, add_const_t<ref_t>>::type
  operator*() const {
    return const_cast<add_const_t<ref_t>>(*(it_));
  }


  typename std::enable_if<!std::is_const<pointer_t>::value, pointer_t>::type
  operator->() {
    return const_cast<remove_const_t<pointer_t>>(static_cast<const ref_iterator*>(this)->operator->());
  }

  add_const_t<pointer_t> operator->() const {
    return const_cast<add_const_t<pointer_t>>(&(operator*()));
  }

  protected:
  T container_;
  ITERATOR_T it_;
  typename ref_iterator::difference_type distance_{};
};


/// Iterator which return const ref on container's values
template<class T, typename U = typename decay_t<T>::value_type, class CT = typename std::add_const<T>::type>
using const_ref_iterator = ref_iterator<CT, U, typename decay_t<CT>::const_iterator>;


/// Iterator which return a ref on container's values given predicates
template<class T, typename U = typename decay_t<T>::value_type,
         class ITERATOR_T = typename decay_t<T>::iterator>
class filter_iterator {

  public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = decay_t<U>;
  using difference_type = ptrdiff_t;
  using pointer = typename std::remove_pointer<U>::type*;
  using reference = typename std::remove_pointer<U>::type&;

  using container_type = T;
  using DT_VAL         = U;
  using DT        = decay_t<T>;
  using ref_t     = typename filter_iterator::reference;
  using pointer_t = typename filter_iterator::pointer;
  using filter_t  = std::function<bool (const typename DT::value_type&)>;

  filter_iterator(T container, filter_t filter) :
    container_{std::forward<T>(container)},
    filters_{}
  {

    it_ = std::begin(container_);

    filters_.push_back(filter),
    it_ = std::begin(container_);

    if (it_ != std::end(container_)) {
      if (!std::all_of(std::begin(filters_), std::end(filters_), [this] (const filter_t& f) {return f(*it_);})) {
        next();
      }
    }
  }

  filter_iterator(T container, const std::vector<filter_t>& filters) :
    container_{std::forward<T>(container)},
    filters_{filters}
  {

    it_ = std::begin(container_);

    if (it_ != std::end(container_)) {
      if (!std::all_of(std::begin(filters_), std::end(filters_), [this] (const filter_t& f) {return f(*it_);})) {
        next();
      }
    }
  }

  filter_iterator(T container) :
    container_{std::forward<T>(container)},
    filters_{}
  {
    it_ = std::begin(container_);
  }

  filter_iterator(const filter_iterator& copy) :
    container_{copy.container_},
    it_{std::begin(container_)},
    filters_{copy.filters_},
    distance_{copy.distance_}
  {
    std::advance(it_, distance_);
  }

  filter_iterator& operator=(filter_iterator other) {
    swap(other);
    return *this;
  }

  void swap(filter_iterator& other) noexcept {
    std::swap(const_cast<remove_const_t<DT>&>(container_), const_cast<remove_const_t<DT>&>(other.container_));
    std::swap(it_,        other.it_);
    std::swap(filters_,   other.filters_);
    std::swap(size_c_,    other.size_c_);
    std::swap(distance_,  other.distance_);
  }


  filter_iterator& def(filter_t func) {
    filters_.push_back(func);
    size_c_ = 0;
    return *this;
  }

  filter_iterator& operator++() {
    next();
    return *this;
  }

  filter_iterator operator++(int) {
    filter_iterator retval = *this;
    ++(*this);
    return retval;
  }

  filter_iterator begin() const {
    return {container_, filters_};
  }

  filter_iterator cbegin() const {
    return begin();
  }

  filter_iterator end() const {
    // we don't need filter for the end iterator
    filter_iterator it_end{container_};

    it_end.it_       =  it_end.container_.end();
    it_end.distance_ = it_end.container_.size();

    return it_end;
  }

  filter_iterator cend() const {
    return end();
  }

  typename std::enable_if<!std::is_const<ref_t>::value, remove_const_t<ref_t>>::type
  operator*() {
    return const_cast<remove_const_t<ref_t>>(static_cast<const filter_iterator*>(this)->operator*());
  }

  template<typename V = DT_VAL>
  typename std::enable_if<std::is_pointer<V>::value, add_const_t<ref_t>>::type
  operator*() const {
    assert(*it_ && "integrity error: nullptr");
    return const_cast<add_const_t<ref_t>>(static_cast<ref_t>(**it_));
  }

  template<typename V = DT_VAL>
  typename std::enable_if<!std::is_pointer<V>::value, add_const_t<ref_t>>::type
  operator*() const {
    return const_cast<add_const_t<ref_t>>(*(it_));
  }


  typename std::enable_if<!std::is_const<ref_t>::value, remove_const_t<ref_t>>::type
  operator[](size_t n) {
    return const_cast<remove_const_t<ref_t>>(static_cast<const filter_iterator*>(this)->operator[](n));
  }

  add_const_t<ref_t> operator[](size_t n) const {
    assert(n < size() && "integrity error: out of bound");

    auto it = begin();
    std::advance(it, n);
    return const_cast<add_const_t<ref_t>>(*it);
  }


  typename std::enable_if<!std::is_const<pointer_t>::value, pointer_t>::type
  operator->() {
    return const_cast<remove_const_t<pointer_t>>(static_cast<const filter_iterator*>(this)->operator->());
  }

  add_const_t<pointer_t> operator->() const {
    return const_cast<add_const_t<pointer_t>>(&(operator*()));
  }

  size_t size() const {
    if (filters_.empty()) {
      return container_.size();
    }

    if (size_c_ > 0) {
      return size_c_;
    }
    filter_iterator it = begin();
    size_t size = 0;

    auto end_iter = std::end(it);
    for (; it != end_iter; ++it) ++size;
    size_c_ = size;
    return size_c_;
  }


  bool empty() const {
    return size() == 0;
  }


  bool operator==(const filter_iterator& other) const {
    return (container_.size() == other.container_.size() && distance_ == other.distance_);
  }

  bool operator!=(const filter_iterator& other) const {
    return !(*this == other);
  }

  protected:
  void next() {
    if (it_ == std::end(container_)) {
      distance_ = container_.size();
      return;
    }

    do {
      it_ = std::next(it_);
      distance_++;
    } while(it_ != std::end(container_) &&
            !std::all_of(std::begin(filters_), std::end(filters_),
                         [this] (const filter_t& f) { return f(*it_); }));

  }


  mutable size_t size_c_ = 0;
  T container_;
  ITERATOR_T it_;
  std::vector<filter_t> filters_;
  typename filter_iterator::difference_type distance_ = 0;
};

/// Iterator which return a const ref on container's values given predicates
template<class T, typename U = typename decay_t<T>::value_type,
         class CT = typename std::add_const<T>::type>
using const_filter_iterator = filter_iterator<CT, U, typename decay_t<CT>::const_iterator>;

// ----------------------------------------------------------------------------
// This part is taken from LLVM
// ----------------------------------------------------------------------------

template <typename IteratorT>
class iterator_range {
  public:
  using IteratorTy = IteratorT;
  iterator_range(IteratorT&& it_begin, IteratorT&& it_end)
      : begin_(std::forward<IteratorT>(it_begin)),
        end_(std::forward<IteratorT>(it_end)) {}

  IteratorT begin() const { return begin_; }
  IteratorT end() const { return end_; }
  bool empty() const { return begin_ == end_; }

  typename IteratorT::value_type at(typename IteratorT::difference_type pos) const {
    static_assert(IsRandomAccess, "at() needs random access iterator");
    auto it = begin_;
    std::advance(it, pos);
    return *it;
  }

  typename IteratorT::value_type operator[](typename IteratorT::difference_type pos) const {
    return at(pos);
  }

  std::ptrdiff_t size() const {
    static_assert(IsRandomAccess, "size() needs random access iterator");
    return std::distance(begin_, end_);
  }

  protected:
  enum {
    IsRandomAccess = std::is_base_of<std::random_access_iterator_tag,
                                      typename IteratorT::iterator_category>::value,
    IsBidirectional = std::is_base_of<std::bidirectional_iterator_tag,
                                      typename IteratorT::iterator_category>::value,
  };
  private:
  IteratorT begin_;
  IteratorT end_;
};

template <class T> iterator_range<T> make_range(T&& x, T&& y) {
  return iterator_range<T>(std::forward<T>(x), std::forward<T>(y));
}


/// CRTP base class which implements the entire standard iterator facade
/// in terms of a minimal subset of the interface.
///
/// Use this when it is reasonable to implement most of the iterator
/// functionality in terms of a core subset. If you need special behavior or
/// there are performance implications for this, you may want to override the
/// relevant members instead.
///
/// Note, one abstraction that this does *not* provide is implementing
/// subtraction in terms of addition by negating the difference. Negation isn't
/// always information preserving, and I can see very reasonable iterator
/// designs where this doesn't work well. It doesn't really force much added
/// boilerplate anyways.
///
/// Another abstraction that this doesn't provide is implementing increment in
/// terms of addition of one. These aren't equivalent for all iterator
/// categories, and respecting that adds a lot of complexity for little gain.
///
/// Iterators are expected to have const rules analogous to pointers, with a
/// single, const-qualified operator*() that returns ReferenceT. This matches
/// the second and third pointers in the following example:
/// \code
///   int Value;
///   { int *I = &Value; }             // ReferenceT 'int&'
///   { int *const I = &Value; }       // ReferenceT 'int&'; const
///   { const int *I = &Value; }       // ReferenceT 'const int&'
///   { const int *const I = &Value; } // ReferenceT 'const int&'; const
/// \endcode
/// If an iterator facade returns a handle to its own state, then T (and
/// PointerT and ReferenceT) should usually be const-qualified. Otherwise, if
/// clients are expected to modify the handle itself, the field can be declared
/// mutable or use const_cast.
///
/// Classes wishing to use `iterator_facade_base` should implement the following
/// methods:
///
/// Forward Iterators:
///   (All of the following methods)
///   - DerivedT &operator=(const DerivedT &R);
///   - bool operator==(const DerivedT &R) const;
///   - T &operator*() const;
///   - DerivedT &operator++();
///
/// Bidirectional Iterators:
///   (All methods of forward iterators, plus the following)
///   - DerivedT &operator--();
///
/// Random-access Iterators:
///   (All methods of bidirectional iterators excluding the following)
///   - DerivedT &operator++();
///   - DerivedT &operator--();
///   (and plus the following)
///   - bool operator<(const DerivedT &RHS) const;
///   - DifferenceTypeT operator-(const DerivedT &R) const;
///   - DerivedT &operator+=(DifferenceTypeT N);
///   - DerivedT &operator-=(DifferenceTypeT N);
///
template <typename DerivedT, typename IteratorCategoryT, typename T,
          typename DifferenceTypeT = std::ptrdiff_t, typename PointerT = T *,
          typename ReferenceT = T &>
class iterator_facade_base {
public:
  using iterator_category = IteratorCategoryT;
  using value_type = T;
  using difference_type = DifferenceTypeT;
  using pointer = PointerT;
  using reference = ReferenceT;

protected:
  enum {
    IsRandomAccess = std::is_base_of<std::random_access_iterator_tag,
                                     IteratorCategoryT>::value,
    IsBidirectional = std::is_base_of<std::bidirectional_iterator_tag,
                                      IteratorCategoryT>::value,
  };

  /// A proxy object for computing a reference via indirecting a copy of an
  /// iterator. This is used in APIs which need to produce a reference via
  /// indirection but for which the iterator object might be a temporary. The
  /// proxy preserves the iterator internally and exposes the indirected
  /// reference via a conversion operator.
  class ReferenceProxy {
    friend iterator_facade_base;

    DerivedT I;

    ReferenceProxy(DerivedT I) : I(std::move(I)) {}

  public:
    operator ReferenceT() const { return *I; }
  };

  /// A proxy object for computing a pointer via indirecting a copy of a
  /// reference. This is used in APIs which need to produce a pointer but for
  /// which the reference might be a temporary. The proxy preserves the
  /// reference internally and exposes the pointer via a arrow operator.
  class PointerProxy {
    friend iterator_facade_base;

    ReferenceT R;

    template <typename RefT>
    PointerProxy(RefT &&R) : R(std::forward<RefT>(R)) {}

  public:
    PointerT operator->() const { return &R; }
  };

public:
  DerivedT operator+(DifferenceTypeT n) const {
    static_assert(std::is_base_of<iterator_facade_base, DerivedT>::value,
                  "Must pass the derived type to this template!");
    static_assert(
        IsRandomAccess,
        "The '+' operator is only defined for random access iterators.");
    DerivedT tmp = *static_cast<const DerivedT *>(this);
    tmp += n;
    return tmp;
  }
  friend DerivedT operator+(DifferenceTypeT n, const DerivedT &i) {
    static_assert(
        IsRandomAccess,
        "The '+' operator is only defined for random access iterators.");
    return i + n;
  }
  DerivedT operator-(DifferenceTypeT n) const {
    static_assert(
        IsRandomAccess,
        "The '-' operator is only defined for random access iterators.");
    DerivedT tmp = *static_cast<const DerivedT *>(this);
    tmp -= n;
    return tmp;
  }

  DerivedT &operator++() {
    static_assert(std::is_base_of<iterator_facade_base, DerivedT>::value,
                  "Must pass the derived type to this template!");
    return static_cast<DerivedT *>(this)->operator+=(1);
  }
  DerivedT operator++(int) {
    DerivedT tmp = *static_cast<DerivedT *>(this);
    ++*static_cast<DerivedT *>(this);
    return tmp;
  }
  DerivedT &operator--() {
    static_assert(
        IsBidirectional,
        "The decrement operator is only defined for bidirectional iterators.");
    return static_cast<DerivedT *>(this)->operator-=(1);
  }
  DerivedT operator--(int) {
    static_assert(
        IsBidirectional,
        "The decrement operator is only defined for bidirectional iterators.");
    DerivedT tmp = *static_cast<DerivedT *>(this);
    --*static_cast<DerivedT *>(this);
    return tmp;
  }

#ifndef __cpp_impl_three_way_comparison
  bool operator!=(const DerivedT &RHS) const {
    return !(static_cast<const DerivedT &>(*this) == RHS);
  }
#endif

  bool operator>(const DerivedT &RHS) const {
    static_assert(
        IsRandomAccess,
        "Relational operators are only defined for random access iterators.");
    return !(static_cast<const DerivedT &>(*this) < RHS) &&
           !(static_cast<const DerivedT &>(*this) == RHS);
  }
  bool operator<=(const DerivedT &RHS) const {
    static_assert(
        IsRandomAccess,
        "Relational operators are only defined for random access iterators.");
    return !(static_cast<const DerivedT &>(*this) > RHS);
  }
  bool operator>=(const DerivedT &RHS) const {
    static_assert(
        IsRandomAccess,
        "Relational operators are only defined for random access iterators.");
    return !(static_cast<const DerivedT &>(*this) < RHS);
  }

  PointerProxy operator->() const {
    return static_cast<const DerivedT *>(this)->operator*();
  }
  ReferenceProxy operator[](DifferenceTypeT n) const {
    static_assert(IsRandomAccess,
                  "Subscripting is only defined for random access iterators.");
    return static_cast<const DerivedT *>(this)->operator+(n);
  }
};

/// CRTP base class for adapting an iterator to a different type.
///
/// This class can be used through CRTP to adapt one iterator into another.
/// Typically this is done through providing in the derived class a custom \c
/// operator* implementation. Other methods can be overridden as well.
template <
    typename DerivedT, typename WrappedIteratorT,
    typename IteratorCategoryT =
        typename std::iterator_traits<WrappedIteratorT>::iterator_category,
    typename T = typename std::iterator_traits<WrappedIteratorT>::value_type,
    typename DifferenceTypeT =
        typename std::iterator_traits<WrappedIteratorT>::difference_type,
    typename PointerT = typename std::conditional<
        std::is_same<T, typename std::iterator_traits<
                            WrappedIteratorT>::value_type>::value,
        typename std::iterator_traits<WrappedIteratorT>::pointer, T *>,
    typename ReferenceT = typename std::conditional<
        std::is_same<T, typename std::iterator_traits<
                            WrappedIteratorT>::value_type>::value,
        typename std::iterator_traits<WrappedIteratorT>::reference, T &>::type>
class iterator_adaptor_base
    : public iterator_facade_base<DerivedT, IteratorCategoryT, T,
                                  DifferenceTypeT, PointerT, ReferenceT> {
  using BaseT = typename iterator_adaptor_base::iterator_facade_base;

protected:
  WrappedIteratorT I;

  iterator_adaptor_base() = default;

  explicit iterator_adaptor_base(WrappedIteratorT u) : I(std::move(u)) {
    static_assert(std::is_base_of<iterator_adaptor_base, DerivedT>::value,
                  "Must pass the derived type to this template!");
  }

  const WrappedIteratorT &wrapped() const { return I; }

public:
  using difference_type = DifferenceTypeT;

  DerivedT &operator+=(difference_type n) {
    static_assert(
        BaseT::IsRandomAccess,
        "The '+=' operator is only defined for random access iterators.");
    I += n;
    return *static_cast<DerivedT *>(this);
  }
  DerivedT &operator-=(difference_type n) {
    static_assert(
        BaseT::IsRandomAccess,
        "The '-=' operator is only defined for random access iterators.");
    I -= n;
    return *static_cast<DerivedT *>(this);
  }
  using BaseT::operator-;
  difference_type operator-(const DerivedT &RHS) const {
    static_assert(
        BaseT::IsRandomAccess,
        "The '-' operator is only defined for random access iterators.");
    return I - RHS.I;
  }

  // We have to explicitly provide ++ and -- rather than letting the facade
  // forward to += because WrappedIteratorT might not support +=.
  using BaseT::operator++;
  DerivedT &operator++() {
    ++I;
    return *static_cast<DerivedT *>(this);
  }
  using BaseT::operator--;
  DerivedT &operator--() {
    static_assert(
        BaseT::IsBidirectional,
        "The decrement operator is only defined for bidirectional iterators.");
    --I;
    return *static_cast<DerivedT *>(this);
  }

  friend bool operator==(const iterator_adaptor_base &LHS,
                         const iterator_adaptor_base &RHS) {
    return LHS.I == RHS.I;
  }
  friend bool operator<(const iterator_adaptor_base &LHS,
                        const iterator_adaptor_base &RHS) {
    static_assert(
        BaseT::IsRandomAccess,
        "Relational operators are only defined for random access iterators.");
    return LHS.I < RHS.I;
  }

  ReferenceT operator*() const { return *I; }
};

/// An iterator type that allows iterating over the pointees via some
/// other iterator.
///
/// The typical usage of this is to expose a type that iterates over Ts, but
/// which is implemented with some iterator over T*s:
///
/// \code
///   using iterator = pointee_iterator<SmallVectorImpl<T *>::iterator>;
/// \endcode
template <typename WrappedIteratorT,
          typename T = typename std::remove_reference<decltype(
              **std::declval<WrappedIteratorT>())>::type>
struct pointee_iterator
    : iterator_adaptor_base<
          pointee_iterator<WrappedIteratorT, T>, WrappedIteratorT,
          typename std::iterator_traits<WrappedIteratorT>::iterator_category,
          T> {
  pointee_iterator() = default;
  template <typename U>
  pointee_iterator(U &&u)
      : pointee_iterator::iterator_adaptor_base(std::forward<U &&>(u)) {}

  T &operator*() const { return **this->I; }
};

template <typename RangeT, typename WrappedIteratorT =
                               decltype(std::begin(std::declval<RangeT>()))>
iterator_range<pointee_iterator<WrappedIteratorT>>
make_pointee_range(RangeT &&Range) {
  using PointeeIteratorT = pointee_iterator<WrappedIteratorT>;
  return make_range(PointeeIteratorT(std::begin(std::forward<RangeT>(Range))),
                    PointeeIteratorT(std::end(std::forward<RangeT>(Range))));
}

template <typename WrappedIteratorT,
          typename T = decltype(&*std::declval<WrappedIteratorT>())>
class pointer_iterator
    : public iterator_adaptor_base<
          pointer_iterator<WrappedIteratorT, T>, WrappedIteratorT,
          typename std::iterator_traits<WrappedIteratorT>::iterator_category,
          T> {
  mutable T Ptr;

public:
  pointer_iterator() = default;

  explicit pointer_iterator(WrappedIteratorT u)
      : pointer_iterator::iterator_adaptor_base(std::move(u)) {}

  T &operator*() const { return Ptr = &*this->I; }
};

template <typename RangeT, typename WrappedIteratorT =
                               decltype(std::begin(std::declval<RangeT>()))>
iterator_range<pointer_iterator<WrappedIteratorT>>
make_pointer_range(RangeT &&Range) {
  using PointerIteratorT = pointer_iterator<WrappedIteratorT>;
  return make_range(PointerIteratorT(std::begin(std::forward<RangeT>(Range))),
                    PointerIteratorT(std::end(std::forward<RangeT>(Range))));
}

template <typename WrappedIteratorT,
          typename T1 = typename std::remove_reference<decltype(
              **std::declval<WrappedIteratorT>())>::type,
          typename T2 = typename std::add_pointer<T1>::type>
using raw_pointer_iterator =
    pointer_iterator<pointee_iterator<WrappedIteratorT, T1>, T2>;

}

#endif
