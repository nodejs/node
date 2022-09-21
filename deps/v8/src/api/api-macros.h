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
 * API methods that are part of the debug interface should use
 *
 * PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE
 *
 * in a similar fashion to ENTER_V8.
 *
 * Don't use macros with DO_NOT_USE in their name.
 *
 * TODO(cbruni): Document LOG_API and other RuntimeCallStats macros.
 * TODO(verwaest): All API methods should invoke one of the ENTER_V8* macros.
 * TODO(verwaest): Remove calls form API methods to DO_NOT_USE macros.
 */

#define API_RCS_SCOPE(i_isolate, class_name, function_name) \
  RCS_SCOPE(i_isolate,                                      \
            i::RuntimeCallCounterId::kAPI_##class_name##_##function_name);

#define ENTER_V8_BASIC(i_isolate)                            \
  /* Embedders should never enter V8 after terminating it */ \
  DCHECK(!i_isolate->is_execution_terminating());            \
  i::VMState<v8::OTHER> __state__((i_isolate))

#define ENTER_V8_HELPER_INTERNAL(i_isolate, context, class_name,    \
                                 function_name, bailout_value,      \
                                 HandleScopeClass, do_callback)     \
  if (i_isolate->is_execution_terminating()) {                      \
    return bailout_value;                                           \
  }                                                                 \
  HandleScopeClass handle_scope(i_isolate);                         \
  CallDepthScope<do_callback> call_depth_scope(i_isolate, context); \
  API_RCS_SCOPE(i_isolate, class_name, function_name);              \
  i::VMState<v8::OTHER> __state__((i_isolate));                     \
  bool has_pending_exception = false

#define PREPARE_FOR_DEBUG_INTERFACE_EXECUTION_WITH_ISOLATE(i_isolate, T)       \
  if (i_isolate->is_execution_terminating()) {                                 \
    return MaybeLocal<T>();                                                    \
  }                                                                            \
  InternalEscapableScope handle_scope(i_isolate);                              \
  CallDepthScope<false> call_depth_scope(i_isolate, v8::Local<v8::Context>()); \
  i::VMState<v8::OTHER> __state__((i_isolate));                                \
  bool has_pending_exception = false

#define PREPARE_FOR_EXECUTION_WITH_CONTEXT(context, class_name, function_name, \
                                           bailout_value, HandleScopeClass,    \
                                           do_callback)                        \
  auto i_isolate = context.IsEmpty()                                           \
                       ? i::Isolate::Current()                                 \
                       : reinterpret_cast<i::Isolate*>(context->GetIsolate()); \
  ENTER_V8_HELPER_INTERNAL(i_isolate, context, class_name, function_name,      \
                           bailout_value, HandleScopeClass, do_callback);

#define PREPARE_FOR_EXECUTION(context, class_name, function_name, T)          \
  PREPARE_FOR_EXECUTION_WITH_CONTEXT(context, class_name, function_name,      \
                                     MaybeLocal<T>(), InternalEscapableScope, \
                                     false)

#define ENTER_V8(i_isolate, context, class_name, function_name, bailout_value, \
                 HandleScopeClass)                                             \
  ENTER_V8_HELPER_INTERNAL(i_isolate, context, class_name, function_name,      \
                           bailout_value, HandleScopeClass, true)

#ifdef DEBUG
#define ENTER_V8_NO_SCRIPT(i_isolate, context, class_name, function_name, \
                           bailout_value, HandleScopeClass)               \
  ENTER_V8_HELPER_INTERNAL(i_isolate, context, class_name, function_name, \
                           bailout_value, HandleScopeClass, false);       \
  i::DisallowJavascriptExecutionDebugOnly __no_script__((i_isolate))

#define DCHECK_NO_SCRIPT_NO_EXCEPTION_MAYBE_TEARDOWN(i_isolate)       \
  i::DisallowJavascriptExecutionDebugOnly __no_script__((i_isolate)); \
  i::DisallowExceptions __no_exceptions__((i_isolate))

// Lightweight version for APIs that don't require an active context.
#define DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate)             \
  /* Embedders should never enter V8 after terminating it */ \
  DCHECK(!i_isolate->is_execution_terminating());            \
  DCHECK_NO_SCRIPT_NO_EXCEPTION_MAYBE_TEARDOWN(i_isolate)

#define ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate) \
  i::VMState<v8::OTHER> __state__((i_isolate));    \
  DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate)

// Used instead of ENTER_V8_NO_SCRIPT_NO_EXCEPTION where the V8 Api is entered
// during termination sequences.
#define ENTER_V8_MAYBE_TEARDOWN(i_isolate)      \
  i::VMState<v8::OTHER> __state__((i_isolate)); \
  DCHECK_NO_SCRIPT_NO_EXCEPTION_MAYBE_TEARDOWN(i_isolate)

#define ENTER_V8_FOR_NEW_CONTEXT(i_isolate)         \
  DCHECK(!(i_isolate)->is_execution_terminating()); \
  i::VMState<v8::OTHER> __state__((i_isolate));     \
  i::DisallowExceptions __no_exceptions__((i_isolate))
#else  // DEBUG
#define ENTER_V8_NO_SCRIPT(i_isolate, context, class_name, function_name, \
                           bailout_value, HandleScopeClass)               \
  ENTER_V8_HELPER_INTERNAL(i_isolate, context, class_name, function_name, \
                           bailout_value, HandleScopeClass, false)

#define DCHECK_NO_SCRIPT_NO_EXCEPTION(i_isolate)
#define DCHECK_NO_SCRIPT_NO_EXCEPTION_MAYBE_TEARDOWN(i_isolate)

#define ENTER_V8_NO_SCRIPT_NO_EXCEPTION(i_isolate) \
  i::VMState<v8::OTHER> __state__((i_isolate));

#define ENTER_V8_MAYBE_TEARDOWN(i_isolate) \
  i::VMState<v8::OTHER> __state__((i_isolate));

#define ENTER_V8_FOR_NEW_CONTEXT(i_isolate) \
  i::VMState<v8::OTHER> __state__((i_isolate));
#endif  // DEBUG

#define EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE(i_isolate, value) \
  do {                                                              \
    if (has_pending_exception) {                                    \
      call_depth_scope.Escape();                                    \
      return value;                                                 \
    }                                                               \
  } while (false)

#define RETURN_ON_FAILED_EXECUTION(T) \
  EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE(i_isolate, MaybeLocal<T>())

#define RETURN_ON_FAILED_EXECUTION_PRIMITIVE(T) \
  EXCEPTION_BAILOUT_CHECK_SCOPED_DO_NOT_USE(i_isolate, Nothing<T>())

#define RETURN_ESCAPED(value) return handle_scope.Escape(value);
