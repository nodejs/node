// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARGUMENTS_H_
#define V8_ARGUMENTS_H_

#include "src/allocation.h"
#include "src/handles.h"
#include "src/objects.h"
#include "src/objects/slots.h"
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
//   Object Runtime_function(Arguments args) {
//     ... use args[i] here ...
//   }
//
// Note that length_ (whose value is in the integer range) is defined
// as intptr_t to provide endian-neutrality on 64-bit archs.

class Arguments {
 public:
  Arguments(int length, Address* arguments)
      : length_(length), arguments_(arguments) {
    DCHECK_GE(length_, 0);
  }

  Object operator[](int index) { return Object(*address_of_arg_at(index)); }

  template <class S = Object>
  inline Handle<S> at(int index);

  inline int smi_at(int index);

  inline double number_at(int index);

  inline void set_at(int index, Object value) {
    *address_of_arg_at(index) = value->ptr();
  }

  inline FullObjectSlot slot_at(int index) {
    return FullObjectSlot(address_of_arg_at(index));
  }

  inline Address* address_of_arg_at(int index) {
    DCHECK_LT(static_cast<uint32_t>(index), static_cast<uint32_t>(length_));
    return reinterpret_cast<Address*>(reinterpret_cast<Address>(arguments_) -
                                      index * kSystemPointerSize);
  }

  // Get the total number of arguments including the receiver.
  int length() const { return static_cast<int>(length_); }

  // Arguments on the stack are in reverse order (compared to an array).
  FullObjectSlot first_slot() { return slot_at(length() - 1); }
  FullObjectSlot last_slot() { return slot_at(0); }

 private:
  intptr_t length_;
  Address* arguments_;
};

template <>
inline Handle<Object> Arguments::at(int index) {
  return Handle<Object>(address_of_arg_at(index));
}

double ClobberDoubleRegisters(double x1, double x2, double x3, double x4);

#ifdef DEBUG
#define CLOBBER_DOUBLE_REGISTERS() ClobberDoubleRegisters(1, 2, 3, 4);
#else
#define CLOBBER_DOUBLE_REGISTERS()
#endif

// TODO(cbruni): add global flag to check whether any tracing events have been
// enabled.
#define RUNTIME_FUNCTION_RETURNS_TYPE(Type, InternalType, Convert, Name)      \
  static V8_INLINE InternalType __RT_impl_##Name(Arguments args,              \
                                                 Isolate* isolate);           \
                                                                              \
  V8_NOINLINE static Type Stats_##Name(int args_length, Address* args_object, \
                                       Isolate* isolate) {                    \
    RuntimeCallTimerScope timer(isolate, RuntimeCallCounterId::k##Name);      \
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.runtime"),                     \
                 "V8.Runtime_" #Name);                                        \
    Arguments args(args_length, args_object);                                 \
    return Convert(__RT_impl_##Name(args, isolate));                          \
  }                                                                           \
                                                                              \
  Type Name(int args_length, Address* args_object, Isolate* isolate) {        \
    DCHECK(isolate->context().is_null() || isolate->context()->IsContext());  \
    CLOBBER_DOUBLE_REGISTERS();                                               \
    if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled())) {              \
      return Stats_##Name(args_length, args_object, isolate);                 \
    }                                                                         \
    Arguments args(args_length, args_object);                                 \
    return Convert(__RT_impl_##Name(args, isolate));                          \
  }                                                                           \
                                                                              \
  static InternalType __RT_impl_##Name(Arguments args, Isolate* isolate)

#define CONVERT_OBJECT(x) (x)->ptr()
#define CONVERT_OBJECTPAIR(x) (x)

#define RUNTIME_FUNCTION(Name) \
  RUNTIME_FUNCTION_RETURNS_TYPE(Address, Object, CONVERT_OBJECT, Name)

#define RUNTIME_FUNCTION_RETURN_PAIR(Name)                                  \
  RUNTIME_FUNCTION_RETURNS_TYPE(ObjectPair, ObjectPair, CONVERT_OBJECTPAIR, \
                                Name)

}  // namespace internal
}  // namespace v8

#endif  // V8_ARGUMENTS_H_
