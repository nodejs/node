// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_FACTORY_BASE_INL_H_
#define V8_HEAP_FACTORY_BASE_INL_H_

#include "src/heap/factory-base.h"
// Include the non-inl header before the rest of the headers.

#include <type_traits>

#include "src/execution/local-isolate-inl.h"
#include "src/numbers/conversions.h"
#include "src/objects/heap-number.h"
#include "src/objects/heap-object-field-inl.h"
#include "src/objects/map.h"
#include "src/objects/slots-inl.h"
#include "src/objects/smi.h"
#include "src/objects/struct-inl.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

#define RO_ROOT_ACCESSOR(Type, name, CamelName) \
  template <typename Impl>                      \
  Handle<Type> FactoryBase<Impl>::name() {      \
    return isolate()->roots_table().name();     \
  }
READ_ONLY_ROOT_LIST(RO_ROOT_ACCESSOR)
#ifndef V8_ENABLE_TDZ_HOLE
RO_ROOT_ACCESSOR(TdzHole, tdz_hole_value, TdzHoleValue)
#endif
#undef ROOT_ACCESSOR

#define MUTABLE_ROOT_ACCESSOR(Type, name, CamelName)     \
  template <typename Impl>                               \
  Handle<Type> FactoryBase<Impl>::name() {               \
    return handle(isolate()->heap()->name(), isolate()); \
  }
