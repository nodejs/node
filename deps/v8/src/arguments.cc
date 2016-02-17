// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arguments.h"

#include "src/api.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {


template <typename T>
template <typename V>
v8::Local<V> CustomArguments<T>::GetReturnValue(Isolate* isolate) {
  // Check the ReturnValue.
  Object** handle = &this->begin()[kReturnValueOffset];
  // Nothing was set, return empty handle as per previous behaviour.
  if ((*handle)->IsTheHole()) return v8::Local<V>();
  return Utils::Convert<Object, V>(Handle<Object>(handle));
}


v8::Local<v8::Value> FunctionCallbackArguments::Call(FunctionCallback f) {
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


#define WRITE_CALL_0(Function, ReturnValue)                            \
  v8::Local<ReturnValue> PropertyCallbackArguments::Call(Function f) { \
    Isolate* isolate = this->isolate();                                \
    VMState<EXTERNAL> state(isolate);                                  \
    ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));       \
    PropertyCallbackInfo<ReturnValue> info(begin());                   \
    f(info);                                                           \
    return GetReturnValue<ReturnValue>(isolate);                       \
  }


#define WRITE_CALL_1(Function, ReturnValue, Arg1)                     \
  v8::Local<ReturnValue> PropertyCallbackArguments::Call(Function f,  \
                                                         Arg1 arg1) { \
    Isolate* isolate = this->isolate();                               \
    VMState<EXTERNAL> state(isolate);                                 \
    ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f));      \
    PropertyCallbackInfo<ReturnValue> info(begin());                  \
    f(arg1, info);                                                    \
    return GetReturnValue<ReturnValue>(isolate);                      \
  }


#define WRITE_CALL_2(Function, ReturnValue, Arg1, Arg2)          \
  v8::Local<ReturnValue> PropertyCallbackArguments::Call(        \
      Function f, Arg1 arg1, Arg2 arg2) {                        \
    Isolate* isolate = this->isolate();                          \
    VMState<EXTERNAL> state(isolate);                            \
    ExternalCallbackScope call_scope(isolate, FUNCTION_ADDR(f)); \
    PropertyCallbackInfo<ReturnValue> info(begin());             \
    f(arg1, arg2, info);                                         \
    return GetReturnValue<ReturnValue>(isolate);                 \
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


}  // namespace internal
}  // namespace v8
