// Copyright 2026 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: any_span.h
// -----------------------------------------------------------------------------
//
// AnySpan provides a view of a random access container, much like absl::Span
// (go/totw/93). See also go/totw/145#gtlanyspan for an introduction of AnySpan.
//
// The primary differences from absl::Span are:
//  * AnySpan works with any random access container, whereas Span only works if
//    elements are contiguous in memory -- both will work with std::vector, but
//    only AnySpan will work with std::deque.
//  * AnySpan performs a variety of transformations, such as dereferencing
//    containers of pointers, or accessing specific members from a collection of
//    structs, whereas Span does not offer such capability. For example,
//    AnySpan<std::string> can handle both std::vector<std::string> and
//    std::vector<std::string*>. Safe implicit conversions for a container's
//    value type (such as up-casting from child classes, or converting
//    reference_wrapper<T> to const T&) will happen implicitly.
//  * AnySpan's generality has some small runtime cost, usually only a
//    conditional branch per element access, or a function-pointer call in the
//    worst case. Span may be preferable when the inputs are likely to be
//    contiguous and performance is critical.
//
// AnySpan<T> is a mutable view to the elements and AnySpan<const T> is a
// read-only view to the elements, similar to absl::Span.
//
// AnySpan only requires containers to provide a size() and an operator[] that
// returns a reference. It will use data() if it returns a pointer to the type
// returned by operator[], which allows it to perform some internal
// optimizations (this should apply to many well behaved random access
// containers that use arrays internally, but notably
// RepeatedPtrField<T>::data() returns T** instead of T*).
//
// Using AnySpan as an input parameter:
//
// To write a function that can accept vector<MyMessage>,
// vector<unique_ptr<MyMessage>>, or RepeatedPtrField<MyMessage> as inputs, you
// can use AnySpan as the input to the function. AnySpan should be passed by
// value and it is trivially copyable so it does not need to be moved:
//
//   void MyFunction(AnySpan<const MyMessage> messages);
//
// You can invoke MyFunction with a vector<MyMessage> or deque<MyMessage>:
//
//  std::vector<MyMessage> messages = ...;
//  MyFunction(messages);
//
// Or a container of smart pointers:
//
//  std::deque<std::unique_ptr<MyMessage>> message_ptrs = ...;
//  MyFunction(AnySpan<const MyMessage>(
//      message_ptrs, any_span_transform::Deref()));
//
// Or, you can call the same function with a repeated proto field of type
// MyMessage:
//
//  OtherMessage proto_message = ...;
//  MyFunction(proto_message.repeated_field());
//
//
// Using AnySpan as an output parameter:
//
// To write a function that allows mutation of a fixed-size container of
// objects, you can use AnySpan with a non-const value type.
//
//   void MyMutatingFunction(AnySpan<MyMessage> messages);
//
// To bind a mutable AnySpan to a container, callers must construct it
// explicitly around an lvalue:
//
//   std::vector<MyMessage> messages = ...;
//   MyMutatingFunction(AnySpan<MyMessages>(messages));
//
// Or use one of the "Make" functions:
//
//   std::vector<MyMessage*> message_ptrs = ...;
//   MyMutatingFunction(MakeDerefAnySpan(message_ptrs));
//
// Or, if you are already dealing with a mutable view-like object, construction
// can usually be implicit:
//
//   absl::Span<MyMessage> mutable_span = ...;
//   MyMutatingFunction(mutable_span);
//
// Transforming Spans:
//
// A set of useful transformation functors are provided (see the
// any_span_transform namespace), but you can provide your own transforms as
// well.
//
// Transforms work for both mutable and const values. When a transform is used
// for a mutable AnySpan, it will usually have to accept its argument as a
// mutable reference.
//
// Transforms can be any object supported by std::invoke, such as
// callable objects, function pointers, member function pointers, and even data
// members. Invoking a transform must return a reference to T or a reference to
// a compatible object such as a std::reference_wrapper or a child class.
// Transforms that return value types will not compile and would return
// dangling references if they did.
//
//  struct MyStruct {
//    int member;
//  }
//
//  std::vector<MyStruct> structs = ...;
//
//  // Create an AnySpan<const int> that accesses the members of 'structs':
//  auto mem_ptr = &MyStruct::member;
//  AnySpan<const int> members(structs, mem_ptr);
//
//  // Or, using a lambda:
//  auto get_member = [](const MyStruct& s) -> const int& {
//    return s.member;
//  };
//  AnySpan<const int> members_from_lambda(structs, get_member);
//
// Transforms must outlive the spans that use them (even member/method pointers,
// but not function pointer). Callable transforms must provide a const call
// operator that takes a single argument and returns a reference. Transforms
// will be executed every time an element is accessed, so complex transforms may
// have significant performance consequences.
//
// Factory Functions:
//
// A set of useful functions for constructing common types of AnySpans are
// provided. Factories with "Const" in the name produce AnySpans of const
// elements. Factories with "Deref" in the name will dereference elements of the
// container or array:
//
//  AnySpan<T> MakeAnySpan(Container& c);
//  AnySpan<T> MakeDerefAnySpan(Container& c);
//  AnySpan<T> MakeAnySpan(T* ptr, std::size_t size);
//  AnySpan<const T> MakeConstAnySpan(const Container& c);
//  AnySpan<const T> MakeConstDerefAnySpan(const Container& c);
//  AnySpan<const T> MakeConstAnySpan(const T* ptr, std::size_t size);
//
// Lifetime Gotchas:
//
// Take care when constructing spans as named variables! AnySpan captures all
// arguments by reference, even if it's a pointer:
//
//  AnySpan<T> span(v, &MyClass::SomeMethod);  // Dangling reference!
//
//  // Also bad! The lambda is destroyed before the span.
//  AnySpan<T> span(v, [](U& u) { return SomeFunction(u); });
//
// Free functions are ok:
//
//  AnySpan<T> span(v, SomeFunction);  // This is OK.
//  AnySpan<T> span(v, &SomeFunction);  // This is OK too.
//
// In all other cases, you must ensure that the object used as a transform
// outlives the span, even if that object is a pointer type.
//
// AnySpan is also capable of capturing another AnySpan, so watch out for
// implicit conversions between types of AnySpans:
//
//   // MakeDerefAnySpan() returns an AnySpan<Derived>, leaving 's' pointing to
//   // a temporary!
//   vector<Derived*> v;
//   AnySpan<Base> s = MakeDerefAnySpan(v);
//
// Adapting Spans:
//
// Since AnySpan only expects operator[] and size(), it is relatively simple to
// write light-weight adaptor classes that can behave like containers. See the
// any_span_adaptor namespace for a utility class that does this for iterators
// and views.
//
// Adapters are more powerful than transforms, since they allow you to change
// the value type and element order of a container, but transforms will
// generally perform better and leave code with fewer object lifetime concerns.
//
//
// Note about RepeatedPtrField performance:
//
// AnySpan will use data() when it returns a pointer to the same type returned
// by operator[], however RepeatedPtrField's operator[] returns T& and its
// data() returns a T**. Because of this, AnySpan will fall back to a less
// efficient version of type-erasure. If you have a performance critical use of
// RepeatedPtrField, you might find this pattern to have better performance:
//
//  MyFunction(AnySpan<const MyMessage>(
//      proto_message.repeated_field().data(),
//      proto_message.repeated_field().size(),
//      any_span_transform::Deref()));
//
#ifndef ABSL_TYPES_ANY_SPAN_H_
#define ABSL_TYPES_ANY_SPAN_H_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/internal/hardening.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/base/throw_delegate.h"
#include "absl/meta/type_traits.h"
#include "absl/types/internal/any_span.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// The accessors in the 'any_span_transform' namespace return references to
// Transform functors that may be passed to AnySpan. Generally you should
// prefer to use these functors whenever possible, as they may trigger internal
// optimizations that are otherwise not possible, and they are valid for the
// duration of the program, so you do not have to worry about their lifetime.
namespace any_span_transform {

//
// Identity() returns a functor that returns whatever is passed to it. Generally
// you should prefer to use AnySpan's implicit constructor directly, but this
// may be useful if you are writing templates on top of AnySpan.
//
// Returns a const reference so that callers don't have to worry about
// lifetime of the functor.
//

struct IdentityT {
  template <typename T>
  T& operator()(T& v) const {  // NOLINT(runtime/references)
    return v;
  }
};

inline const IdentityT& Identity() {
  static const IdentityT f = {};
  return f;
}

struct DerefT {
  template <typename Ptr>
  auto operator()(Ptr& ptr) const  // NOLINT(runtime/references)
      -> decltype(*ptr) {
    ABSL_RAW_DCHECK(ptr, "Cannot dereference null pointer");
    return *ptr;
  }
};

// Deref() returns a functor that dereferences whatever is passed to it. It
// works for smart and raw pointers, as well as std::optional. Do not use this
// with containers that may contain elements that cannot be dereferenced, such
// as null pointers.
//
// Returns a const reference so that callers don't have to worry about lifetime
// of the functor.
inline const DerefT& Deref() {
  static const DerefT f = {};
  return f;
}

}  // namespace any_span_transform