MUTABLE_ROOT_LIST(MUTABLE_ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

template <typename Impl>
Handle<Boolean> FactoryBase<Impl>::ToBoolean(bool value) {
  return value ? Cast<Boolean>(impl()->true_value())
               : Cast<Boolean>(impl()->false_value());
}

namespace detail {
template <typename T>
struct is_safe_allocation_arg : std::true_type {};
template <typename U>
struct is_safe_allocation_arg<Tagged<U>>
    : std::bool_constant<is_subtype_v<U, Smi> || is_read_only_v<U>> {};
template <typename U>
  requires(std::is_base_of_v<HeapObject, U>)
struct is_safe_allocation_arg<U*> : std::false_type {};

template <typename Arg>
decltype(auto) UnwrapAllocationArg(Arg&& arg) {
  using Decayed = std::decay_t<Arg>;
  if constexpr (is_direct_handle_v<Decayed> ||
                std::is_base_of_v<HandleBase, Decayed>) {
    return *arg;
  } else {
    static_assert(
        is_safe_allocation_arg<Decayed>::value,
        "Pass handles, not raw Tagged<HeapObject>, to Factory::New<T> "
        "since allocation can trigger GC.");
    return std::forward<Arg>(arg);
  }
}
}  // namespace detail

template <typename Impl>
template <typename T, typename... Args>
DirectHandle<T> FactoryBase<Impl>::New(AllocationType allocation,
                                       Args&&... args) {
  static_assert(
      !requires { &T::template OffsetOfDataStart<T>; },
      "Cannot use Factory::New<T> with variable-sized types "
      "(FLEXIBLE_ARRAY_MEMBER).");
  static_assert(sizeof(T) <= kMaxRegularHeapObjectSize);
  AllocationWitness witness = AllocateWithWitness(sizeof(T), allocation);
  T* result = new (witness)
      T(witness, detail::UnwrapAllocationArg(std::forward<Args>(args))...);
  return direct_handle(result, isolate());
}

template <typename Impl>
template <typename T, AllocationType kAllocation, typename... Args>
DirectHandle<T> FactoryBase<Impl>::New(Args&&... args) {
  return New<T>(kAllocation, std::forward<Args>(args)...);
}

template <typename Impl>
template <AllocationType allocation>
Handle<UninitializedHeapNumber>
FactoryBase<Impl>::NewUninitializedHeapNumber() {
  static_assert(sizeof(HeapNumber) == sizeof(UninitializedHeapNumber));
  static_assert(sizeof(HeapNumber) <= kMaxRegularHeapObjectSize);
  AllocationWitness witness = AllocateWithWitness(
      sizeof(UninitializedHeapNumber), allocation,
      USE_ALLOCATION_ALIGNMENT_HEAP_NUMBER_BOOL ? kDoubleUnaligned
                                                : kTaggedAligned);
  return handle(new (witness) UninitializedHeapNumber(witness), isolate());
}

template <typename Impl>
template <AllocationType allocation>
Handle<Number> FactoryBase<Impl>::NewNumber(double value) {
  // Materialize as a SMI if possible.
  int32_t int_value;
  if (DoubleToSmiInteger(value, &int_value)) {
    return handle(Smi::FromInt(int_value), isolate());
  }
  return NewHeapNumber<allocation>(value);
}

template <typename Impl>
template <AllocationType allocation>
Handle<Number> FactoryBase<Impl>::NewNumberFromInt(int32_t value) {
  if (Smi::IsValid(value)) return handle(Smi::FromInt(value), isolate());
  // Bypass NewNumber to avoid various redundant checks.
  return NewHeapNumber<allocation>(FastI2D(value));
}

template <typename Impl>
template <AllocationType allocation>
Handle<Number> FactoryBase<Impl>::NewNumberFromUint(uint32_t value) {
  int32_t int32v = static_cast<int32_t>(value);
  if (int32v >= 0 && Smi::IsValid(int32v)) {
    return handle(Smi::FromInt(int32v), isolate());
  }
  return NewHeapNumber<allocation>(FastUI2D(value));
}

template <typename Impl>
template <AllocationType allocation>
DirectHandle<Number> FactoryBase<Impl>::NewNumberFromSize(size_t value) {
  // We can't use Smi::IsValid() here because that operates on a signed
  // intptr_t, and casting from size_t could create a bogus sign bit.
  if (value <= static_cast<size_t>(Smi::kMaxValue)) {
    return direct_handle(Smi::FromIntptr(static_cast<intptr_t>(value)),
                         isolate());
  }
  return NewHeapNumber<allocation>(static_cast<double>(value));
}

template <typename Impl>
template <AllocationType allocation>
DirectHandle<Number> FactoryBase<Impl>::NewNumberFromInt64(int64_t value) {
  if (value <= std::numeric_limits<int32_t>::max() &&
      value >= std::numeric_limits<int32_t>::min() &&
      Smi::IsValid(static_cast<int32_t>(value))) {
    return direct_handle(Smi::FromInt(static_cast<int32_t>(value)), isolate());
  }
  return NewHeapNumber<allocation>(static_cast<double>(value));
}

template <typename Impl>
template <AllocationType allocation>
Handle<HeapNumber> FactoryBase<Impl>::NewHeapNumber(double value) {
  static_assert(sizeof(HeapNumber) <= kMaxRegularHeapObjectSize);
  AllocationWitness witness = AllocateWithWitness(
      sizeof(HeapNumber), allocation,
      USE_ALLOCATION_ALIGNMENT_HEAP_NUMBER_BOOL ? kDoubleUnaligned
                                                : kTaggedAligned);
  std::optional<SharedObjectConditionalSafePublishGuard> publish_guard;
  if constexpr (IsSharedAllocationType(allocation)) {
    publish_guard.emplace(witness.object(), allocation);
  }
  return handle(new (witness) HeapNumber(witness, value), isolate());
}

template <typename Impl>
template <AllocationType allocation>
Handle<HeapNumber> FactoryBase<Impl>::NewHeapNumberFromBits(uint64_t bits) {
  static_assert(sizeof(HeapNumber) <= kMaxRegularHeapObjectSize);
  AllocationWitness witness = AllocateWithWitness(
      sizeof(HeapNumber), allocation,
      USE_ALLOCATION_ALIGNMENT_HEAP_NUMBER_BOOL ? kDoubleUnaligned
                                                : kTaggedAligned);
  return handle(new (witness) HeapNumber(witness, Float64::FromBits(bits)),
                isolate());
}

template <typename Impl>
template <AllocationType allocation>
Handle<HeapNumber> FactoryBase<Impl>::NewHeapInt32(int32_t value) {
  return NewHeapNumberFromBits<allocation>(
      (static_cast<uint64_t>(kHoleNanUpper32) << 32) |
      static_cast<uint32_t>(value));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_FACTORY_BASE_INL_H_
