// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_UTILS_H_
#define V8_BUILTINS_BUILTINS_UTILS_H_

#include "src/base/logging.h"
#include "src/builtins/builtins.h"
#include "src/execution/arguments.h"
#include "src/execution/frame-constants.h"
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
    DCHECK(Tagged<Object>((*at(0)).ptr()).IsObject());
  }

  // Zero index states for receiver.
  Tagged<Object> operator[](int index) const {
    DCHECK_LT(index, length());
    return Tagged<Object>(*address_of_arg_at(index + kArgsIndex));
  }

  // Zero index states for receiver.
  template <class S = Object>
  Handle<S> at(int index) const {
    DCHECK_LT(index, length());
    return Handle<S>(address_of_arg_at(index + kArgsIndex));
  }

  // Zero index states for receiver.
  inline void set_at(int index, Tagged<Object> value) {
    DCHECK_LT(index, length());
    *address_of_arg_at(index + kArgsIndex) = value.ptr();
  }

  // Note: this should return the address after the receiver,
  // even when length() == 1.
  inline Address* address_of_first_argument() const {
    return address_of_arg_at(kFirstArgsIndex);
  }

  static constexpr int kNewTargetIndex = 0;
  static constexpr int kTargetIndex = 1;
  static constexpr int kArgcIndex = 2;
  // TODO(ishell): this padding is required only on arm64.
  static constexpr int kPaddingIndex = 3;

  static constexpr int kNumExtraArgs = 4;
  static constexpr int kNumExtraArgsWithReceiver = 5;

  static constexpr int kArgsIndex = kNumExtraArgs;
  static constexpr int kReceiverIndex = kArgsIndex;
  static constexpr int kFirstArgsIndex = kArgsIndex + 1;  // Skip receiver.
  // Index of the receiver argument in JS arguments array returned by
  // |address_of_first_argument()|.
  static constexpr int kReceiverArgsIndex = kArgsIndex - kFirstArgsIndex;

  // Zero index states for receiver.
  inline Handle<Object> atOrUndefined(Isolate* isolate, int index) const;
  inline Handle<JSAny> receiver() const;
  inline Handle<JSFunction> target() const;
  inline Handle<HeapObject> new_target() const;

  // Gets the total number of arguments including the receiver (but
  // excluding extra arguments).
  int length() const { return Arguments::length() - kNumExtraArgs; }
};

static_assert(BuiltinArguments::kNewTargetIndex ==
              BuiltinExitFrameConstants::kNewTargetIndex);
static_assert(BuiltinArguments::kTargetIndex ==
              BuiltinExitFrameConstants::kTargetIndex);
static_assert(BuiltinArguments::kArgcIndex ==
              BuiltinExitFrameConstants::kArgcIndex);
static_assert(BuiltinArguments::kPaddingIndex ==
              BuiltinExitFrameConstants::kPaddingIndex);

static_assert(BuiltinArguments::kNumExtraArgs ==
              BuiltinExitFrameConstants::kNumExtraArgs);
static_assert(BuiltinArguments::kNumExtraArgsWithReceiver ==
              BuiltinExitFrameConstants::kNumExtraArgsWithReceiver);

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
#define BUILTIN_RCS(name)                                                  \
  V8_WARN_UNUSED_RESULT static Tagged<Object> Builtin_Impl_##name(         \
      BuiltinArguments args, Isolate* isolate);                            \
                                                                           \
  V8_NOINLINE static Address Builtin_Impl_Stats_##name(                    \
      int args_length, Address* args_object, Isolate* isolate) {           \
    BuiltinArguments args(args_length, args_object);                       \
    RCS_SCOPE(isolate, RuntimeCallCounterId::kBuiltin_##name);             \
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.runtime"),                  \
                 "V8.Builtin_" #name);                                     \
    return BUILTIN_CONVERT_RESULT(Builtin_Impl_##name(args, isolate));     \
  }                                                                        \
                                                                           \
  V8_WARN_UNUSED_RESULT Address Builtin_##name(                            \
      int args_length, Address* args_object, Isolate* isolate) {           \
    DCHECK(isolate->context().is_null() || IsContext(isolate->context())); \
    if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled())) {           \
      return Builtin_Impl_Stats_##name(args_length, args_object, isolate); \
    }                                                                      \
    BuiltinArguments args(args_length, args_object);                       \
    return BUILTIN_CONVERT_RESULT(Builtin_Impl_##name(args, isolate));     \
  }                                                                        \
                                                                           \
  V8_WARN_UNUSED_RESULT static Tagged<Object> Builtin_Impl_##name(         \
      BuiltinArguments args, Isolate* isolate)

#define BUILTIN_NO_RCS(name)                                               \
  V8_WARN_UNUSED_RESULT static Tagged<Object> Builtin_Impl_##name(         \
      BuiltinArguments args, Isolate* isolate);                            \
                                                                           \
  V8_WARN_UNUSED_RESULT Address Builtin_##name(                            \
      int args_length, Address* args_object, Isolate* isolate) {           \
    DCHECK(isolate->context().is_null() || IsContext(isolate->context())); \
    BuiltinArguments args(args_length, args_object);                       \
    return BUILTIN_CONVERT_RESULT(Builtin_Impl_##name(args, isolate));     \
  }                                                                        \
                                                                           \
  V8_WARN_UNUSED_RESULT static Tagged<Object> Builtin_Impl_##name(         \
      BuiltinArguments args, Isolate* isolate)

#ifdef V8_RUNTIME_CALL_STATS
#define BUILTIN(name) BUILTIN_RCS(name)
#else  // V8_RUNTIME_CALL_STATS
#define BUILTIN(name) BUILTIN_NO_RCS(name)
#endif  // V8_RUNTIME_CALL_STATS
// ----------------------------------------------------------------------------

#define CHECK_RECEIVER(Type, name, method)                                  \
  if (!Is##Type(*args.receiver())) {                                        \
    THROW_NEW_ERROR_RETURN_FAILURE(                                         \
        isolate,                                                            \
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,          \
                     isolate->factory()->NewStringFromAsciiChecked(method), \
                     args.receiver()));                                     \
  }                                                                         \
  auto name = Cast<Type>(args.receiver())

// Throws a TypeError for {method} if the receiver is not coercible to Object,
// or converts the receiver to a String otherwise and assigns it to a new var
// with the given {name}.
#define TO_THIS_STRING(name, method)                                          \
  if (IsNullOrUndefined(*args.receiver(), isolate)) {                         \
    THROW_NEW_ERROR_RETURN_FAILURE(                                           \
        isolate,                                                              \
        NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,               \
                     isolate->factory()->NewStringFromAsciiChecked(method))); \
  }                                                                           \
  DirectHandle<String> name;                                                  \
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                         \
      isolate, name, Object::ToString(isolate, args.receiver()))

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_UTILS_H_
