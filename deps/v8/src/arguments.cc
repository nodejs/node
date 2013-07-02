// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "v8.h"
#include "arguments.h"

#include "vm-state-inl.h"

namespace v8 {
namespace internal {


static bool Match(void* a, void* b) {
  return a == b;
}


static uint32_t Hash(void* function) {
  uintptr_t as_int = reinterpret_cast<uintptr_t>(function);
  if (sizeof(function) == 4) return static_cast<uint32_t>(as_int);
  uint64_t as_64 = static_cast<uint64_t>(as_int);
  return
      static_cast<uint32_t>(as_64 >> 32) ^
      static_cast<uint32_t>(as_64);
}


CallbackTable::CallbackTable(): map_(Match, 64) {}


bool CallbackTable::Contains(void* function) {
  ASSERT(function != NULL);
  return map_.Lookup(function, Hash(function), false) != NULL;
}


void CallbackTable::InsertCallback(Isolate* isolate,
                           void* function,
                           bool returns_void) {
  if (function == NULL) return;
  // Don't store for performance.
  if (kStoreVoidFunctions != returns_void) return;
  CallbackTable* table = isolate->callback_table();
  if (table == NULL) {
    table = new CallbackTable();
    isolate->set_callback_table(table);
  }
  typedef HashMap::Entry Entry;
  Entry* entry = table->map_.Lookup(function, Hash(function), true);
  ASSERT(entry != NULL);
  ASSERT(entry->value == NULL || entry->value == function);
  entry->value = function;
}


template<typename T>
template<typename V>
v8::Handle<V> CustomArguments<T>::GetReturnValue(Isolate* isolate) {
  // Check the ReturnValue.
  Object** handle = &this->end()[kReturnValueOffset];
  // Nothing was set, return empty handle as per previous behaviour.
  if ((*handle)->IsTheHole()) return v8::Handle<V>();
  return Utils::Convert<Object, V>(Handle<Object>(handle));
}


v8::Handle<v8::Value> FunctionCallbackArguments::Call(InvocationCallback f) {
  Isolate* isolate = this->isolate();
  void* f_as_void = CallbackTable::FunctionToVoidPtr(f);
  bool new_style = CallbackTable::ReturnsVoid(isolate, f_as_void);
  VMState<EXTERNAL> state(isolate);
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));
  if (new_style) {
    FunctionCallback c = reinterpret_cast<FunctionCallback>(f);
    FunctionCallbackInfo<v8::Value> info(end(),
                                         argv_,
                                         argc_,
                                         is_construct_call_);
    c(info);
  } else {
    v8::Arguments args(end(),
                       argv_,
                       argc_,
                       is_construct_call_);
    v8::Handle<v8::Value> return_value = f(args);
    if (!return_value.IsEmpty()) return return_value;
  }
  return GetReturnValue<v8::Value>(isolate);
}


#define WRITE_CALL_0(OldFunction, NewFunction, ReturnValue)                    \
v8::Handle<ReturnValue> PropertyCallbackArguments::Call(OldFunction f) {       \
  Isolate* isolate = this->isolate();                                          \
  void* f_as_void = CallbackTable::FunctionToVoidPtr(f);                       \
  bool new_style = CallbackTable::ReturnsVoid(isolate, f_as_void);             \
  VMState<EXTERNAL> state(isolate);                                            \
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                 \
  if (new_style) {                                                             \
    NewFunction c = reinterpret_cast<NewFunction>(f);                          \
    PropertyCallbackInfo<ReturnValue> info(end());                             \
    c(info);                                                                   \
  } else {                                                                     \
    v8::AccessorInfo info(end());                                              \
    v8::Handle<ReturnValue> return_value = f(info);                            \
    if (!return_value.IsEmpty()) return return_value;                          \
  }                                                                            \
  return GetReturnValue<ReturnValue>(isolate);                                 \
}

#define WRITE_CALL_1(OldFunction, NewFunction, ReturnValue, Arg1)              \
v8::Handle<ReturnValue> PropertyCallbackArguments::Call(OldFunction f,         \
                                                        Arg1 arg1) {           \
  Isolate* isolate = this->isolate();                                          \
  void* f_as_void = CallbackTable::FunctionToVoidPtr(f);                       \
  bool new_style = CallbackTable::ReturnsVoid(isolate, f_as_void);             \
  VMState<EXTERNAL> state(isolate);                                            \
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                 \
  if (new_style) {                                                             \
    NewFunction c = reinterpret_cast<NewFunction>(f);                          \
    PropertyCallbackInfo<ReturnValue> info(end());                             \
    c(arg1, info);                                                             \
  } else {                                                                     \
    v8::AccessorInfo info(end());                                              \
    v8::Handle<ReturnValue> return_value = f(arg1, info);                      \
    if (!return_value.IsEmpty()) return return_value;                          \
  }                                                                            \
  return GetReturnValue<ReturnValue>(isolate);                                 \
}

#define WRITE_CALL_2(OldFunction, NewFunction, ReturnValue, Arg1, Arg2)        \
v8::Handle<ReturnValue> PropertyCallbackArguments::Call(OldFunction f,         \
                                                        Arg1 arg1,             \
                                                        Arg2 arg2) {           \
  Isolate* isolate = this->isolate();                                          \
  void* f_as_void = CallbackTable::FunctionToVoidPtr(f);                       \
  bool new_style = CallbackTable::ReturnsVoid(isolate, f_as_void);             \
  VMState<EXTERNAL> state(isolate);                                            \
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                 \
  if (new_style) {                                                             \
    NewFunction c = reinterpret_cast<NewFunction>(f);                          \
    PropertyCallbackInfo<ReturnValue> info(end());                             \
    c(arg1, arg2, info);                                                       \
  } else {                                                                     \
    v8::AccessorInfo info(end());                                              \
    v8::Handle<ReturnValue> return_value = f(arg1, arg2, info);                \
    if (!return_value.IsEmpty()) return return_value;                          \
  }                                                                            \
  return GetReturnValue<ReturnValue>(isolate);                                 \
}

#define WRITE_CALL_2_VOID(OldFunction, NewFunction, ReturnValue, Arg1, Arg2)   \
void PropertyCallbackArguments::Call(OldFunction f,                            \
                                     Arg1 arg1,                                \
                                     Arg2 arg2) {                              \
  Isolate* isolate = this->isolate();                                          \
  void* f_as_void = CallbackTable::FunctionToVoidPtr(f);                       \
  bool new_style = CallbackTable::ReturnsVoid(isolate, f_as_void);             \
  VMState<EXTERNAL> state(isolate);                                            \
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                 \
  if (new_style) {                                                             \
    NewFunction c = reinterpret_cast<NewFunction>(f);                          \
    PropertyCallbackInfo<ReturnValue> info(end());                             \
    c(arg1, arg2, info);                                                       \
  } else {                                                                     \
    v8::AccessorInfo info(end());                                              \
    f(arg1, arg2, info);                                                       \
  }                                                                            \
}

FOR_EACH_CALLBACK_TABLE_MAPPING_0(WRITE_CALL_0)
FOR_EACH_CALLBACK_TABLE_MAPPING_1(WRITE_CALL_1)
FOR_EACH_CALLBACK_TABLE_MAPPING_2(WRITE_CALL_2)
FOR_EACH_CALLBACK_TABLE_MAPPING_2_VOID_RETURN(WRITE_CALL_2_VOID)

#undef WRITE_CALL_0
#undef WRITE_CALL_1
#undef WRITE_CALL_2
#undef WRITE_CALL_2_VOID


} }  // namespace v8::internal

