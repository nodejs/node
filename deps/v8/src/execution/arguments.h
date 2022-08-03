// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ARGUMENTS_H_
#define V8_EXECUTION_ARGUMENTS_H_

#include "src/execution/clobber-registers.h"
#include "src/handles/handles.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/tracing/trace-event.h"
#include "src/utils/allocation.h"

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

template <ArgumentsType arguments_type>
class Arguments {
 public:
  // Scope to temporarily change the value of an argument.
  class ChangeValueScope {
   public:
    inline ChangeValueScope(Isolate* isolate, Arguments* args, int index,
                            Object value);
    ~ChangeValueScope() { *location_ = old_value_->ptr(); }

   private:
    Address* location_;
    Handle<Object> old_value_;
  };

  Arguments(int length, Address* arguments)
      : length_(length), arguments_(arguments) {
    DCHECK_GE(length_, 0);
  }

  V8_INLINE Object operator[](int index) const {
    return Object(*address_of_arg_at(index));
  }

  template <class S = Object>
  V8_INLINE Handle<S> at(int index) const;

  V8_INLINE int smi_value_at(int index) const;
  V8_INLINE uint32_t positive_smi_value_at(int index) const;

  V8_INLINE int tagged_index_value_at(int index) const;

  V8_INLINE double number_value_at(int index) const;

  V8_INLINE FullObjectSlot slot_at(int index) const {
    return FullObjectSlot(address_of_arg_at(index));
  }

  V8_INLINE Address* address_of_arg_at(int index) const {
    DCHECK_LE(static_cast<uint32_t>(index), static_cast<uint32_t>(length_));
    uintptr_t offset = index * kSystemPointerSize;
    if (arguments_type == ArgumentsType::kJS) {
      offset = (length_ - index - 1) * kSystemPointerSize;
    }
    return reinterpret_cast<Address*>(reinterpret_cast<Address>(arguments_) -
                                      offset);
  }

  // Get the total number of arguments including the receiver.
  V8_INLINE int length() const { return static_cast<int>(length_); }

  // Arguments on the stack are in reverse order (compared to an array).
  FullObjectSlot first_slot() const {
    int index = length() - 1;
    if (arguments_type == ArgumentsType::kJS) index = 0;
    return slot_at(index);
  }

  FullObjectSlot last_slot() const {
    int index = 0;
    if (arguments_type == ArgumentsType::kJS) index = length() - 1;
    return slot_at(index);
  }

 private:
  intptr_t length_;
  Address* arguments_;
};

template <ArgumentsType T>
template <class S>
Handle<S> Arguments<T>::at(int index) const {
  Handle<Object> obj = Handle<Object>(address_of_arg_at(index));
  return Handle<S>::cast(obj);
}

#ifdef DEBUG
#define CLOBBER_DOUBLE_REGISTERS() ClobberDoubleRegisters(1, 2, 3, 4);
#else
#define CLOBBER_DOUBLE_REGISTERS()
#endif

// TODO(cbruni): add global flag to check whether any tracing events have been
// enabled.
#ifdef V8_RUNTIME_CALL_STATS
#define RUNTIME_ENTRY_WITH_RCS(Type, InternalType, Convert, Name)             \
  V8_NOINLINE static Type Stats_##Name(int args_length, Address* args_object, \
                                       Isolate* isolate) {                    \
    RCS_SCOPE(isolate, RuntimeCallCounterId::k##Name);                        \
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.runtime"),                     \
                 "V8.Runtime_" #Name);                                        \
    RuntimeArguments args(args_length, args_object);                          \
    return Convert(__RT_impl_##Name(args, isolate));                          \
  }

#define TEST_AND_CALL_RCS(Name)                                \
  if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled())) { \
    return Stats_##Name(args_length, args_object, isolate);    \
  }

#else  // V8_RUNTIME_CALL_STATS
#define RUNTIME_ENTRY_WITH_RCS(Type, InternalType, Convert, Name)
#define TEST_AND_CALL_RCS(Name)

#endif  // V8_RUNTIME_CALL_STATS

#define RUNTIME_FUNCTION_RETURNS_TYPE(Type, InternalType, Convert, Name)    \
  static V8_INLINE InternalType __RT_impl_##Name(RuntimeArguments args,     \
                                                 Isolate* isolate);         \
  RUNTIME_ENTRY_WITH_RCS(Type, InternalType, Convert, Name)                 \
  Type Name(int args_length, Address* args_object, Isolate* isolate) {      \
    DCHECK(isolate->context().is_null() || isolate->context().IsContext()); \
    CLOBBER_DOUBLE_REGISTERS();                                             \
    TEST_AND_CALL_RCS(Name)                                                 \
    RuntimeArguments args(args_length, args_object);                        \
    return Convert(__RT_impl_##Name(args, isolate));                        \
  }                                                                         \
                                                                            \
  static InternalType __RT_impl_##Name(RuntimeArguments args, Isolate* isolate)

#define CONVERT_OBJECT(x) (x).ptr()
#define CONVERT_OBJECTPAIR(x) (x)

#define RUNTIME_FUNCTION(Name) \
  RUNTIME_FUNCTION_RETURNS_TYPE(Address, Object, CONVERT_OBJECT, Name)

#define RUNTIME_FUNCTION_RETURN_PAIR(Name)                                  \
  RUNTIME_FUNCTION_RETURNS_TYPE(ObjectPair, ObjectPair, CONVERT_OBJECTPAIR, \
                                Name)

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ARGUMENTS_H_