// Utilities for adapting things to look like the interface that AnySpan
// expects. For the most part this is based on iterators and views, and is
// intended to be composed with absl/types/iterator_adaptors.h.
namespace any_span_adaptor {

// Adapts a pair of iterators into a container-like object that AnySpan can
// wrap. This is useful if you are faced with a range or view of random access
// iterators. Iter must be a valid random access iterator.
template <typename Iter>
class Range {
 public:
  static_assert(
      std::is_same<typename std::iterator_traits<Iter>::iterator_category,
                   std::random_access_iterator_tag>::value,
      "Iter must be a random access iterator.");

  Range(Iter begin, Iter end) {
    absl::base_internal::HardeningAssertLE(begin, end);
    begin_ = begin;
    end_ = end;
  }

  std::size_t size() const { return end_ - begin_; }

  decltype(std::declval<Iter>()[0]) operator[](std::size_t i) const {
    absl::base_internal::HardeningAssertLT(i, size());
    return begin_[i];
  }

 private:
  Iter begin_;
  Iter end_;
};

// Returns a Range adaptor that wraps the given pair of iterators. The return
// value of this function must outlive any spans that use it. Iter must be a
// valid random access iterator.
template <typename Iter>
Range<Iter> MakeAdaptorFromRange(Iter begin, Iter end) {
  return Range<Iter>(begin, end);
}

// Returns a Range adaptor that wraps the given view. The begin() and end()
// functions of the given view must return valid random access iterators. The
// return value of this function must outlive any spans that use it.
template <typename View>
auto MakeAdaptorFromView(View& view)  // NOLINT(runtime/references)
    -> Range<decltype(view.begin())> {
  return Range<decltype(view.begin())>(view.begin(), view.end());
}

}  // namespace any_span_adaptor

