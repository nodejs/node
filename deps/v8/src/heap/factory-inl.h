// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_FACTORY_INL_H_
#define V8_HEAP_FACTORY_INL_H_

#include "src/heap/factory.h"

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!
#include "src/execution/isolate-inl.h"
#include "src/handles/handles-inl.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/string-inl.h"
#include "src/strings/string-hasher.h"

namespace v8 {
namespace internal {

#define ROOT_ACCESSOR(Type, name, CamelName)                                 \
  Handle<Type> Factory::name() {                                             \
    return Handle<Type>(&isolate()->roots_table()[RootIndex::k##CamelName]); \
  }
ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

Handle<String> Factory::InternalizeString(Handle<String> string) {
  if (string->IsInternalizedString()) return string;
  return StringTable::LookupString(isolate(), string);
}

Handle<Name> Factory::InternalizeName(Handle<Name> name) {
  if (name->IsUniqueName()) return name;
  return StringTable::LookupString(isolate(), Handle<String>::cast(name));
}

Handle<String> Factory::NewSubString(Handle<String> str, int begin, int end) {
  if (begin == 0 && end == str->length()) return str;
  return NewProperSubString(str, begin, end);
}

Handle<Object> Factory::NewNumberFromSize(size_t value) {
  // We can't use Smi::IsValid() here because that operates on a signed
  // intptr_t, and casting from size_t could create a bogus sign bit.
  if (value <= static_cast<size_t>(Smi::kMaxValue)) {
    return Handle<Object>(Smi::FromIntptr(static_cast<intptr_t>(value)),
                          isolate());
  }
  return NewHeapNumber(static_cast<double>(value));
}

Handle<Object> Factory::NewNumberFromInt64(int64_t value) {
  if (value <= std::numeric_limits<int32_t>::max() &&
      value >= std::numeric_limits<int32_t>::min() &&
      Smi::IsValid(static_cast<int32_t>(value))) {
    return Handle<Object>(Smi::FromInt(static_cast<int32_t>(value)), isolate());
  }
  return NewHeapNumber(static_cast<double>(value));
}

template <AllocationType allocation>
Handle<HeapNumber> Factory::NewHeapNumber(double value) {
  Handle<HeapNumber> heap_number = NewHeapNumber<allocation>();
  heap_number->set_value(value);
  return heap_number;
}

template <AllocationType allocation>
Handle<HeapNumber> Factory::NewHeapNumberFromBits(uint64_t bits) {
  Handle<HeapNumber> heap_number = NewHeapNumber<allocation>();
  heap_number->set_value_as_bits(bits);
  return heap_number;
}

Handle<HeapNumber> Factory::NewHeapNumberWithHoleNaN() {
  return NewHeapNumberFromBits(kHoleNanInt64);
}

Handle<JSArray> Factory::NewJSArrayWithElements(Handle<FixedArrayBase> elements,
                                                ElementsKind elements_kind,
                                                AllocationType allocation) {
  return NewJSArrayWithElements(elements, elements_kind, elements->length(),
                                allocation);
}

Handle<JSObject> Factory::NewFastOrSlowJSObjectFromMap(
    Handle<Map> map, int number_of_slow_properties, AllocationType allocation,
    Handle<AllocationSite> allocation_site) {
  return map->is_dictionary_map()
             ? NewSlowJSObjectFromMap(map, number_of_slow_properties,
                                      allocation, allocation_site)
             : NewJSObjectFromMap(map, allocation, allocation_site);
}

Handle<Object> Factory::NewURIError() {
  return NewError(isolate()->uri_error_function(),
                  MessageTemplate::kURIMalformed);
}

template <typename T>
inline MaybeHandle<T> Factory::Throw(Handle<Object> exception) {
  return isolate()->Throw<T>(exception);
}

ReadOnlyRoots Factory::read_only_roots() { return ReadOnlyRoots(isolate()); }

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_FACTORY_INL_H_
