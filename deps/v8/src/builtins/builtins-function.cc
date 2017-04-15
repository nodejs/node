// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/compiler.h"
#include "src/string-builder.h"

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
  {
    IncrementalStringBuilder builder(isolate);
    builder.AppendCharacter('(');
    builder.AppendCString(token);
    builder.AppendCharacter('(');
    bool parenthesis_in_arg_string = false;
    if (argc > 1) {
      for (int i = 1; i < argc; ++i) {
        if (i > 1) builder.AppendCharacter(',');
        Handle<String> param;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, param, Object::ToString(isolate, args.at(i)), Object);
        param = String::Flatten(param);
        builder.AppendString(param);
        // If the formal parameters string include ) - an illegal
        // character - it may make the combined function expression
        // compile. We avoid this problem by checking for this early on.
        DisallowHeapAllocation no_gc;  // Ensure vectors stay valid.
        String::FlatContent param_content = param->GetFlatContent();
        for (int i = 0, length = param->length(); i < length; ++i) {
          if (param_content.Get(i) == ')') {
            parenthesis_in_arg_string = true;
            break;
          }
        }
      }
      // If the formal parameters include an unbalanced block comment, the
      // function must be rejected. Since JavaScript does not allow nested
      // comments we can include a trailing block comment to catch this.
      builder.AppendCString("\n/*``*/");
    }
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
    ASSIGN_RETURN_ON_EXCEPTION(isolate, function,
                               Compiler::GetFunctionFromString(
                                   handle(target->native_context(), isolate),
                                   source, ONLY_SINGLE_FUNCTION_LITERAL),
                               Object);
    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result,
        Execution::Call(isolate, function, target_global_proxy, 0, nullptr),
        Object);
    function = Handle<JSFunction>::cast(result);
    function->shared()->set_name_should_print_as_anonymous(true);
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
    Handle<Map> map = Map::AsLanguageMode(
        initial_map, shared_info->language_mode(), shared_info->kind());

    Handle<Context> context(function->context(), isolate);
    function = isolate->factory()->NewFunctionFromSharedFunctionInfo(
        map, shared_info, context, NOT_TENURED);
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
  Handle<Script> script = handle(Script::cast(func->shared()->script()));
  int position = script->GetEvalPosition();
  USE(position);

  return *func;
}

namespace {

Object* DoFunctionBind(Isolate* isolate, BuiltinArguments args) {
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
    Handle<Object> length(Smi::kZero, isolate);
    Maybe<PropertyAttributes> attributes =
        JSReceiver::GetPropertyAttributes(&length_lookup);
    if (!attributes.IsJust()) return isolate->heap()->exception();
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
  // If the targets name is the default JSFunction accessor, we can keep the
  // accessor that's installed by default on the JSBoundFunction. It lazily
  // computes the value from the underlying internal name.
  LookupIterator name_lookup(target, isolate->factory()->name_string(), target,
                             LookupIterator::OWN);
  if (!target->IsJSFunction() ||
      name_lookup.state() != LookupIterator::ACCESSOR ||
      !name_lookup.GetAccessors()->IsAccessorInfo()) {
    Handle<Object> target_name;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, target_name,
                                       Object::GetProperty(&name_lookup));
    Handle<String> name;
    if (target_name->IsString()) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, name,
          Name::ToFunctionName(Handle<String>::cast(target_name)));
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, name, isolate->factory()->NewConsString(
                             isolate->factory()->bound__string(), name));
    } else {
      name = isolate->factory()->bound__string();
    }
    LookupIterator it(function, isolate->factory()->name_string());
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