template <typename T>
class AnySpan;

template <typename T>
class ABSL_ATTRIBUTE_VIEW AnySpan {
 private:
  template <typename Iter, typename Value>
  class IteratorBase;

  template <typename U>
  using EnableIfMutable = std::enable_if_t<!std::is_const<T>::value, U>;

  template <typename U>
  using EnableIfConst = std::enable_if_t<std::is_const<T>::value, U>;

  static std::true_type CreatesATemporaryImpl(std::decay_t<T>&&);
  static std::false_type CreatesATemporaryImpl(const T&);
  template <typename U,
            typename B = decltype(CreatesATemporaryImpl(std::declval<U>()))>
  struct CreatesATemporary : B {};

  // Enable if invoke(transform, element) is valid and if a reference to T can
  // bind to its output. This prevents situations where the constructor may be
  // ambiguous.
  // We also verify that the conversion from TransformResult to T& does not
  // create a temporary. Otherwise, we would get a false positive in the
  // enabler where `const char*` looks like can be converted to
  // `const std::string&`.
  template <typename Transform, typename Element,
            typename TransformResult = decltype(std::invoke(
                std::declval<const Transform&>(), std::declval<Element>()))>
  using EnableIfTransformIsValid =
      std::enable_if_t<std::is_convertible_v<TransformResult, T&> &&
                       !CreatesATemporary<TransformResult>::value>;

  // Enable if Container appears to be a valid container. Just checks for size()
  // and makes sure the class is not an AnySpan for now.
  template <typename Container>
  using EnableIfContainer =
      std::enable_if_t<any_span_internal::HasSize<Container>::value &&
                       !any_span_internal::IsAnySpan<Container>::value>;

  template <typename Element>
  using EnableIfDifferentElementType =
      std::enable_if_t<!std::is_same<T, Element>::value &&
                       !std::is_same<T, const Element>::value>;

  template <typename Transform>
  using EnableIfTransformIsByCopy =
      std::enable_if_t<any_span_internal::kIsTransformCopied<Transform>, bool>;
  template <typename Transform>
  using EnableIfTransformIsByRef =
      std::enable_if_t<!any_span_internal::kIsTransformCopied<Transform>, bool>;

 public:
  using element_type = T;
  using value_type = typename std::remove_const<T>::type;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using absl_internal_is_view = std::true_type;

  static const size_type npos = static_cast<size_type>(-1);  // NOLINT

  using reference = T&;
  using const_reference = typename std::add_const<T>::type&;

  using pointer = T*;
  using const_pointer = typename std::add_const<T>::type*;

  // Note that iterator will be const if T is const.
  class iterator;
  class const_iterator;

  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Null and empty by default.
  AnySpan() = default;

  // Creates a span that wraps an initializer list. This makes it possible to
  // pass a brace-enclosed initializer list to a function expecting an AnySpan.
  //
  // Example:
  //
  //   void Process(AnySpan<const int> x);
  //   Process({1, 2, 3});
  //
  // The initializer_list must outlive this AnySpan.
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      std::initializer_list<value_type> l ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : AnySpan(l.begin(), l.size()) {}

