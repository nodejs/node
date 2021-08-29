// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_UTILS_H_
#define V8_BUILTINS_BUILTINS_UTILS_H_

#include "src/base/logging.h"
#include "src/builtins/builtins.h"
#include "src/execution/arguments.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/logging/runtime-call-stats-scope.h"

namespace v8 {
namespace internal {

// Arguments object passed to C++ builtins.
class BuiltinArguments : public JavaScriptArguments {
 public:
  BuiltinArguments(int length, Address* arguments)
      : Arguments(length, arguments) {
    // Check we have at least the receiver.
    DCHECK_LE(1, this->length());
  }

  Object operator[](int index) const {
    DCHECK_LT(index, length());
    return Object(*address_of_arg_at(index + kArgsOffset));
  }

  template <class S = Object>
  Handle<S> at(int index) const {
    DCHECK_LT(index, length());
    return Handle<S>(address_of_arg_at(index + kArgsOffset));
  }

  inline void set_at(int index, Object value) {
    DCHECK_LT(index, length());
    *address_of_arg_at(index + kArgsOffset) = value.ptr();
  }

  // Note: this should return the address after the receiver,
  // even when length() == 1.
  inline Address* address_of_first_argument() const {
    return address_of_arg_at(kArgsOffset + 1);  // Skips receiver.
  }

  static constexpr int kNewTargetOffset = 0;
  static constexpr int kTargetOffset = 1;
  static constexpr int kArgcOffset = 2;
  static constexpr int kPaddingOffset = 3;

  static constexpr int kNumExtraArgs = 4;
  static constexpr int kNumExtraArgsWithReceiver = 5;
  static constexpr int kArgsOffset = 4;

  inline Handle<Object> atOrUndefined(Isolate* isolate, int index) const;
  inline Handle<Object> receiver() const;
  inline Handle<JSFunction> target() const;
  inline Handle<HeapObject> new_target() const;

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
#define BUILTIN(name)                                                       \
  V8_WARN_UNUSED_RESULT static Object Builtin_Impl_##name(                  \
      BuiltinArguments args, Isolate* isolate);                             \
                                                                            \
  V8_NOINLINE static Address Builtin_Impl_Stats_##name(                     \
      int args_length, Address* args_object, Isolate* isolate) {            \
    BuiltinArguments args(args_length, args_object);                        \
    RCS_SCOPE(isolate, RuntimeCallCounterId::kBuiltin_##name);              \
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.runtime"),                   \
                 "V8.Builtin_" #name);                                      \
    return CONVERT_OBJECT(Builtin_Impl_##name(args, isolate));              \
  }                                                                         \
                                                                            \
  V8_WARN_UNUSED_RESULT Address Builtin_##name(                             \
      int args_length, Address* args_object, Isolate* isolate) {            \
    DCHECK(isolate->context().is_null() || isolate->context().IsContext()); \
    if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled())) {            \
      return Builtin_Impl_Stats_##name(args_length, args_object, isolate);  \
    }                                                                       \
    BuiltinArguments args(args_length, args_object);                        \
    return CONVERT_OBJECT(Builtin_Impl_##name(args, isolate));              \
  }                                                                         \
                                                                            \
  V8_WARN_UNUSED_RESULT static Object Builtin_Impl_##name(                  \
      BuiltinArguments args, Isolate* isolate)

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
  if (args.receiver()->IsNullOrUndefined(isolate)) {                          \
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