void Builtins::Generate_FastFunctionPrototypeBind(
    compiler::CodeAssemblerState* state) {
  using compiler::Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  CodeStubAssembler assembler(state);
  Label slow(&assembler);

  Node* argc = assembler.Parameter(BuiltinDescriptor::kArgumentsCount);
  Node* context = assembler.Parameter(BuiltinDescriptor::kContext);
  Node* new_target = assembler.Parameter(BuiltinDescriptor::kNewTarget);

  CodeStubArguments args(&assembler, argc);

  // Check that receiver has instance type of JS_FUNCTION_TYPE
  Node* receiver = args.GetReceiver();
  assembler.GotoIf(assembler.TaggedIsSmi(receiver), &slow);

  Node* receiver_map = assembler.LoadMap(receiver);
  Node* instance_type = assembler.LoadMapInstanceType(receiver_map);
  assembler.GotoIf(
      assembler.Word32NotEqual(instance_type,
                               assembler.Int32Constant(JS_FUNCTION_TYPE)),
      &slow);

  // Disallow binding of slow-mode functions. We need to figure out whether the
  // length and name property are in the original state.
  assembler.Comment("Disallow binding of slow-mode functions");
  assembler.GotoIf(assembler.IsDictionaryMap(receiver_map), &slow);

  // Check whether the length and name properties are still present as
  // AccessorInfo objects. In that case, their value can be recomputed even if
  // the actual value on the object changes.
  assembler.Comment("Check descriptor array length");
  Node* descriptors = assembler.LoadMapDescriptors(receiver_map);
  Node* descriptors_length = assembler.LoadFixedArrayBaseLength(descriptors);
  assembler.GotoIf(assembler.SmiLessThanOrEqual(descriptors_length,
                                                assembler.SmiConstant(1)),
                   &slow);

  // Check whether the length and name properties are still present as
  // AccessorInfo objects. In that case, their value can be recomputed even if
  // the actual value on the object changes.
  assembler.Comment("Check name and length properties");
  const int length_index = JSFunction::kLengthDescriptorIndex;
  Node* maybe_length = assembler.LoadFixedArrayElement(
      descriptors, DescriptorArray::ToKeyIndex(length_index));
  assembler.GotoIf(
      assembler.WordNotEqual(maybe_length,
                             assembler.LoadRoot(Heap::klength_stringRootIndex)),
      &slow);

  Node* maybe_length_accessor = assembler.LoadFixedArrayElement(
      descriptors, DescriptorArray::ToValueIndex(length_index));
  assembler.GotoIf(assembler.TaggedIsSmi(maybe_length_accessor), &slow);
  Node* length_value_map = assembler.LoadMap(maybe_length_accessor);
  assembler.GotoUnless(assembler.IsAccessorInfoMap(length_value_map), &slow);

  const int name_index = JSFunction::kNameDescriptorIndex;
  Node* maybe_name = assembler.LoadFixedArrayElement(
      descriptors, DescriptorArray::ToKeyIndex(name_index));
  assembler.GotoIf(
      assembler.WordNotEqual(maybe_name,
                             assembler.LoadRoot(Heap::kname_stringRootIndex)),
      &slow);

  Node* maybe_name_accessor = assembler.LoadFixedArrayElement(
      descriptors, DescriptorArray::ToValueIndex(name_index));
  assembler.GotoIf(assembler.TaggedIsSmi(maybe_name_accessor), &slow);
  Node* name_value_map = assembler.LoadMap(maybe_name_accessor);
  assembler.GotoUnless(assembler.IsAccessorInfoMap(name_value_map), &slow);

  // Choose the right bound function map based on whether the target is
  // constructable.
  assembler.Comment("Choose the right bound function map");
  Variable bound_function_map(&assembler, MachineRepresentation::kTagged);
  Label with_constructor(&assembler);
  CodeStubAssembler::VariableList vars({&bound_function_map}, assembler.zone());
  Node* native_context = assembler.LoadNativeContext(context);

  Label map_done(&assembler, vars);
  Node* bit_field = assembler.LoadMapBitField(receiver_map);
  int mask = static_cast<int>(1 << Map::kIsConstructor);
  assembler.GotoIf(assembler.IsSetWord32(bit_field, mask), &with_constructor);

  bound_function_map.Bind(assembler.LoadContextElement(
      native_context, Context::BOUND_FUNCTION_WITHOUT_CONSTRUCTOR_MAP_INDEX));
  assembler.Goto(&map_done);

  assembler.Bind(&with_constructor);
  bound_function_map.Bind(assembler.LoadContextElement(
      native_context, Context::BOUND_FUNCTION_WITH_CONSTRUCTOR_MAP_INDEX));
  assembler.Goto(&map_done);

  assembler.Bind(&map_done);

  // Verify that __proto__ matches that of a the target bound function.
  assembler.Comment("Verify that __proto__ matches target bound function");
  Node* prototype = assembler.LoadMapPrototype(receiver_map);
  Node* expected_prototype =
      assembler.LoadMapPrototype(bound_function_map.value());
  assembler.GotoIf(assembler.WordNotEqual(prototype, expected_prototype),
                   &slow);

  // Allocate the arguments array.
  assembler.Comment("Allocate the arguments array");
  Variable argument_array(&assembler, MachineRepresentation::kTagged);
  Label empty_arguments(&assembler);
  Label arguments_done(&assembler, &argument_array);
  assembler.GotoIf(
      assembler.Uint32LessThanOrEqual(argc, assembler.Int32Constant(1)),
      &empty_arguments);
  Node* elements_length = assembler.ChangeUint32ToWord(
      assembler.Int32Sub(argc, assembler.Int32Constant(1)));
  Node* elements = assembler.AllocateFixedArray(FAST_ELEMENTS, elements_length);
  Variable index(&assembler, MachineType::PointerRepresentation());
  index.Bind(assembler.IntPtrConstant(0));
  CodeStubAssembler::VariableList foreach_vars({&index}, assembler.zone());
  args.ForEach(foreach_vars,
               [&assembler, elements, &index](compiler::Node* arg) {
                 assembler.StoreFixedArrayElement(elements, index.value(), arg);
                 assembler.Increment(index);
               },
               assembler.IntPtrConstant(1));
  argument_array.Bind(elements);
  assembler.Goto(&arguments_done);

  assembler.Bind(&empty_arguments);
  argument_array.Bind(assembler.EmptyFixedArrayConstant());
  assembler.Goto(&arguments_done);

  assembler.Bind(&arguments_done);

  // Determine bound receiver.
  assembler.Comment("Determine bound receiver");
  Variable bound_receiver(&assembler, MachineRepresentation::kTagged);
  Label has_receiver(&assembler);
  Label receiver_done(&assembler, &bound_receiver);
  assembler.GotoIf(assembler.Word32NotEqual(argc, assembler.Int32Constant(0)),
                   &has_receiver);
  bound_receiver.Bind(assembler.UndefinedConstant());
  assembler.Goto(&receiver_done);

  assembler.Bind(&has_receiver);
  bound_receiver.Bind(args.AtIndex(0));
  assembler.Goto(&receiver_done);

  assembler.Bind(&receiver_done);

  // Allocate the resulting bound function.
  assembler.Comment("Allocate the resulting bound function");
  Node* bound_function = assembler.Allocate(JSBoundFunction::kSize);
  assembler.StoreMapNoWriteBarrier(bound_function, bound_function_map.value());
  assembler.StoreObjectFieldNoWriteBarrier(
      bound_function, JSBoundFunction::kBoundTargetFunctionOffset, receiver);
  assembler.StoreObjectFieldNoWriteBarrier(bound_function,
                                           JSBoundFunction::kBoundThisOffset,
                                           bound_receiver.value());
  assembler.StoreObjectFieldNoWriteBarrier(
      bound_function, JSBoundFunction::kBoundArgumentsOffset,
      argument_array.value());
  Node* empty_fixed_array = assembler.EmptyFixedArrayConstant();
  assembler.StoreObjectFieldNoWriteBarrier(
      bound_function, JSObject::kPropertiesOffset, empty_fixed_array);
  assembler.StoreObjectFieldNoWriteBarrier(
      bound_function, JSObject::kElementsOffset, empty_fixed_array);

  args.PopAndReturn(bound_function);
  assembler.Bind(&slow);

  Node* target = assembler.LoadFromFrame(
      StandardFrameConstants::kFunctionOffset, MachineType::TaggedPointer());
  assembler.TailCallStub(
      CodeFactory::FunctionPrototypeBind(assembler.isolate()), context, target,
      new_target, argc);
}

