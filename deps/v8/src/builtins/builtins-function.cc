// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/compiler.h"
#include "src/logging/counters.h"
#include "src/numbers/conversions.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/lookup.h"
#include "src/objects/objects-inl.h"
#include "src/strings/string-builder-inl.h"

namespace v8 {
namespace internal {

namespace {

// ES6 section 19.2.1.1.1 CreateDynamicFunction
MaybeHandle<Object> CreateDynamicFunction(Isolate* isolate,
                                          BuiltinArguments args,
                                          const char* token) {
  // Compute number of arguments, ignoring the receiver.
  DCHECK_LE(1, args.length());
  int const argc = args.length() - 1;

  Handle<JSFunction> target = args.target();
  Handle<JSObject> target_global_proxy(target->global_proxy(), isolate);

  if (!Builtins::AllowDynamicFunction(isolate, target, target_global_proxy)) {
    isolate->CountUsage(v8::Isolate::kFunctionConstructorReturnedUndefined);
    return isolate->factory()->undefined_value();
  }

  // Build the source string.
  Handle<String> source;
  int parameters_end_pos = kNoSourcePosition;
  {
    IncrementalStringBuilder builder(isolate);
    builder.AppendCharacter('(');
    builder.AppendCString(token);
    builder.AppendCString(" anonymous(");
    bool parenthesis_in_arg_string = false;
    if (argc > 1) {
      for (int i = 1; i < argc; ++i) {
        if (i > 1) builder.AppendCharacter(',');
        Handle<String> param;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, param, Object::ToString(isolate, args.at(i)), Object);
        param = String::Flatten(isolate, param);
        builder.AppendString(param);
      }
    }
    builder.AppendCharacter('\n');
    parameters_end_pos = builder.Length();
    builder.AppendCString(") {\n");
    if (argc > 0) {
      Handle<String> body;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, body, Object::ToString(isolate, args.at(argc)), Object);
      builder.AppendString(body);
    }
    builder.AppendCString("\n})");
    ASSIGN_RETURN_ON_EXCEPTION(isolate, source, builder.Finish(), Object);

    // The SyntaxError must be thrown after all the (observable) ToString
    // conversions are done.
    if (parenthesis_in_arg_string) {
      THROW_NEW_ERROR(isolate,
                      NewSyntaxError(MessageTemplate::kParenthesisInArgString),
                      Object);
    }
  }

  // Compile the string in the constructor and not a helper so that errors to
  // come from here.
  Handle<JSFunction> function;
  {
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, function,
        Compiler::GetFunctionFromString(
            handle(target->native_context(), isolate), source,
            ONLY_SINGLE_FUNCTION_LITERAL, parameters_end_pos),
        Object);
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, function, target_global_proxy, 0, nullptr),
        Object);
    function = Handle<JSFunction>::cast(result);
    function->shared().set_name_should_print_as_anonymous(true);
  }

  // If new.target is equal to target then the function created
  // is already correctly setup and nothing else should be done
  // here. But if new.target is not equal to target then we are
  // have a Function builtin subclassing case and therefore the
  // function has wrong initial map. To fix that we create a new
  // function object with correct initial map.
  Handle<Object> unchecked_new_target = args.new_target();
  if (!unchecked_new_target->IsUndefined(isolate) &&
      !unchecked_new_target.is_identical_to(target)) {
    Handle<JSReceiver> new_target =
        Handle<JSReceiver>::cast(unchecked_new_target);
    Handle<Map> initial_map;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, initial_map,
        JSFunction::GetDerivedMap(isolate, target, new_target), Object);

    Handle<SharedFunctionInfo> shared_info(function->shared(), isolate);
    Handle<Map> map = Map::AsLanguageMode(isolate, initial_map, shared_info);

    Handle<Context> context(function->context(), isolate);
    function = isolate->factory()->NewFunctionFromSharedFunctionInfo(
        map, shared_info, context, AllocationType::kYoung);
  }
  return function;
}

}  // namespace

// ES6 section 19.2.1.1 Function ( p1, p2, ... , pn, body )
BUILTIN(FunctionConstructor) {
  HandleScope scope(isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, CreateDynamicFunction(isolate, args, "function"));
  return *result;
}

// ES6 section 25.2.1.1 GeneratorFunction (p1, p2, ... , pn, body)
BUILTIN(GeneratorFunctionConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(isolate,
                           CreateDynamicFunction(isolate, args, "function*"));
}

BUILTIN(AsyncFunctionConstructor) {
  HandleScope scope(isolate);
  Handle<Object> maybe_func;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, maybe_func,
      CreateDynamicFunction(isolate, args, "async function"));
  if (!maybe_func->IsJSFunction()) return *maybe_func;

  // Do not lazily compute eval position for AsyncFunction, as they may not be
  // determined after the function is resumed.
  Handle<JSFunction> func = Handle<JSFunction>::cast(maybe_func);
  Handle<Script> script =
      handle(Script::cast(func->shared().script()), isolate);
  int position = Script::GetEvalPosition(isolate, script);
  USE(position);

  return *func;
}

