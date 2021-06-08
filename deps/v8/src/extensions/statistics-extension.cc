// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/statistics-extension.h"

#include "src/common/assert-scope.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/logging/counters.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

const char* const StatisticsExtension::kSource =
    "native function getV8Statistics();";


v8::Local<v8::FunctionTemplate> StatisticsExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate, v8::Local<v8::String> str) {
  DCHECK_EQ(strcmp(*v8::String::Utf8Value(isolate, str), "getV8Statistics"), 0);
  return v8::FunctionTemplate::New(isolate, StatisticsExtension::GetCounters);
}


static void AddCounter(v8::Isolate* isolate,
                       v8::Local<v8::Object> object,
                       StatsCounter* counter,
                       const char* name) {
  if (counter->Enabled()) {
    object
        ->Set(isolate->GetCurrentContext(),
              v8::String::NewFromUtf8(isolate, name).ToLocalChecked(),
              v8::Number::New(isolate, *counter->GetInternalPointer()))
        .FromJust();
  }
}

static void AddNumber(v8::Isolate* isolate, v8::Local<v8::Object> object,
                      double value, const char* name) {
  object
      ->Set(isolate->GetCurrentContext(),
            v8::String::NewFromUtf8(isolate, name).ToLocalChecked(),
            v8::Number::New(isolate, value))
      .FromJust();
}


static void AddNumber64(v8::Isolate* isolate,
                        v8::Local<v8::Object> object,
                        int64_t value,
                        const char* name) {
  object
      ->Set(isolate->GetCurrentContext(),
            v8::String::NewFromUtf8(isolate, name).ToLocalChecked(),
            v8::Number::New(isolate, static_cast<double>(value)))
      .FromJust();
}


void StatisticsExtension::GetCounters(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = reinterpret_cast<Isolate*>(args.GetIsolate());
  Heap* heap = isolate->heap();

  if (args.Length() > 0) {  // GC if first argument evaluates to true.
    if (args[0]->IsBoolean() && args[0]->BooleanValue(args.GetIsolate())) {
      heap->CollectAllGarbage(Heap::kNoGCFlags,
                              GarbageCollectionReason::kCountersExtension);
    }
  }

  Counters* counters = isolate->counters();
  v8::Local<v8::Object> result = v8::Object::New(args.GetIsolate());

  struct StatisticsCounter {
    v8::internal::StatsCounter* counter;
    const char* name;
  };
  // clang-format off
  const StatisticsCounter counter_list[] = {
#define ADD_COUNTER(name, caption) {counters->name(), #name},
      STATS_COUNTER_LIST_1(ADD_COUNTER)
      STATS_COUNTER_LIST_2(ADD_COUNTER)
      STATS_COUNTER_NATIVE_CODE_LIST(ADD_COUNTER)
#undef ADD_COUNTER
  };  // End counter_list array.
  // clang-format on

  for (size_t i = 0; i < arraysize(counter_list); i++) {
    AddCounter(args.GetIsolate(), result, counter_list[i].counter,
               counter_list[i].name);
  }

  struct StatisticNumber {
    size_t number;
    const char* name;
  };

  const StatisticNumber numbers[] = {
      {heap->memory_allocator()->Size(), "total_committed_bytes"},
      {heap->new_space()->Size(), "new_space_live_bytes"},
      {heap->new_space()->Available(), "new_space_available_bytes"},
      {heap->new_space()->CommittedMemory(), "new_space_commited_bytes"},
      {heap->old_space()->Size(), "old_space_live_bytes"},
      {heap->old_space()->Available(), "old_space_available_bytes"},
      {heap->old_space()->CommittedMemory(), "old_space_commited_bytes"},
      {heap->code_space()->Size(), "code_space_live_bytes"},
      {heap->code_space()->Available(), "code_space_available_bytes"},
      {heap->code_space()->CommittedMemory(), "code_space_commited_bytes"},
      {heap->lo_space()->Size(), "lo_space_live_bytes"},
      {heap->lo_space()->Available(), "lo_space_available_bytes"},
      {heap->lo_space()->CommittedMemory(), "lo_space_commited_bytes"},
      {heap->code_lo_space()->Size(), "code_lo_space_live_bytes"},
      {heap->code_lo_space()->Available(), "code_lo_space_available_bytes"},
      {heap->code_lo_space()->CommittedMemory(),
       "code_lo_space_commited_bytes"},
  };

  for (size_t i = 0; i < arraysize(numbers); i++) {
    AddNumber(args.GetIsolate(), result, numbers[i].number, numbers[i].name);
  }

  AddNumber64(args.GetIsolate(), result, heap->external_memory(),
              "amount_of_external_allocated_memory");
  args.GetReturnValue().Set(result);

  DisallowGarbageCollection no_gc;
  HeapObjectIterator iterator(
      reinterpret_cast<Isolate*>(args.GetIsolate())->heap());
  int reloc_info_total = 0;
  int source_position_table_total = 0;
  for (HeapObject obj = iterator.Next(); !obj.is_null();
       obj = iterator.Next()) {
    Object maybe_source_positions;
    if (obj.IsCode()) {
      Code code = Code::cast(obj);
      reloc_info_total += code.relocation_info().Size();
      maybe_source_positions = code.source_position_table();
    } else if (obj.IsBytecodeArray()) {
      maybe_source_positions =
          BytecodeArray::cast(obj).source_position_table(kAcquireLoad);
    } else {
      continue;
    }
    if (!maybe_source_positions.IsByteArray()) continue;
    ByteArray source_positions = ByteArray::cast(maybe_source_positions);
    if (source_positions.length() == 0) continue;
    source_position_table_total += source_positions.Size();
  }

  AddNumber(args.GetIsolate(), result, reloc_info_total,
            "reloc_info_total_size");
  AddNumber(args.GetIsolate(), result, source_position_table_total,
            "source_position_table_total_size");
}

}  // namespace internal
}  // namespace v8
