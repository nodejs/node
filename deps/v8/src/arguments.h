// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARGUMENTS_H_
#define V8_ARGUMENTS_H_

#include "src/allocation.h"
#include "src/objects.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

// Arguments provides access to runtime call parameters.
//
// It uses the fact that the instance fields of Arguments
// (length_, arguments_) are "overlayed" with the parameters
// (no. of parameters, and the parameter pointer) passed so
// that inside the C++ function, the parameters passed can
// be accessed conveniently:
//
//   Object* Runtime_function(Arguments args) {
//     ... use args[i] here ...
//   }
//
// Note that length_ (whose value is in the integer range) is defined
// as intptr_t to provide endian-neutrality on 64-bit archs.

class Arguments BASE_EMBEDDED {
 public:
  Arguments(int length, Object** arguments)
      : length_(length), arguments_(arguments) {
    DCHECK_GE(length_, 0);
  }

  Object*& operator[] (int index) {
    DCHECK_GE(index, 0);
    DCHECK_LT(static_cast<uint32_t>(index), static_cast<uint32_t>(length_));
    return *(reinterpret_cast<Object**>(reinterpret_cast<intptr_t>(arguments_) -
                                        index * kPointerSize));
  }

  template <class S = Object>
  Handle<S> at(int index) {
    Object** value = &((*this)[index]);
    // This cast checks that the object we're accessing does indeed have the
    // expected type.
    S::cast(*value);
    return Handle<S>(reinterpret_cast<S**>(value));
  }

  int smi_at(int index) { return Smi::ToInt((*this)[index]); }

  double number_at(int index) {
    return (*this)[index]->Number();
  }

  // Get the total number of arguments including the receiver.
  int length() const { return static_cast<int>(length_); }

  Object** arguments() { return arguments_; }

  Object** lowest_address() { return &this->operator[](length() - 1); }

  Object** highest_address() { return &this->operator[](0); }

 private:
  intptr_t length_;
  Object** arguments_;
};

double ClobberDoubleRegisters(double x1, double x2, double x3, double x4);

#ifdef DEBUG
#define CLOBBER_DOUBLE_REGISTERS() ClobberDoubleRegisters(1, 2, 3, 4);
#else
#define CLOBBER_DOUBLE_REGISTERS()
#endif

// TODO(cbruni): add global flag to check whether any tracing events have been
// enabled.
#define RUNTIME_FUNCTION_RETURNS_TYPE(Type, Name)                             \
  static V8_INLINE Type __RT_impl_##Name(Arguments args, Isolate* isolate);   \
                                                                              \
  V8_NOINLINE static Type Stats_##Name(int args_length, Object** args_object, \
                                       Isolate* isolate) {                    \
    RuntimeCallTimerScope timer(isolate, RuntimeCallCounterId::k##Name);      \
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.runtime"),                     \
                 "V8.Runtime_" #Name);                                        \
    Arguments args(args_length, args_object);                                 \
    return __RT_impl_##Name(args, isolate);                                   \
  }                                                                           \
                                                                              \
  Type Name(int args_length, Object** args_object, Isolate* isolate) {        \
    DCHECK(isolate->context() == nullptr || isolate->context()->IsContext()); \
    CLOBBER_DOUBLE_REGISTERS();                                               \
    if (V8_UNLIKELY(FLAG_runtime_stats)) {                                    \
      return Stats_##Name(args_length, args_object, isolate);                 \
    }                                                                         \
    Arguments args(args_length, args_object);                                 \
    return __RT_impl_##Name(args, isolate);                                   \
  }                                                                           \
                                                                              \
  static Type __RT_impl_##Name(Arguments args, Isolate* isolate)

#define RUNTIME_FUNCTION(Name) RUNTIME_FUNCTION_RETURNS_TYPE(Object*, Name)
#define RUNTIME_FUNCTION_RETURN_PAIR(Name) \
    RUNTIME_FUNCTION_RETURNS_TYPE(ObjectPair, Name)

}  // namespace internal
}  // namespace v8

#endif  // V8_ARGUMENTS_H_
