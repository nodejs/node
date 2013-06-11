// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "statistics-extension.h"

namespace v8 {
namespace internal {

const char* const StatisticsExtension::kSource =
    "native function getV8Statistics();";


v8::Handle<v8::FunctionTemplate> StatisticsExtension::GetNativeFunction(
    v8::Handle<v8::String> str) {
  ASSERT(strcmp(*v8::String::Utf8Value(str), "getV8Statistics") == 0);
  return v8::FunctionTemplate::New(StatisticsExtension::GetCounters);
}


static void AddCounter(v8::Local<v8::Object> object,
                       StatsCounter* counter,
                       const char* name) {
  if (counter->Enabled()) {
    object->Set(v8::String::New(name),
                v8::Number::New(*counter->GetInternalPointer()));
  }
}

static void AddNumber(v8::Local<v8::Object> object,
                      intptr_t value,
                      const char* name) {
  object->Set(v8::String::New(name),
              v8::Number::New(static_cast<double>(value)));
}


void StatisticsExtension::GetCounters(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = Isolate::Current();
  Heap* heap = isolate->heap();

  if (args.Length() > 0) {  // GC if first argument evaluates to true.
    if (args[0]->IsBoolean() && args[0]->ToBoolean()->Value()) {
      heap->CollectAllGarbage(Heap::kNoGCFlags, "counters extension");
    }
  }

  Counters* counters = isolate->counters();
  v8::Local<v8::Object> result = v8::Object::New();

#define ADD_COUNTER(name, caption)                                            \
  AddCounter(result, counters->name(), #name);

  STATS_COUNTER_LIST_1(ADD_COUNTER)
  STATS_COUNTER_LIST_2(ADD_COUNTER)
#undef ADD_COUNTER
#define ADD_COUNTER(name)                                                     \
  AddCounter(result, counters->count_of_##name(), "count_of_" #name);         \
  AddCounter(result, counters->size_of_##name(),  "size_of_" #name);

  INSTANCE_TYPE_LIST(ADD_COUNTER)
#undef ADD_COUNTER
#define ADD_COUNTER(name)                                                     \
  AddCounter(result, counters->count_of_CODE_TYPE_##name(),                   \
             "count_of_CODE_TYPE_" #name);                                    \
  AddCounter(result, counters->size_of_CODE_TYPE_##name(),                    \
             "size_of_CODE_TYPE_" #name);

  CODE_KIND_LIST(ADD_COUNTER)
#undef ADD_COUNTER
#define ADD_COUNTER(name)                                                     \
  AddCounter(result, counters->count_of_FIXED_ARRAY_##name(),                 \
             "count_of_FIXED_ARRAY_" #name);                                  \
  AddCounter(result, counters->size_of_FIXED_ARRAY_##name(),                  \
             "size_of_FIXED_ARRAY_" #name);

  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(ADD_COUNTER)
#undef ADD_COUNTER

  AddNumber(result, isolate->memory_allocator()->Size(),
            "total_committed_bytes");
  AddNumber(result, heap->new_space()->Size(),
            "new_space_live_bytes");
  AddNumber(result, heap->new_space()->Available(),
            "new_space_available_bytes");
  AddNumber(result, heap->new_space()->CommittedMemory(),
            "new_space_commited_bytes");
  AddNumber(result, heap->old_pointer_space()->Size(),
            "old_pointer_space_live_bytes");
  AddNumber(result, heap->old_pointer_space()->Available(),
            "old_pointer_space_available_bytes");
  AddNumber(result, heap->old_pointer_space()->CommittedMemory(),
            "old_pointer_space_commited_bytes");
  AddNumber(result, heap->old_data_space()->Size(),
            "old_data_space_live_bytes");
  AddNumber(result, heap->old_data_space()->Available(),
            "old_data_space_available_bytes");
  AddNumber(result, heap->old_data_space()->CommittedMemory(),
            "old_data_space_commited_bytes");
  AddNumber(result, heap->code_space()->Size(),
            "code_space_live_bytes");
  AddNumber(result, heap->code_space()->Available(),
            "code_space_available_bytes");
  AddNumber(result, heap->code_space()->CommittedMemory(),
            "code_space_commited_bytes");
  AddNumber(result, heap->cell_space()->Size(),
            "cell_space_live_bytes");
  AddNumber(result, heap->cell_space()->Available(),
            "cell_space_available_bytes");
  AddNumber(result, heap->cell_space()->CommittedMemory(),
            "cell_space_commited_bytes");
  AddNumber(result, heap->lo_space()->Size(),
            "lo_space_live_bytes");
  AddNumber(result, heap->lo_space()->Available(),
            "lo_space_available_bytes");
  AddNumber(result, heap->lo_space()->CommittedMemory(),
            "lo_space_commited_bytes");
  AddNumber(result, heap->amount_of_external_allocated_memory(),
            "amount_of_external_allocated_memory");
  args.GetReturnValue().Set(result);
}


void StatisticsExtension::Register() {
  static StatisticsExtension statistics_extension;
  static v8::DeclareExtension declaration(&statistics_extension);
}

} }  // namespace v8::internal
