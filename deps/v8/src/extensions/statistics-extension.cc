// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/statistics-extension.h"

namespace v8 {
namespace internal {

const char* const StatisticsExtension::kSource =
    "native function getV8Statistics();";


v8::Handle<v8::FunctionTemplate> StatisticsExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate,
    v8::Handle<v8::String> str) {
  DCHECK(strcmp(*v8::String::Utf8Value(str), "getV8Statistics") == 0);
  return v8::FunctionTemplate::New(isolate, StatisticsExtension::GetCounters);
}


static void AddCounter(v8::Isolate* isolate,
                       v8::Local<v8::Object> object,
                       StatsCounter* counter,
                       const char* name) {
  if (counter->Enabled()) {
    object->Set(v8::String::NewFromUtf8(isolate, name),
                v8::Number::New(isolate, *counter->GetInternalPointer()));
  }
}

static void AddNumber(v8::Isolate* isolate,
                      v8::Local<v8::Object> object,
                      intptr_t value,
                      const char* name) {
  object->Set(v8::String::NewFromUtf8(isolate, name),
              v8::Number::New(isolate, static_cast<double>(value)));
}


static void AddNumber64(v8::Isolate* isolate,
                        v8::Local<v8::Object> object,
                        int64_t value,
                        const char* name) {
  object->Set(v8::String::NewFromUtf8(isolate, name),
              v8::Number::New(isolate, static_cast<double>(value)));
}


void StatisticsExtension::GetCounters(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = reinterpret_cast<Isolate*>(args.GetIsolate());
  Heap* heap = isolate->heap();

  if (args.Length() > 0) {  // GC if first argument evaluates to true.
    if (args[0]->IsBoolean() && args[0]->ToBoolean()->Value()) {
      heap->CollectAllGarbage(Heap::kNoGCFlags, "counters extension");
    }
  }

  Counters* counters = isolate->counters();
  v8::Local<v8::Object> result = v8::Object::New(args.GetIsolate());

#define ADD_COUNTER(name, caption)                                            \
  AddCounter(args.GetIsolate(), result, counters->name(), #name);

  STATS_COUNTER_LIST_1(ADD_COUNTER)
  STATS_COUNTER_LIST_2(ADD_COUNTER)
#undef ADD_COUNTER
#define ADD_COUNTER(name)                                                      \
  AddCounter(args.GetIsolate(), result, counters->count_of_##name(),           \
             "count_of_" #name);                                               \
  AddCounter(args.GetIsolate(), result, counters->size_of_##name(),            \
             "size_of_" #name);

  INSTANCE_TYPE_LIST(ADD_COUNTER)
#undef ADD_COUNTER
#define ADD_COUNTER(name)                                                      \
  AddCounter(args.GetIsolate(), result, counters->count_of_CODE_TYPE_##name(), \
             "count_of_CODE_TYPE_" #name);                                     \
  AddCounter(args.GetIsolate(), result, counters->size_of_CODE_TYPE_##name(),  \
             "size_of_CODE_TYPE_" #name);

  CODE_KIND_LIST(ADD_COUNTER)
#undef ADD_COUNTER
#define ADD_COUNTER(name)                                                      \
  AddCounter(args.GetIsolate(), result,                                        \
             counters->count_of_FIXED_ARRAY_##name(),                          \
             "count_of_FIXED_ARRAY_" #name);                                   \
  AddCounter(args.GetIsolate(), result,                                        \
             counters->size_of_FIXED_ARRAY_##name(),                           \
             "size_of_FIXED_ARRAY_" #name);

  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(ADD_COUNTER)
#undef ADD_COUNTER

  AddNumber(args.GetIsolate(), result, isolate->memory_allocator()->Size(),
            "total_committed_bytes");
  AddNumber(args.GetIsolate(), result, heap->new_space()->Size(),
            "new_space_live_bytes");
  AddNumber(args.GetIsolate(), result, heap->new_space()->Available(),
            "new_space_available_bytes");
  AddNumber(args.GetIsolate(), result, heap->new_space()->CommittedMemory(),
            "new_space_commited_bytes");
  AddNumber(args.GetIsolate(), result, heap->old_pointer_space()->Size(),
            "old_pointer_space_live_bytes");
  AddNumber(args.GetIsolate(), result, heap->old_pointer_space()->Available(),
            "old_pointer_space_available_bytes");
  AddNumber(args.GetIsolate(), result,
            heap->old_pointer_space()->CommittedMemory(),
            "old_pointer_space_commited_bytes");
  AddNumber(args.GetIsolate(), result, heap->old_data_space()->Size(),
            "old_data_space_live_bytes");
  AddNumber(args.GetIsolate(), result, heap->old_data_space()->Available(),
            "old_data_space_available_bytes");
  AddNumber(args.GetIsolate(), result,
            heap->old_data_space()->CommittedMemory(),
            "old_data_space_commited_bytes");
  AddNumber(args.GetIsolate(), result, heap->code_space()->Size(),
            "code_space_live_bytes");
  AddNumber(args.GetIsolate(), result, heap->code_space()->Available(),
            "code_space_available_bytes");
  AddNumber(args.GetIsolate(), result, heap->code_space()->CommittedMemory(),
            "code_space_commited_bytes");
  AddNumber(args.GetIsolate(), result, heap->cell_space()->Size(),
            "cell_space_live_bytes");
  AddNumber(args.GetIsolate(), result, heap->cell_space()->Available(),
            "cell_space_available_bytes");
  AddNumber(args.GetIsolate(), result, heap->cell_space()->CommittedMemory(),
            "cell_space_commited_bytes");
  AddNumber(args.GetIsolate(), result, heap->property_cell_space()->Size(),
            "property_cell_space_live_bytes");
  AddNumber(args.GetIsolate(), result, heap->property_cell_space()->Available(),
            "property_cell_space_available_bytes");
  AddNumber(args.GetIsolate(), result,
            heap->property_cell_space()->CommittedMemory(),
            "property_cell_space_commited_bytes");
  AddNumber(args.GetIsolate(), result, heap->lo_space()->Size(),
            "lo_space_live_bytes");
  AddNumber(args.GetIsolate(), result, heap->lo_space()->Available(),
            "lo_space_available_bytes");
  AddNumber(args.GetIsolate(), result, heap->lo_space()->CommittedMemory(),
            "lo_space_commited_bytes");
  AddNumber64(args.GetIsolate(), result,
              heap->amount_of_external_allocated_memory(),
              "amount_of_external_allocated_memory");
  args.GetReturnValue().Set(result);
}

} }  // namespace v8::internal
