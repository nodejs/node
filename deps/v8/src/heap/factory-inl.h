// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_FACTORY_INL_H_
#define V8_HEAP_FACTORY_INL_H_

#include "src/heap/factory.h"

#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/string-hasher.h"

namespace v8 {
namespace internal {

#define ROOT_ACCESSOR(type, name, camel_name)                         \
  Handle<type> Factory::name() {                                      \
    return Handle<type>(bit_cast<type**>(                             \
        &isolate()->heap()->roots_[Heap::k##camel_name##RootIndex])); \
  }
ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define STRUCT_MAP_ACCESSOR(NAME, Name, name)                      \
  Handle<Map> Factory::name##_map() {                              \
    return Handle<Map>(bit_cast<Map**>(                            \
        &isolate()->heap()->roots_[Heap::k##Name##MapRootIndex])); \
  }
STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define DATA_HANDLER_MAP_ACCESSOR(NAME, Name, Size, name)                \
  Handle<Map> Factory::name##_map() {                                    \
    return Handle<Map>(bit_cast<Map**>(                                  \
        &isolate()->heap()->roots_[Heap::k##Name##Size##MapRootIndex])); \
  }
DATA_HANDLER_LIST(DATA_HANDLER_MAP_ACCESSOR)
#undef DATA_HANDLER_MAP_ACCESSOR

#define STRING_ACCESSOR(name, str)                              \
  Handle<String> Factory::name() {                              \
    return Handle<String>(bit_cast<String**>(                   \
        &isolate()->heap()->roots_[Heap::k##name##RootIndex])); \
  }
INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name)                                   \
  Handle<Symbol> Factory::name() {                              \
    return Handle<Symbol>(bit_cast<Symbol**>(                   \
        &isolate()->heap()->roots_[Heap::k##name##RootIndex])); \
  }
PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define SYMBOL_ACCESSOR(name, description)                      \
  Handle<Symbol> Factory::name() {                              \
    return Handle<Symbol>(bit_cast<Symbol**>(                   \
        &isolate()->heap()->roots_[Heap::k##name##RootIndex])); \
  }
PUBLIC_SYMBOL_LIST(SYMBOL_ACCESSOR)
WELL_KNOWN_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define ACCESSOR_INFO_ACCESSOR(accessor_name, AccessorName)        \
  Handle<AccessorInfo> Factory::accessor_name##_accessor() {       \
    return Handle<AccessorInfo>(bit_cast<AccessorInfo**>(          \
        &isolate()                                                 \
             ->heap()                                              \
             ->roots_[Heap::k##AccessorName##AccessorRootIndex])); \
  }
ACCESSOR_INFO_LIST(ACCESSOR_INFO_ACCESSOR)
#undef ACCESSOR_INFO_ACCESSOR

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

Handle<Object> Factory::NewNumberFromSize(size_t value,
                                          PretenureFlag pretenure) {
  // We can't use Smi::IsValid() here because that operates on a signed
  // intptr_t, and casting from size_t could create a bogus sign bit.
  if (value <= static_cast<size_t>(Smi::kMaxValue)) {
    return Handle<Object>(Smi::FromIntptr(static_cast<intptr_t>(value)),
                          isolate());
  }
  return NewNumber(static_cast<double>(value), pretenure);
}

Handle<Object> Factory::NewNumberFromInt64(int64_t value,
                                           PretenureFlag pretenure) {
  if (value <= std::numeric_limits<int32_t>::max() &&
      value >= std::numeric_limits<int32_t>::min() &&
      Smi::IsValid(static_cast<int32_t>(value))) {
    return Handle<Object>(Smi::FromInt(static_cast<int32_t>(value)), isolate());
  }
  return NewNumber(static_cast<double>(value), pretenure);
}

Handle<HeapNumber> Factory::NewHeapNumber(double value, MutableMode mode,
                                          PretenureFlag pretenure) {
  Handle<HeapNumber> heap_number = NewHeapNumber(mode, pretenure);
  heap_number->set_value(value);
  return heap_number;
}

Handle<HeapNumber> Factory::NewHeapNumberFromBits(uint64_t bits,
                                                  MutableMode mode,
                                                  PretenureFlag pretenure) {
  Handle<HeapNumber> heap_number = NewHeapNumber(mode, pretenure);
  heap_number->set_value_as_bits(bits);
  return heap_number;
}

Handle<HeapNumber> Factory::NewMutableHeapNumber(PretenureFlag pretenure) {
  return NewHeapNumberFromBits(kHoleNanInt64, MUTABLE, pretenure);
}

Handle<JSArray> Factory::NewJSArrayWithElements(Handle<FixedArrayBase> elements,
                                                ElementsKind elements_kind,
                                                PretenureFlag pretenure) {
  return NewJSArrayWithElements(elements, elements_kind, elements->length(),
                                pretenure);
}

Handle<Object> Factory::NewURIError() {
  return NewError(isolate()->uri_error_function(),
                  MessageTemplate::kURIMalformed);
}

Handle<String> Factory::Uint32ToString(uint32_t value) {
  Handle<String> result = NumberToString(NewNumberFromUint(value));

  if (result->length() <= String::kMaxArrayIndexSize) {
    uint32_t field = StringHasher::MakeArrayIndexHash(value, result->length());
    result->set_hash_field(field);
  }
  return result;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_FACTORY_INL_H_