BUILTIN(AsyncGeneratorFunctionConstructor) {
  HandleScope scope(isolate);
  Handle<Object> maybe_func;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, maybe_func,
      CreateDynamicFunction(isolate, args, "async function*"));
  if (!maybe_func->IsJSFunction()) return *maybe_func;

  // Do not lazily compute eval position for AsyncFunction, as they may not be
  // determined after the function is resumed.
  Handle<JSFunction> func = Handle<JSFunction>::cast(maybe_func);
  Handle<Script> script =
      handle(Script::cast(func->shared().script()), isolate);
  int position = Script::GetEvalPosition(isolate, script);
  USE(position);

  return *func;
}

namespace {

Object DoFunctionBind(Isolate* isolate, BuiltinArguments args) {
  HandleScope scope(isolate);
  DCHECK_LE(1, args.length());
  if (!args.receiver()->IsCallable()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kFunctionBind));
  }

  // Allocate the bound function with the given {this_arg} and {args}.
  Handle<JSReceiver> target = args.at<JSReceiver>(0);
  Handle<Object> this_arg = isolate->factory()->undefined_value();
  ScopedVector<Handle<Object>> argv(std::max(0, args.length() - 2));
  if (args.length() > 1) {
    this_arg = args.at(1);
    for (int i = 2; i < args.length(); ++i) {
      argv[i - 2] = args.at(i);
    }
  }
  Handle<JSBoundFunction> function;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, function,
      isolate->factory()->NewJSBoundFunction(target, this_arg, argv));

  LookupIterator length_lookup(target, isolate->factory()->length_string(),
                               target, LookupIterator::OWN);
  // Setup the "length" property based on the "length" of the {target}.
  // If the targets length is the default JSFunction accessor, we can keep the
  // accessor that's installed by default on the JSBoundFunction. It lazily
  // computes the value from the underlying internal length.
  if (!target->IsJSFunction() ||
      length_lookup.state() != LookupIterator::ACCESSOR ||
      !length_lookup.GetAccessors()->IsAccessorInfo()) {
    Handle<Object> length(Smi::zero(), isolate);
    Maybe<PropertyAttributes> attributes =
        JSReceiver::GetPropertyAttributes(&length_lookup);
    if (attributes.IsNothing()) return ReadOnlyRoots(isolate).exception();
    if (attributes.FromJust() != ABSENT) {
      Handle<Object> target_length;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, target_length,
                                         Object::GetProperty(&length_lookup));
      if (target_length->IsNumber()) {
        length = isolate->factory()->NewNumber(std::max(
            0.0, DoubleToInteger(target_length->Number()) - argv.length()));
      }
    }
    LookupIterator it(function, isolate->factory()->length_string(), function);
    DCHECK_EQ(LookupIterator::ACCESSOR, it.state());
    RETURN_FAILURE_ON_EXCEPTION(isolate,
                                JSObject::DefineOwnPropertyIgnoreAttributes(
                                    &it, length, it.property_attributes()));
  }

  // Setup the "name" property based on the "name" of the {target}.
  // If the target's name is the default JSFunction accessor, we can keep the
  // accessor that's installed by default on the JSBoundFunction. It lazily
  // computes the value from the underlying internal name.
  LookupIterator name_lookup(target, isolate->factory()->name_string(), target);
  if (!target->IsJSFunction() ||
      name_lookup.state() != LookupIterator::ACCESSOR ||
      !name_lookup.GetAccessors()->IsAccessorInfo() ||
      (name_lookup.IsFound() && !name_lookup.HolderIsReceiver())) {
    Handle<Object> target_name;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, target_name,
                                       Object::GetProperty(&name_lookup));
    Handle<String> name;
    if (target_name->IsString()) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, name,
          Name::ToFunctionName(isolate, Handle<String>::cast(target_name)));
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, name, isolate->factory()->NewConsString(
                             isolate->factory()->bound__string(), name));
    } else {
      name = isolate->factory()->bound__string();
    }
    LookupIterator it(isolate, function, isolate->factory()->name_string());
    DCHECK_EQ(LookupIterator::ACCESSOR, it.state());
    RETURN_FAILURE_ON_EXCEPTION(isolate,
                                JSObject::DefineOwnPropertyIgnoreAttributes(
                                    &it, name, it.property_attributes()));
  }
  return *function;
}

}  // namespace

// ES6 section 19.2.3.2 Function.prototype.bind ( thisArg, ...args )
BUILTIN(FunctionPrototypeBind) { return DoFunctionBind(isolate, args); }

// ES6 section 19.2.3.5 Function.prototype.toString ( )
BUILTIN(FunctionPrototypeToString) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  if (receiver->IsJSBoundFunction()) {
    return *JSBoundFunction::ToString(Handle<JSBoundFunction>::cast(receiver));
  }
  if (receiver->IsJSFunction()) {
    return *JSFunction::ToString(Handle<JSFunction>::cast(receiver));
  }
  // With the revised toString behavior, all callable objects are valid
  // receivers for this method.
  if (receiver->IsJSReceiver() &&
      JSReceiver::cast(*receiver).map().is_callable()) {
    return ReadOnlyRoots(isolate).function_native_code_string();
  }
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNotGeneric,
                            isolate->factory()->NewStringFromAsciiChecked(
                                "Function.prototype.toString"),
                            isolate->factory()->Function_string()));
}

}  // namespace internal
}  // namespace v8