// TODO(verwaest): This is a temporary helper until the FastFunctionBind stub
// can tailcall to the builtin directly.
RUNTIME_FUNCTION(Runtime_FunctionBind) {
  DCHECK_EQ(2, args.length());
  Arguments* incoming = reinterpret_cast<Arguments*>(args[0]);
  // Rewrap the arguments as builtins arguments.
  int argc = incoming->length() + BuiltinArguments::kNumExtraArgsWithReceiver;
  BuiltinArguments caller_args(argc, incoming->arguments() + 1);
  return DoFunctionBind(isolate, caller_args);
}

// ES6 section 19.2.3.5 Function.prototype.toString ( )
BUILTIN(FunctionPrototypeToString) {
  HandleScope scope(isolate);
  Handle<Object> receiver = args.receiver();
  if (receiver->IsJSBoundFunction()) {
    return *JSBoundFunction::ToString(Handle<JSBoundFunction>::cast(receiver));
  } else if (receiver->IsJSFunction()) {
    return *JSFunction::ToString(Handle<JSFunction>::cast(receiver));
  }
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError(MessageTemplate::kNotGeneric,
                            isolate->factory()->NewStringFromAsciiChecked(
                                "Function.prototype.toString")));
}

// ES6 section 19.2.3.6 Function.prototype [ @@hasInstance ] ( V )
void Builtins::Generate_FunctionPrototypeHasInstance(
    compiler::CodeAssemblerState* state) {
  using compiler::Node;
  CodeStubAssembler assembler(state);

  Node* f = assembler.Parameter(0);
  Node* v = assembler.Parameter(1);
  Node* context = assembler.Parameter(4);
  Node* result = assembler.OrdinaryHasInstance(context, f, v);
  assembler.Return(result);
}

}  // namespace internal
}  // namespace v8
