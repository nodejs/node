// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_UTILS_H_
#define V8_BUILTINS_BUILTINS_UTILS_H_

#include "src/arguments.h"
#include "src/base/logging.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

// Arguments object passed to C++ builtins.
class BuiltinArguments : public Arguments {
 public:
  BuiltinArguments(int length, Object** arguments)
      : Arguments(length, arguments) {
    // Check we have at least the receiver.
    DCHECK_LE(1, this->length());
  }

  Object*& operator[](int index) {
    DCHECK_LT(index, length());
    return Arguments::operator[](index);
  }

  template <class S>
  Handle<S> at(int index) {
    DCHECK_LT(index, length());
    return Arguments::at<S>(index);
  }

  Handle<Object> atOrUndefined(Isolate* isolate, int index) {
    if (index >= length()) {
      return isolate->factory()->undefined_value();
    }
    return at<Object>(index);
  }

  Handle<Object> receiver() { return Arguments::at<Object>(0); }

  static const int kNewTargetOffset = 0;
  static const int kTargetOffset = 1;
  static const int kArgcOffset = 2;
  static const int kNumExtraArgs = 3;
  static const int kNumExtraArgsWithReceiver = 4;

  template <class S>
  Handle<S> target() {
    return Arguments::at<S>(Arguments::length() - 1 - kTargetOffset);
  }
  Handle<HeapObject> new_target() {
    return Arguments::at<HeapObject>(Arguments::length() - 1 -
                                     kNewTargetOffset);
  }

  // Gets the total number of arguments including the receiver (but
  // excluding extra arguments).
  int length() const { return Arguments::length() - kNumExtraArgs; }
};

// ----------------------------------------------------------------------------
// Support macro for defining builtins in C++.
// ----------------------------------------------------------------------------
//
// A builtin function is defined by writing:
//
//   BUILTIN(name) {
//     ...
//   }
//
// In the body of the builtin function the arguments can be accessed
// through the BuiltinArguments object args.
// TODO(cbruni): add global flag to check whether any tracing events have been
// enabled.
#define BUILTIN(name)                                                         \
  MUST_USE_RESULT static Object* Builtin_Impl_##name(BuiltinArguments args,   \
                                                     Isolate* isolate);       \
                                                                              \
  V8_NOINLINE static Object* Builtin_Impl_Stats_##name(                       \
      int args_length, Object** args_object, Isolate* isolate) {              \
    BuiltinArguments args(args_length, args_object);                          \
    RuntimeCallTimerScope timer(isolate, &RuntimeCallStats::Builtin_##name);  \
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.runtime"),                     \
                 "V8.Builtin_" #name);                                        \
    return Builtin_Impl_##name(args, isolate);                                \
  }                                                                           \
                                                                              \
  MUST_USE_RESULT Object* Builtin_##name(                                     \
      int args_length, Object** args_object, Isolate* isolate) {              \
    DCHECK(isolate->context() == nullptr || isolate->context()->IsContext()); \
    if (V8_UNLIKELY(TRACE_EVENT_RUNTIME_CALL_STATS_TRACING_ENABLED() ||       \
                    FLAG_runtime_call_stats)) {                               \
      return Builtin_Impl_Stats_##name(args_length, args_object, isolate);    \
    }                                                                         \
    BuiltinArguments args(args_length, args_object);                          \
    return Builtin_Impl_##name(args, isolate);                                \
  }                                                                           \
                                                                              \
  MUST_USE_RESULT static Object* Builtin_Impl_##name(BuiltinArguments args,   \
                                                     Isolate* isolate)

// ----------------------------------------------------------------------------

#define CHECK_RECEIVER(Type, name, method)                                  \
  if (!args.receiver()->Is##Type()) {                                       \
    THROW_NEW_ERROR_RETURN_FAILURE(                                         \
        isolate,                                                            \
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,          \
                     isolate->factory()->NewStringFromAsciiChecked(method), \
                     args.receiver()));                                     \
  }                                                                         \
  Handle<Type> name = Handle<Type>::cast(args.receiver())

// Throws a TypeError for {method} if the receiver is not coercible to Object,
// or converts the receiver to a String otherwise and assigns it to a new var
// with the given {name}.
#define TO_THIS_STRING(name, method)                                          \
  if (args.receiver()->IsNull(isolate) ||                                     \
      args.receiver()->IsUndefined(isolate)) {                                \
    THROW_NEW_ERROR_RETURN_FAILURE(                                           \
        isolate,                                                              \
        NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,               \
                     isolate->factory()->NewStringFromAsciiChecked(method))); \
  }                                                                           \
  Handle<String> name;                                                        \
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                         \
      isolate, name, Object::ToString(isolate, args.receiver()))

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_UTILS_H_