  // Creates a span that wraps an initializer list of a type other than
  // value_type, or with an explicit transform. Applies the optional transform
  // to elements before returning them.
  //
  // Example:
  //
  //   struct Base {};
  //   struct Derived : Base {};
  //
  //   void Process(AnySpan<const Base> x);
  //   Process({Derived(a), Derived(b), Derived(c)});
  //
  //     where the default identity transform would apply an implicit
  //     derived-to-base  conversion.
  //
  // The initializer_list must outlive this AnySpan.
  template <typename Element, typename Transform,
            typename = EnableIfTransformIsValid<Transform, const Element&>,
            EnableIfTransformIsByCopy<Transform> = true>
  constexpr AnySpan(std::initializer_list<Element> l
                        ABSL_ATTRIBUTE_LIFETIME_BOUND,
                    const Transform& transform)
      : AnySpan(l.begin(), l.size(), transform) {}
  template <typename Element,
            typename Transform = any_span_transform::IdentityT,
            typename = EnableIfTransformIsValid<Transform, const Element&>,
            EnableIfTransformIsByRef<Transform> = true>
  constexpr AnySpan(std::initializer_list<Element> l
                        ABSL_ATTRIBUTE_LIFETIME_BOUND,
                    const Transform& transform ABSL_ATTRIBUTE_LIFETIME_BOUND =
                        any_span_transform::Identity())
      : AnySpan(l.begin(), l.size(), transform) {}

  // Creates a span that wraps an array. Applies the optional transform to
  // elements before returning them.
  //
  // Transform must be a function object with a const operator() that takes
  // Element as an argument and return a reference to T or compatible object.
  //
  // Both the transform and array must outlive this span.
  template <typename Element, typename Transform,
            typename = EnableIfTransformIsValid<Transform, const Element&>,
            EnableIfTransformIsByCopy<Transform> = true>
  constexpr AnySpan(const Element* absl_nullable ptr
                        ABSL_ATTRIBUTE_LIFETIME_BOUND,
                    size_type size, const Transform& transform)
      : AnySpan(any_span_internal::MakeArrayGetter<T>(ptr, transform), size) {}
  template <typename Element,
            typename Transform = any_span_transform::IdentityT,
            typename = EnableIfTransformIsValid<Transform, const Element&>,
            EnableIfTransformIsByRef<Transform> = true>
  constexpr AnySpan(const Element* absl_nullable ptr
                        ABSL_ATTRIBUTE_LIFETIME_BOUND,
                    size_type size,
                    const Transform& transform ABSL_ATTRIBUTE_LIFETIME_BOUND =
                        any_span_transform::Identity())
      : AnySpan(any_span_internal::MakeArrayGetter<T>(ptr, transform), size) {}

