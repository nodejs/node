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
#include "src/heap/factory-base-inl.h"
#include "src/objects/feedback-cell.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/string-inl.h"
#include "src/objects/string-table-inl.h"
#include "src/strings/string-hasher.h"

namespace v8 {
namespace internal {

#define ROOT_ACCESSOR(Type, name, CamelName)                                 \
  Handle<Type> Factory::name() {                                             \
    return Handle<Type>(&isolate()->roots_table()[RootIndex::k##CamelName]); \
  }
MUTABLE_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

bool Factory::CodeBuilder::CompiledWithConcurrentBaseline() const {
  return v8_flags.concurrent_sparkplug && kind_ == CodeKind::BASELINE &&
         !local_isolate_->is_main_thread();
}

Handle<String> Factory::InternalizeString(Handle<String> string) {
  if (string->IsInternalizedString()) return string;
  return isolate()->string_table()->LookupString(isolate(), string);
}

Handle<Name> Factory::InternalizeName(Handle<Name> name) {
  if (name->IsUniqueName()) return name;
  return isolate()->string_table()->LookupString(isolate(),
                                                 Handle<String>::cast(name));
}

Handle<String> Factory::NewSubString(Handle<String> str, int begin, int end) {
  if (begin == 0 && end == str->length()) return str;
  return NewProperSubString(str, begin, end);
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

ReadOnlyRoots Factory::read_only_roots() const {
  return ReadOnlyRoots(isolate());
}

HeapAllocator* Factory::allocator() const {
  return isolate()->heap()->allocator();
}

Factory::CodeBuilder& Factory::CodeBuilder::set_interpreter_data(
    Handle<HeapObject> interpreter_data) {
  // This DCHECK requires this function to be in -inl.h.
  DCHECK(interpreter_data->IsInterpreterData() ||
         interpreter_data->IsBytecodeArray());
  interpreter_data_ = interpreter_data;
  return *this;
}

void Factory::NumberToStringCacheSet(Handle<Object> number, int hash,
                                     Handle<String> js_string) {
  if (!number_string_cache()->get(hash * 2).IsUndefined(isolate()) &&
      !v8_flags.optimize_for_size) {
    int full_size = isolate()->heap()->MaxNumberToStringCacheSize();
    if (number_string_cache()->length() != full_size) {
      Handle<FixedArray> new_cache =
          NewFixedArray(full_size, AllocationType::kOld);
      isolate()->heap()->set_number_string_cache(*new_cache);
      return;
    }
  }
  DisallowGarbageCollection no_gc;
  FixedArray cache = *number_string_cache();
  cache.set(hash * 2, *number);
  cache.set(hash * 2 + 1, *js_string);
}

Handle<Object> Factory::NumberToStringCacheGet(Object number, int hash) {
  DisallowGarbageCollection no_gc;
  FixedArray cache = *number_string_cache();
  Object key = cache.get(hash * 2);
  if (key == number || (key.IsHeapNumber() && number.IsHeapNumber() &&
                        key.Number() == number.Number())) {
    return Handle<String>(String::cast(cache.get(hash * 2 + 1)), isolate());
  }
  return undefined_value();
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_FACTORY_INL_H_
