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


template<typename T>
template<typename V>
v8::Handle<V> CustomArguments<T>::GetReturnValue(Isolate* isolate) {
  // Check the ReturnValue.
  Object** handle = &this->begin()[kReturnValueOffset];
  // Nothing was set, return empty handle as per previous behaviour.
  if ((*handle)->IsTheHole()) return v8::Handle<V>();
  return Utils::Convert<Object, V>(Handle<Object>(handle));
}


v8::Handle<v8::Value> FunctionCallbackArguments::Call(FunctionCallback f) {
  Isolate* isolate = this->isolate();
  VMState<EXTERNAL> state(isolate);
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));
  FunctionCallbackInfo<v8::Value> info(begin(),
                                       argv_,
                                       argc_,
                                       is_construct_call_);
  f(info);
  return GetReturnValue<v8::Value>(isolate);
}


#define WRITE_CALL_0(Function, ReturnValue)                                    \
v8::Handle<ReturnValue> PropertyCallbackArguments::Call(Function f) {          \
  Isolate* isolate = this->isolate();                                          \
  VMState<EXTERNAL> state(isolate);                                            \
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                 \
  PropertyCallbackInfo<ReturnValue> info(begin());                             \
  f(info);                                                                     \
  return GetReturnValue<ReturnValue>(isolate);                                 \
}


#define WRITE_CALL_1(Function, ReturnValue, Arg1)                              \
v8::Handle<ReturnValue> PropertyCallbackArguments::Call(Function f,            \
                                                        Arg1 arg1) {           \
  Isolate* isolate = this->isolate();                                          \
  VMState<EXTERNAL> state(isolate);                                            \
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                 \
  PropertyCallbackInfo<ReturnValue> info(begin());                             \
  f(arg1, info);                                                               \
  return GetReturnValue<ReturnValue>(isolate);                                 \
}


#define WRITE_CALL_2(Function, ReturnValue, Arg1, Arg2)                        \
v8::Handle<ReturnValue> PropertyCallbackArguments::Call(Function f,            \
                                                        Arg1 arg1,             \
                                                        Arg2 arg2) {           \
  Isolate* isolate = this->isolate();                                          \
  VMState<EXTERNAL> state(isolate);                                            \
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                 \
  PropertyCallbackInfo<ReturnValue> info(begin());                             \
  f(arg1, arg2, info);                                                         \
  return GetReturnValue<ReturnValue>(isolate);                                 \
}


#define WRITE_CALL_2_VOID(Function, ReturnValue, Arg1, Arg2)                   \
void PropertyCallbackArguments::Call(Function f,                               \
                                     Arg1 arg1,                                \
                                     Arg2 arg2) {                              \
  Isolate* isolate = this->isolate();                                          \
  VMState<EXTERNAL> state(isolate);                                            \
  ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));                 \
  PropertyCallbackInfo<ReturnValue> info(begin());                             \
  f(arg1, arg2, info);                                                         \
}


FOR_EACH_CALLBACK_TABLE_MAPPING_0(WRITE_CALL_0)
FOR_EACH_CALLBACK_TABLE_MAPPING_1(WRITE_CALL_1)
FOR_EACH_CALLBACK_TABLE_MAPPING_2(WRITE_CALL_2)
FOR_EACH_CALLBACK_TABLE_MAPPING_2_VOID_RETURN(WRITE_CALL_2_VOID)

#undef WRITE_CALL_0
#undef WRITE_CALL_1
#undef WRITE_CALL_2
#undef WRITE_CALL_2_VOID


double ClobberDoubleRegisters(double x1, double x2, double x3, double x4) {
  // TODO(ulan): This clobbers only subset of registers depending on compiler,
  // Rewrite this in assembly to really clobber all registers.
  // GCC for ia32 uses the FPU and does not touch XMM registers.
  return x1 * 1.01 + x2 * 2.02 + x3 * 3.03 + x4 * 4.04;
}


} }  // namespace v8::internal
