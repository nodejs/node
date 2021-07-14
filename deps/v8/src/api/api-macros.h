// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note 1: Any file that includes this one should include api-macros-undef.h
// at the bottom.

// Note 2: This file is deliberately missing the include guards (the undeffing
// approach wouldn't work otherwise).
//
// PRESUBMIT_INTENTIONALLY_MISSING_INCLUDE_GUARD

/*
 * Most API methods should use one of the three macros:
 *
 * ENTER_V8, ENTER_V8_NO_SCRIPT, ENTER_V8_NO_SCRIPT_NO_EXCEPTION.
 *
 * The latter two assume that no script is executed, and no exceptions are
 * scheduled in addition (respectively). Creating a pending exception and
 * removing it before returning is ok.
 *
 * Exceptions should be handled either by invoking one of the
 * RETURN_ON_FAILED_EXECUTION* macros.
 *
 * Don't use macros with DO_NOT_USE in their name.
 *
 * TODO(jochen): Document debugger specific macros.
 * TODO(jochen): Document LOG_API and other RuntimeCallStats macros.
 * TODO(jochen): All API methods should invoke one of the ENTER_V8* macros.
 * TODO(jochen): Remove calls form API methods to DO_NOT_USE macros.
 */

#define LOG_API(isolate, class_name, function_name)                        \
  RCS_SCOPE(isolate,                                                       \
            i::RuntimeCallCounterId::kAPI_##class_name##_##function_name); \
  LOG(isolate, ApiEntryCall("v8::" #class_name "::" #function_name))

#define ENTER_V8_DO_NOT_USE(isolate) i::VMState<v8::OTHER> __state__((isolate))

#define ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name,  \
                                   function_name, bailout_value,  \
                                   HandleScopeClass, do_callback) \
  if (IsExecutionTerminatingCheck(isolate)) {                     \
    return bailout_value;                                         \
  }                                                               \
  HandleScopeClass handle_scope(isolate);                         \
  CallDepthScope<do_callback> call_depth_scope(isolate, context); \
  LOG_API(isolate, class_name, function_name);                    \
  i::VMState<v8::OTHER> __state__((isolate));                     \
  bool has_pending_exception = false

#define PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE(isolate, T)       \
  if (IsExecutionTerminatingCheck(isolate)) {                                \
    return MaybeLocal<T>();                                                  \
  }                                                                          \
  InternalEscapableScope handle_scope(isolate);                              \
  CallDepthScope<false> call_depth_scope(isolate, v8::Local<v8::Context>()); \
  i::VMState<v8::OTHER> __state__((isolate));                                \
  bool has_pending_exception = false

#define PREPARE_FOR_EXECUTION_WITH_CONTEXT(context, class_name, function_name, \
                                           bailout_value, HandleScopeClass,    \
                                           do_callback)                        \
  auto isolate = context.IsEmpty()                                             \
                     ? i::Isolate::Current()                                   \
                     : reinterpret_cast<i::Isolate*>(context->GetIsolate());   \
  ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name, function_name,      \
                             bailout_value, HandleScopeClass, do_callback);

#define PREPARE_FOR_EXECUTION(context, class_name, function_name, T)          \
  PREPARE_FOR_EXECUTION_WITH_CONTEXT(context, class_name, function_name,      \
                                     MaybeLocal<T>(), InternalEscapableScope, \
                                     false)

#define ENTER_V8(isolate, context, class_name, function_name, bailout_value, \
                 HandleScopeClass)                                           \
  ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name, function_name,    \
                             bailout_value, HandleScopeClass, true)

#ifdef DEBUG
#define ENTER_V8_NO_SCRIPT(isolate, context, class_name, function_name,   \
                           bailout_value, HandleScopeClass)               \
  ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name, function_name, \
                             bailout_value, HandleScopeClass, false);     \
  i::DisallowJavascriptExecutionDebugOnly __no_script__((isolate))

// Lightweight version for APIs that don't require an active context.
#define ASSERT_NO_SCRIPT_NO_EXCEPTION(isolate)                      \
  i::DisallowJavascriptExecutionDebugOnly __no_script__((isolate)); \
  i::DisallowExceptions __no_exceptions__((isolate))

#define ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate) \
  i::VMState<v8::OTHER> __state__((isolate));    \
  ASSERT_NO_SCRIPT_NO_EXCEPTION(isolate)

#define ENTER_V8_FOR_NEW_CONTEXT(isolate)     \
  i::VMState<v8::OTHER> __state__((isolate)); \
  i::DisallowExceptions __no_exceptions__((isolate))
#else
#define ENTER_V8_NO_SCRIPT(isolate, context, class_name, function_name,   \
                           bailout_value, HandleScopeClass)               \
  ENTER_V8_HELPER_DO_NOT_USE(isolate, context, class_name, function_name, \
                             bailout_value, HandleScopeClass, false)

#define ASSERT_NO_SCRIPT_NO_EXCEPTION(isolate)

#define ENTER_V8_NO_SCRIPT_NO_EXCEPTION(isolate) \
  i::VMState<v8::OTHER> __state__((isolate));

#define ENTER_V8_FOR_NEW_CONTEXT(isolate) \
  i::VMState<v8::OTHER> __state__((isolate));
#endif  // DEBUG

#define EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE(isolate, value) \
  do {                                                            \
    if (has_pending_exception) {                                  \
      call_depth_scope.Escape();                                  \
      return value;                                               \
    }                                                             \
  } while (false)

#define RETURN_ON_FAILED_EXECUTION(T) \
  EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE(isolate, MaybeLocal<T>())

#define RETURN_ON_FAILED_EXECUTION_PRIMITIVE(T) \
  EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE(isolate, Nothing<T>())

#define RETURN_ESCAPED(value) return handle_scope.Escape(value);
