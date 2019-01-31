// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/elements.h"

#include "src/counters.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

namespace {
enum UncurryThisFunctionContextSlot {
  kFunctionSlot = Context::MIN_CONTEXT_SLOTS,
  kFunctionContextLength,
};
}  // namespace

// These functions are key for safe meta-programming:
// http://wiki.ecmascript.org/doku.php?id=conventions:safe_meta_programming
//
// Technically they could all be derived from combinations of
// Function.prototype.{bind,call,apply} but that introduces lots of layers of
// indirection.
//
// Equivalent to:
//
//   function uncurryThis(func) {
//     return function(thisArg, ...args) {
//       return %reflect_apply(func, thisArg, args);
//     };
//   };
//
BUILTIN(ExtrasUtilsUncurryThis) {
  HandleScope scope(isolate);

  DCHECK_EQ(2, args.length());
  Handle<JSFunction> function = args.at<JSFunction>(1);
  Handle<NativeContext> native_context(isolate->context()->native_context(),
                                       isolate);
  Handle<Context> context = isolate->factory()->NewBuiltinContext(
      native_context,
      static_cast<int>(UncurryThisFunctionContextSlot::kFunctionContextLength));

  context->set(static_cast<int>(UncurryThisFunctionContextSlot::kFunctionSlot),
               *function);

  Handle<SharedFunctionInfo> info =
      isolate->factory()->NewSharedFunctionInfoForBuiltin(
          isolate->factory()->empty_string(),
          Builtins::kExtrasUtilsCallReflectApply, kNormalFunction);
  info->DontAdaptArguments();

  Handle<Map> map = isolate->strict_function_without_prototype_map();
  Handle<JSFunction> new_bound_function =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(map, info, context);

  return *new_bound_function;
}

BUILTIN(ExtrasUtilsCallReflectApply) {
  HandleScope scope(isolate);
  Handle<Context> context(isolate->context(), isolate);
  Handle<NativeContext> native_context(isolate->context()->native_context(),
                                       isolate);
  Handle<JSFunction> function(
      JSFunction::cast(context->get(
          static_cast<int>(UncurryThisFunctionContextSlot::kFunctionSlot))),
      isolate);

  Handle<Object> this_arg = args.at(1);

  int const rest_args_atart = 2;
  Arguments argv(args.length() - rest_args_atart,
                 args.address_of_arg_at(rest_args_atart));
  Handle<JSArray> rest_args_array = isolate->factory()->NewJSArray(0);
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, ArrayConstructInitializeElements(rest_args_array, &argv));

  Handle<Object> reflect_apply_args[] = {function, this_arg, rest_args_array};
  Handle<JSFunction> reflect_apply(native_context->reflect_apply(), isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      Execution::Call(isolate, reflect_apply,
                      isolate->factory()->undefined_value(),
                      arraysize(reflect_apply_args), reflect_apply_args));
}

}  // namespace internal
}  // namespace v8