  // Creates a span that wraps an array of fixed size. Applies the optional
  // transform to elements before returning them.
  //
  // Transform must be a function object with a const operator() that takes
  // Element as an argument and return a reference to T or compatible object.
  //
  // Both the transform and array must outlive this span.
  template <typename Element, size_type N, typename Transform,
            typename = EnableIfTransformIsValid<Transform, const Element&>,
            EnableIfTransformIsByCopy<Transform> = true>
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      const Element (&array ABSL_ATTRIBUTE_LIFETIME_BOUND)[N],
      const Transform& transform)
      : AnySpan(array, N, transform) {}
  template <typename Element, size_type N,
            typename Transform = any_span_transform::IdentityT,
            typename = EnableIfTransformIsValid<Transform, const Element&>,
            EnableIfTransformIsByRef<Transform> = true>
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      const Element (&array ABSL_ATTRIBUTE_LIFETIME_BOUND)[N],
      const Transform& transform ABSL_ATTRIBUTE_LIFETIME_BOUND =
          any_span_transform::Identity())
      : AnySpan(array, N, transform) {}

  // Creates a span that wraps a const container. Applies the optional transform
  // to elements before returning them.
  //
  // This constructor is enabled even for mutable spans, since some
  // container-like objects provide mutable element access even when the object
  // itself is const (such as absl::Span)
  //
  // Transform must be a function object with a const operator() that takes the
  // value type of Container as an argument and return a reference to T or
  // compatible object.
  //
  // The transform, container, and the container's underlying storage must
  // outlive this span. Any operation that may reallocate the container's
  // storage or change its size will invalidate the span.
  template <typename Container, typename Transform,
            typename = EnableIfContainer<Container>,
            typename = EnableIfTransformIsValid<
                Transform, decltype(std::declval<const Container&>()[0])>,
            EnableIfTransformIsByCopy<std::enable_if_t<
                absl::type_traits_internal::IsView<Container>::value,
                Transform>> = true>
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      const Container& container, const Transform& transform)
      : AnySpan(any_span_internal::MakeContainerGetter<T>(container, transform),
                container.size()) {}
  template <typename Container, typename Transform,
            typename = EnableIfContainer<Container>,
            typename = EnableIfTransformIsValid<
                Transform, decltype(std::declval<const Container&>()[0])>,
            EnableIfTransformIsByCopy<std::enable_if_t<
                !absl::type_traits_internal::IsView<Container>::value,
                Transform>> = true>
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      const Container& container ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const Transform& transform)
      : AnySpan(any_span_internal::MakeContainerGetter<T>(container, transform),
                container.size()) {}
  template <
      typename Container, typename Transform = any_span_transform::IdentityT,
      typename = EnableIfContainer<Container>,
      typename = EnableIfTransformIsValid<
          Transform, decltype(std::declval<const Container&>()[0])>,
      EnableIfTransformIsByRef<
          std::enable_if_t<absl::type_traits_internal::IsView<Container>::value,
                           Transform>> = true>
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      const Container& container,
      const Transform& transform ABSL_ATTRIBUTE_LIFETIME_BOUND =
          any_span_transform::Identity())
      : AnySpan(any_span_internal::MakeContainerGetter<T>(container, transform),
                container.size()) {}
  template <typename Container,
            typename Transform = any_span_transform::IdentityT,
            typename = EnableIfContainer<Container>,
            typename = EnableIfTransformIsValid<
                Transform, decltype(std::declval<const Container&>()[0])>,
            EnableIfTransformIsByRef<std::enable_if_t<
                !absl::type_traits_internal::IsView<Container>::value,
                Transform>> = true>
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      const Container& container ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const Transform& transform ABSL_ATTRIBUTE_LIFETIME_BOUND =
          any_span_transform::Identity())
      : AnySpan(any_span_internal::MakeContainerGetter<T>(container, transform),
                container.size()) {}

  // Creates a span that wraps a mutable array. Applies the optional transform
  // to elements before returning them.
  //
  // Transform must be a function object with a const operator() that takes
  // Element as an argument and return a reference to T or compatible object.
  //
  // Both the transform and array must outlive this span.
  template <typename Element, typename Transform,
            typename = EnableIfMutable<Element>,
            typename = EnableIfTransformIsValid<Transform, Element&>,
            EnableIfTransformIsByCopy<Transform> = true>
  constexpr AnySpan(Element* absl_nullable ptr ABSL_ATTRIBUTE_LIFETIME_BOUND,
                    size_type size, const Transform& transform)
      : AnySpan(any_span_internal::MakeArrayGetter<T>(ptr, transform), size) {}
  template <typename Element,
            typename Transform = any_span_transform::IdentityT,
            typename = EnableIfMutable<Element>,
            typename = EnableIfTransformIsValid<Transform, Element&>,
            EnableIfTransformIsByRef<Transform> = true>
  constexpr AnySpan(Element* absl_nullable ptr ABSL_ATTRIBUTE_LIFETIME_BOUND,
                    size_type size,
                    const Transform& transform ABSL_ATTRIBUTE_LIFETIME_BOUND =
                        any_span_transform::Identity())
      : AnySpan(any_span_internal::MakeArrayGetter<T>(ptr, transform), size) {}

  // Creates a span that wraps a mutable array of fixed size. Applies the
  // optional transform to elements before returning them.
  //
  // Transform must be a function object with a const operator() that takes
  // Element as an argument and return a reference to T or compatible object.
  //
  // Both the transform and array must outlive this span.
  template <typename Element, size_type N, typename Transform,
            typename = EnableIfMutable<Element>,
            typename = EnableIfTransformIsValid<Transform, Element&>,
            EnableIfTransformIsByCopy<Transform> = true>
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      Element (&array ABSL_ATTRIBUTE_LIFETIME_BOUND)[N],
      const Transform& transform)
      : AnySpan(array, N, transform) {}
  template <typename Element, size_type N,
            typename Transform = any_span_transform::IdentityT,
            typename = EnableIfMutable<Element>,
            typename = EnableIfTransformIsValid<Transform, Element&>,
            EnableIfTransformIsByRef<Transform> = true>
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      Element (&array ABSL_ATTRIBUTE_LIFETIME_BOUND)[N],
      const Transform& transform ABSL_ATTRIBUTE_LIFETIME_BOUND =
          any_span_transform::Identity())
      : AnySpan(array, N, transform) {}

  // Creates a span that wraps a mutable container. Only enabled if T is
  // mutable. Applies the optional transform to elements before returning them.
  //
  // Transform must be a function object with a const operator() that takes the
  // value type of Container as an argument and return a reference to T or
  // compatible object.
  //
  // The transform, container, and the container's underlying storage must
  // outlive this span. Any operation that may reallocate the container's
  // storage or change its size will invalidate the span.
  template <typename Container, typename Transform,
            typename = EnableIfMutable<Container>,
            typename = EnableIfContainer<Container>,
            typename = EnableIfTransformIsValid<
                Transform, decltype(std::declval<Container&>()[0])>,
            EnableIfTransformIsByCopy<Transform> = true>
  constexpr explicit AnySpan(  // NOLINT(google-explicit-constructor)
      Container& container ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const Transform& transform)
      : AnySpan(any_span_internal::MakeContainerGetter<T>(container, transform),
                container.size()) {}
  template <typename Container,
            typename Transform = any_span_transform::IdentityT,
            typename = EnableIfMutable<Container>,
            typename = EnableIfContainer<Container>,
            typename = EnableIfTransformIsValid<
                Transform, decltype(std::declval<Container&>()[0])>,
            EnableIfTransformIsByRef<Transform> = true>
  constexpr explicit AnySpan(  // NOLINT(google-explicit-constructor)
      Container& container ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const Transform& transform ABSL_ATTRIBUTE_LIFETIME_BOUND =
          any_span_transform::Identity())
      : AnySpan(any_span_internal::MakeContainerGetter<T>(container, transform),
                container.size()) {}

  // Converts a mutable span to a const span by copying the internal state
  // (rather than wrapping the other span).
  // TODO(b/179783710): add ABSL_ATTRIBUTE_LIFETIME_BOUND.
  template <typename LazyT = T, typename = EnableIfConst<LazyT>>
  constexpr AnySpan(  // NOLINT(google-explicit-constructor)
      const AnySpan<typename std::remove_const<T>::type>& other)
      : getter_(other.getter_), size_(other.size()) {}

  // Creates a span that wraps around another span of different type.
  //
  // This has performance and lifetime consequences, and can easily happen by
  // mistake. We make such conversions explicit here.
  template <typename Element, typename = EnableIfDifferentElementType<Element>,
            typename = EnableIfTransformIsValid<any_span_transform::IdentityT,
                                                Element&>>
  constexpr explicit AnySpan(
      const AnySpan<Element>& other ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : AnySpan(any_span_internal::MakeContainerGetter<T>(
                    other, any_span_transform::Identity()),
                other.size()) {}

  // Creates a span that wraps around another span. Applies the non-optional
  // transform to elements before returning them.
  //
  // This has lifetime consequences, and may happen by mistake. We make it
  // explicit here.
  template <typename Element, typename Transform,
            typename = EnableIfTransformIsValid<Transform, Element&>,
            EnableIfTransformIsByCopy<Transform> = true>
  constexpr explicit AnySpan(const AnySpan<Element>& other
                                 ABSL_ATTRIBUTE_LIFETIME_BOUND,
                             const Transform& transform)
      : AnySpan(any_span_internal::MakeContainerGetter<T>(other, transform),
                other.size()) {}
  template <typename Element, typename Transform,
            typename = EnableIfTransformIsValid<Transform, Element&>,
            EnableIfTransformIsByRef<Transform> = true>
  constexpr explicit AnySpan(
      const AnySpan<Element>& other ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const Transform& transform ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : AnySpan(any_span_internal::MakeContainerGetter<T>(other, transform),
                other.size()) {}

  // Returns a subspan of this span. This span may become invalid before the
  // subspan, but both the container and transform must remain valid.
  // pos must be non-negative and <= size().
  // len must be non-negative and <= size() - pos, or equal to npos.
  // If len == npos, the subspan continues till the end of this span.

  constexpr AnySpan subspan(size_type pos, size_type len) const {
    const size_t this_size = size();
    if (len == AnySpan<T>::npos) {
      len = this_size - pos;
    }
    absl::base_internal::HardeningAssertLE(pos, this_size);
    absl::base_internal::HardeningAssertLE(len,
                                           static_cast<size_type>(this_size
                                                                  - pos));
    return AnySpan<T>(getter_.Offset(pos), len);
  }

  constexpr AnySpan subspan(size_type pos) const {
    absl::base_internal::HardeningAssertLE(pos, size());
    return AnySpan(getter_.Offset(pos), size() - pos);
  }

  // Returns a `AnySpan` containing first `len` elements. Parameter `len`
  // must be non-negative and <= size().
  constexpr AnySpan first(size_type len) const {
    absl::base_internal::HardeningAssert(len != AnySpan<T>::npos);
    return subspan(0, len);
  }

  // Returns a `AnySpan` containing last `len` elements. Parameter `len` must be
  // non-negative and <= size().
  constexpr AnySpan last(size_type len) const { return subspan(size() - len); }

  // Size operations.
  constexpr size_type size() const { return size_; }
  constexpr bool empty() const { return size() == 0; }

  // Element access.
  constexpr reference operator[](size_type index) const {
    absl::base_internal::HardeningAssertLT(index, size());
    return getter_.Get(index);
  }
  constexpr reference at(size_type index) const {
    if (ABSL_PREDICT_FALSE(index >= size())) {
      absl::ThrowStdOutOfRange("AnySpan::at failed bounds check");
    }
    return getter_.Get(index);
  }
  constexpr reference front() const {
    absl::base_internal::HardeningAssertGT(size(), size_type{0});
    return (*this)[0];
  }
  constexpr reference back() const {
    absl::base_internal::HardeningAssertGT(size(), size_type{0});
    return (*this)[size() - 1];
  }

  // Iterator accessors.
  constexpr iterator begin() const { return iterator(this, 0); }
  constexpr iterator end() const { return iterator(this, size_); }
  constexpr reverse_iterator rbegin() const { return reverse_iterator(end()); }
  constexpr reverse_iterator rend() const { return reverse_iterator(begin()); }
  constexpr const_iterator cbegin() const { return const_iterator(this, 0); }
  constexpr const_iterator cend() const { return const_iterator(this, size_); }
  constexpr const_reverse_iterator crbegin() const { return rbegin(); }
  constexpr const_reverse_iterator crend() const { return rend(); }

  // Constructs from a getter and size. Not for external use.
  AnySpan(any_span_internal::Getter<T> getter, size_type size)
      : getter_(getter), size_(size) {}

  // Support for absl::Hash.
  template <typename H>
  friend constexpr H AbslHashValue(H state, AnySpan any_span) {
    for (const auto& v : any_span) {
      state = H::combine(std::move(state), v);
    }
    return H::combine(std::move(state), any_span.size());
  }

 private:
  template <typename U>
  friend class AnySpan;

  template <typename U>
  friend bool any_span_internal::IsCheap(AnySpan<U> s);

  // Getter to access elements.
  any_span_internal::Getter<T> getter_;

  // The size of this span.
  size_type size_ = 0;

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-template-friend"
#endif
  // The technical reasons we need to declare these friends in this manner are
  // quite subtle and confusing, but they're necessary on some toolchains to
  // allow all mutable/const combinations with this & other range types while
  // avoiding symbol collisions or ODR violations.
  friend bool operator==(AnySpan<const T> a, AnySpan<const T> b);
  friend bool operator!=(AnySpan<const T> a, AnySpan<const T> b);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

  // operator==
  friend bool operator==(AnySpan a, AnySpan b) {
    return any_span_internal::EqualImpl<const T>(a, b);
  }
  friend bool operator!=(AnySpan a, AnySpan b) { return !(a == b); }
};

// Constructs an AnySpan from a container or array.
template <int&... ExplicitArgumentBarrier, typename Container,
          typename T = any_span_internal::ElementType<Container>>
std::enable_if_t<
    absl::type_traits_internal::IsView<std::remove_cv_t<Container>>::value,
    AnySpan<T>>
MakeAnySpan(Container& c) {
  return AnySpan<T>(c);
}
template <int&... ExplicitArgumentBarrier, typename Container,
          typename T = any_span_internal::ElementType<Container>>
std::enable_if_t<
    !absl::type_traits_internal::IsView<std::remove_cv_t<Container>>::value,
    AnySpan<T>>
MakeAnySpan(Container& c ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return AnySpan<T>(c);
}

// Constructs an AnySpan that dereferences a container or array of pointers.
template <int&... ExplicitArgumentBarrier, typename Container,
          typename T = any_span_internal::DerefElementType<Container>>
std::enable_if_t<
    absl::type_traits_internal::IsView<std::remove_cv_t<Container>>::value,
    AnySpan<T>>
MakeDerefAnySpan(Container& c) {
  return AnySpan<T>(c, any_span_transform::Deref());
}
template <int&... ExplicitArgumentBarrier, typename Container,
          typename T = any_span_internal::DerefElementType<Container>>
std::enable_if_t<
    !absl::type_traits_internal::IsView<std::remove_cv_t<Container>>::value,
    AnySpan<T>>
MakeDerefAnySpan(Container& c ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return AnySpan<T>(c, any_span_transform::Deref());
}

// Constructs an AnySpan from a pointer and size.
template <int&... ExplicitArgumentBarrier, typename T>
AnySpan<T> MakeAnySpan(T* absl_nullable ptr ABSL_ATTRIBUTE_LIFETIME_BOUND,
                       std::size_t size) {
  return AnySpan<T>(ptr, size);
}

// Constructs a const AnySpan from a container or array.
template <int&... ExplicitArgumentBarrier, typename Container,
          typename T = any_span_internal::ElementType<const Container>>
std::enable_if_t<absl::type_traits_internal::IsView<Container>::value,
                 AnySpan<const T>>
MakeConstAnySpan(const Container& c) {
  return AnySpan<const T>(c);
}
template <int&... ExplicitArgumentBarrier, typename Container,
          typename T = any_span_internal::ElementType<const Container>>
std::enable_if_t<!absl::type_traits_internal::IsView<Container>::value,
                 AnySpan<const T>>
MakeConstAnySpan(const Container& c ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return AnySpan<const T>(c);
}

// Constructs a const AnySpan that dereferences a container or array of
// pointers.
template <int&... ExplicitArgumentBarrier, typename Container,
          typename T = any_span_internal::DerefElementType<const Container>>
std::enable_if_t<absl::type_traits_internal::IsView<Container>::value,
                 AnySpan<const T>>
MakeConstDerefAnySpan(const Container& c) {
  return AnySpan<const T>(c, any_span_transform::Deref());
}
template <int&... ExplicitArgumentBarrier, typename Container,
          typename T = any_span_internal::DerefElementType<const Container>>
std::enable_if_t<!absl::type_traits_internal::IsView<Container>::value,
                 AnySpan<const T>>
MakeConstDerefAnySpan(const Container& c ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return AnySpan<const T>(c, any_span_transform::Deref());
}

// Constructs an AnySpan from a pointer and size.
template <int&... ExplicitArgumentBarrier, typename T>
AnySpan<const T> MakeConstAnySpan(const T* absl_nullable ptr,
                                  std::size_t size) {
  return AnySpan<const T>(ptr, size);
}

//
// Implementation details follow.
//

template <typename T>
const typename AnySpan<T>::size_type AnySpan<T>::npos;

// Iterator base class. Uses CRTP (Iter should be the child class). Constness of
// the iterator is determined by the constness of Value.
template <typename T>
template <typename Iter, typename Value>
class ABSL_ATTRIBUTE_VIEW AnySpan<T>::IteratorBase {
 private:
  // Returns a reference to this as the child class.
  const Iter& self() const { return static_cast<const Iter&>(*this); }
  Iter& self() { return static_cast<Iter&>(*this); }

 public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = typename std::remove_const<Value>::type;
  using difference_type = std::ptrdiff_t;
  using reference = Value&;
  using pointer = Value*;

  // Constructs an invalid iterator.
  IteratorBase() = default;

  reference operator*() const { return (*container_)[index_]; }

  pointer absl_nonnull operator->() const { return &(*container_)[index_]; }

  reference operator[](difference_type i) const {
    return (*container_)[index_ + i];
  }

  Iter& operator+=(difference_type d) {
    index_ += d;
    return self();
  }

  Iter& operator-=(difference_type d) { return self() += -d; }

  Iter& operator++() {
    self() += 1;
    return self();
  }

  Iter operator++(int) {
    Iter copy(self());
    ++self();
    return copy;
  }

  Iter& operator--() {
    self() -= 1;
    return self();
  }

  Iter operator--(int) {
    Iter copy(self());
    --self();
    return copy;
  }

  Iter operator+(difference_type d) const {
    Iter tmp = self();
    tmp += d;
    return tmp;
  }

  friend Iter operator+(difference_type d, Iter i) { return i + d; }

  Iter operator-(difference_type d) const { return self() + (-d); }

  difference_type operator-(const Iter& other) const {
    return index_ - other.index_;
  }

  friend bool operator==(const Iter& a, const Iter& b) {
    return a.index_ == b.index_;
  }

  friend bool operator!=(const Iter& a, const Iter& b) {
    return a.index_ != b.index_;
  }

  friend bool operator<(const Iter& a, const Iter& b) {
    return a.index_ < b.index_;
  }

  friend bool operator<=(const Iter& a, const Iter& b) {
    return a.index_ <= b.index_;
  }

  friend bool operator>(const Iter& a, const Iter& b) {
    return a.index_ > b.index_;
  }

  friend bool operator>=(const Iter& a, const Iter& b) {
    return a.index_ >= b.index_;
  }

 protected:
  // Constructs an iterator that points to the given index of the given span.
  IteratorBase(const AnySpan* absl_nullable container, size_type index)
      : container_(container), index_(index) {}

  const AnySpan* absl_nullable container_ = nullptr;
  size_type index_ = 0;
};

// iterator implementation. This mostly just forwards to IteratorBase.
template <typename T>
class ABSL_ATTRIBUTE_VIEW AnySpan<T>::iterator
    : public IteratorBase<iterator, T> {
 private:
  using Base = IteratorBase<iterator, T>;

 public:
  using typename Base::difference_type;
  using typename Base::iterator_category;
  using typename Base::pointer;
  using typename Base::reference;
  using typename Base::value_type;

  iterator() = default;

 private:
  // Only let AnySpan construct valid instances.
  friend class AnySpan;

  iterator(const AnySpan* absl_nullable container, size_type index)
      : Base(container, index) {}
};

// const_iterator implementation. This mostly just forwards to IteratorBase,
// but also provides conversion from MutableIterator.
template <typename T>
class AnySpan<T>::const_iterator
    : public IteratorBase<const_iterator, typename std::add_const<T>::type> {
 private:
  using Base = IteratorBase<const_iterator, typename std::add_const<T>::type>;

 public:
  using typename Base::difference_type;
  using typename Base::iterator_category;
  using typename Base::pointer;
  using typename Base::reference;
  using typename Base::value_type;

  const_iterator() = default;

  // Support conversion from mutable iterators.
  // NOLINTNEXTLINE(google-explicit-constructor)
  const_iterator(const iterator& other)  // NOLINT(runtime/explicit)
      : Base(other.container_, other.index_) {}

 private:
  // Only let AnySpan construct valid instances.
  friend class AnySpan;

  const_iterator(const AnySpan* absl_nullable container, size_type index)
      : Base(container, index) {}
};

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TYPES_ANY_SPAN_H_
