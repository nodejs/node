// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"

namespace v8 {
namespace internal {

Handle<Code> Builtins::NonPrimitiveToPrimitive(ToPrimitiveHint hint) {
  switch (hint) {
    case ToPrimitiveHint::kDefault:
      return NonPrimitiveToPrimitive_Default();
    case ToPrimitiveHint::kNumber:
      return NonPrimitiveToPrimitive_Number();
    case ToPrimitiveHint::kString:
      return NonPrimitiveToPrimitive_String();
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

namespace {
// ES6 section 7.1.1 ToPrimitive ( input [ , PreferredType ] )
void Generate_NonPrimitiveToPrimitive(CodeStubAssembler* assembler,
                                      ToPrimitiveHint hint) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;

  Node* input = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);

  // Lookup the @@toPrimitive property on the {input}.
  Callable callable = CodeFactory::GetProperty(assembler->isolate());
  Node* to_primitive_symbol =
      assembler->HeapConstant(assembler->factory()->to_primitive_symbol());
  Node* exotic_to_prim =
      assembler->CallStub(callable, context, input, to_primitive_symbol);

  // Check if {exotic_to_prim} is neither null nor undefined.
  Label ordinary_to_primitive(assembler);
  assembler->GotoIf(
      assembler->WordEqual(exotic_to_prim, assembler->NullConstant()),
      &ordinary_to_primitive);
  assembler->GotoIf(
      assembler->WordEqual(exotic_to_prim, assembler->UndefinedConstant()),
      &ordinary_to_primitive);
  {
    // Invoke the {exotic_to_prim} method on the {input} with a string
    // representation of the {hint}.
    Callable callable = CodeFactory::Call(assembler->isolate());
    Node* hint_string = assembler->HeapConstant(
        assembler->factory()->ToPrimitiveHintString(hint));
    Node* result = assembler->CallJS(callable, context, exotic_to_prim, input,
                                     hint_string);

    // Verify that the {result} is actually a primitive.
    Label if_resultisprimitive(assembler),
        if_resultisnotprimitive(assembler, Label::kDeferred);
    assembler->GotoIf(assembler->TaggedIsSmi(result), &if_resultisprimitive);
    Node* result_instance_type = assembler->LoadInstanceType(result);
    STATIC_ASSERT(FIRST_PRIMITIVE_TYPE == FIRST_TYPE);
    assembler->Branch(assembler->Int32LessThanOrEqual(
                          result_instance_type,
                          assembler->Int32Constant(LAST_PRIMITIVE_TYPE)),
                      &if_resultisprimitive, &if_resultisnotprimitive);

    assembler->Bind(&if_resultisprimitive);
    {
      // Just return the {result}.
      assembler->Return(result);
    }

    assembler->Bind(&if_resultisnotprimitive);
    {
      // Somehow the @@toPrimitive method on {input} didn't yield a primitive.
      assembler->TailCallRuntime(Runtime::kThrowCannotConvertToPrimitive,
                                 context);
    }
  }

  // Convert using the OrdinaryToPrimitive algorithm instead.
  assembler->Bind(&ordinary_to_primitive);
  {
    Callable callable = CodeFactory::OrdinaryToPrimitive(
        assembler->isolate(), (hint == ToPrimitiveHint::kString)
                                  ? OrdinaryToPrimitiveHint::kString
                                  : OrdinaryToPrimitiveHint::kNumber);
    assembler->TailCallStub(callable, context, input);
  }
}
}  // anonymous namespace

void Builtins::Generate_NonPrimitiveToPrimitive_Default(
    compiler::CodeAssemblerState* state) {
  CodeStubAssembler assembler(state);
  Generate_NonPrimitiveToPrimitive(&assembler, ToPrimitiveHint::kDefault);
}

void Builtins::Generate_NonPrimitiveToPrimitive_Number(
    compiler::CodeAssemblerState* state) {
  CodeStubAssembler assembler(state);
  Generate_NonPrimitiveToPrimitive(&assembler, ToPrimitiveHint::kNumber);
}

void Builtins::Generate_NonPrimitiveToPrimitive_String(
    compiler::CodeAssemblerState* state) {
  CodeStubAssembler assembler(state);
  Generate_NonPrimitiveToPrimitive(&assembler, ToPrimitiveHint::kString);
}

void Builtins::Generate_StringToNumber(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* input = assembler.Parameter(Descriptor::kArgument);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.Return(assembler.StringToNumber(context, input));
}

void Builtins::Generate_ToName(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* input = assembler.Parameter(Descriptor::kArgument);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.Return(assembler.ToName(context, input));
}

// static
void Builtins::Generate_NonNumberToNumber(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* input = assembler.Parameter(Descriptor::kArgument);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.Return(assembler.NonNumberToNumber(context, input));
}

// ES6 section 7.1.3 ToNumber ( argument )
void Builtins::Generate_ToNumber(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* input = assembler.Parameter(Descriptor::kArgument);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.Return(assembler.ToNumber(context, input));
}

void Builtins::Generate_ToString(compiler::CodeAssemblerState* state) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* input = assembler.Parameter(Descriptor::kArgument);
  Node* context = assembler.Parameter(Descriptor::kContext);

  Label is_number(&assembler);
  Label runtime(&assembler);

  assembler.GotoIf(assembler.TaggedIsSmi(input), &is_number);

  Node* input_map = assembler.LoadMap(input);
  Node* input_instance_type = assembler.LoadMapInstanceType(input_map);

  Label not_string(&assembler);
  assembler.GotoUnless(assembler.IsStringInstanceType(input_instance_type),
                       &not_string);
  assembler.Return(input);

  Label not_heap_number(&assembler);

  assembler.Bind(&not_string);
  {
    assembler.GotoUnless(assembler.IsHeapNumberMap(input_map),
                         &not_heap_number);
    assembler.Goto(&is_number);
  }

  assembler.Bind(&is_number);
  { assembler.Return(assembler.NumberToString(context, input)); }

  assembler.Bind(&not_heap_number);
  {
    assembler.GotoIf(
        assembler.Word32NotEqual(input_instance_type,
                                 assembler.Int32Constant(ODDBALL_TYPE)),
        &runtime);
    assembler.Return(
        assembler.LoadObjectField(input, Oddball::kToStringOffset));
  }

  assembler.Bind(&runtime);
  {
    assembler.Return(assembler.CallRuntime(Runtime::kToString, context, input));
  }
}

Handle<Code> Builtins::OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint) {
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      return OrdinaryToPrimitive_Number();
    case OrdinaryToPrimitiveHint::kString:
      return OrdinaryToPrimitive_String();
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

namespace {
// 7.1.1.1 OrdinaryToPrimitive ( O, hint )
void Generate_OrdinaryToPrimitive(CodeStubAssembler* assembler,
                                  OrdinaryToPrimitiveHint hint) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;
  typedef TypeConversionDescriptor Descriptor;

  Node* input = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Variable var_result(assembler, MachineRepresentation::kTagged);
  Label return_result(assembler, &var_result);

  Handle<String> method_names[2];
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      method_names[0] = assembler->factory()->valueOf_string();
      method_names[1] = assembler->factory()->toString_string();
      break;
    case OrdinaryToPrimitiveHint::kString:
      method_names[0] = assembler->factory()->toString_string();
      method_names[1] = assembler->factory()->valueOf_string();
      break;
  }
  for (Handle<String> name : method_names) {
    // Lookup the {name} on the {input}.
    Callable callable = CodeFactory::GetProperty(assembler->isolate());
    Node* name_string = assembler->HeapConstant(name);
    Node* method = assembler->CallStub(callable, context, input, name_string);

    // Check if the {method} is callable.
    Label if_methodiscallable(assembler),
        if_methodisnotcallable(assembler, Label::kDeferred);
    assembler->GotoIf(assembler->TaggedIsSmi(method), &if_methodisnotcallable);
    Node* method_map = assembler->LoadMap(method);
    assembler->Branch(assembler->IsCallableMap(method_map),
                      &if_methodiscallable, &if_methodisnotcallable);

    assembler->Bind(&if_methodiscallable);
    {
      // Call the {method} on the {input}.
      Callable callable = CodeFactory::Call(assembler->isolate());
      Node* result = assembler->CallJS(callable, context, method, input);
      var_result.Bind(result);

      // Return the {result} if it is a primitive.
      assembler->GotoIf(assembler->TaggedIsSmi(result), &return_result);
      Node* result_instance_type = assembler->LoadInstanceType(result);
      STATIC_ASSERT(FIRST_PRIMITIVE_TYPE == FIRST_TYPE);
      assembler->GotoIf(assembler->Int32LessThanOrEqual(
                            result_instance_type,
                            assembler->Int32Constant(LAST_PRIMITIVE_TYPE)),
                        &return_result);
    }

    // Just continue with the next {name} if the {method} is not callable.
    assembler->Goto(&if_methodisnotcallable);
    assembler->Bind(&if_methodisnotcallable);
  }

  assembler->TailCallRuntime(Runtime::kThrowCannotConvertToPrimitive, context);

  assembler->Bind(&return_result);
  assembler->Return(var_result.value());
}
}  // anonymous namespace

void Builtins::Generate_OrdinaryToPrimitive_Number(
    compiler::CodeAssemblerState* state) {
  CodeStubAssembler assembler(state);
  Generate_OrdinaryToPrimitive(&assembler, OrdinaryToPrimitiveHint::kNumber);
}

void Builtins::Generate_OrdinaryToPrimitive_String(
    compiler::CodeAssemblerState* state) {
  CodeStubAssembler assembler(state);
  Generate_OrdinaryToPrimitive(&assembler, OrdinaryToPrimitiveHint::kString);
}

// ES6 section 7.1.2 ToBoolean ( argument )
void Builtins::Generate_ToBoolean(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef TypeConversionDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* value = assembler.Parameter(Descriptor::kArgument);

  Label return_true(&assembler), return_false(&assembler);
  assembler.BranchIfToBooleanIsTrue(value, &return_true, &return_false);

  assembler.Bind(&return_true);
  assembler.Return(assembler.BooleanConstant(true));

  assembler.Bind(&return_false);
  assembler.Return(assembler.BooleanConstant(false));
}

void Builtins::Generate_ToLength(compiler::CodeAssemblerState* state) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;
  CodeStubAssembler assembler(state);

  Node* context = assembler.Parameter(1);

  // We might need to loop once for ToNumber conversion.
  Variable var_len(&assembler, MachineRepresentation::kTagged);
  Label loop(&assembler, &var_len);
  var_len.Bind(assembler.Parameter(0));
  assembler.Goto(&loop);
  assembler.Bind(&loop);
  {
    // Shared entry points.
    Label return_len(&assembler),
        return_two53minus1(&assembler, Label::kDeferred),
        return_zero(&assembler, Label::kDeferred);

    // Load the current {len} value.
    Node* len = var_len.value();

    // Check if {len} is a positive Smi.
    assembler.GotoIf(assembler.TaggedIsPositiveSmi(len), &return_len);

    // Check if {len} is a (negative) Smi.
    assembler.GotoIf(assembler.TaggedIsSmi(len), &return_zero);

    // Check if {len} is a HeapNumber.
    Label if_lenisheapnumber(&assembler),
        if_lenisnotheapnumber(&assembler, Label::kDeferred);
    assembler.Branch(assembler.IsHeapNumberMap(assembler.LoadMap(len)),
                     &if_lenisheapnumber, &if_lenisnotheapnumber);

    assembler.Bind(&if_lenisheapnumber);
    {
      // Load the floating-point value of {len}.
      Node* len_value = assembler.LoadHeapNumberValue(len);

      // Check if {len} is not greater than zero.
      assembler.GotoUnless(assembler.Float64GreaterThan(
                               len_value, assembler.Float64Constant(0.0)),
                           &return_zero);

      // Check if {len} is greater than or equal to 2^53-1.
      assembler.GotoIf(
          assembler.Float64GreaterThanOrEqual(
              len_value, assembler.Float64Constant(kMaxSafeInteger)),
          &return_two53minus1);

      // Round the {len} towards -Infinity.
      Node* value = assembler.Float64Floor(len_value);
      Node* result = assembler.ChangeFloat64ToTagged(value);
      assembler.Return(result);
    }

    assembler.Bind(&if_lenisnotheapnumber);
    {
      // Need to convert {len} to a Number first.
      Callable callable = CodeFactory::NonNumberToNumber(assembler.isolate());
      var_len.Bind(assembler.CallStub(callable, context, len));
      assembler.Goto(&loop);
    }

    assembler.Bind(&return_len);
    assembler.Return(var_len.value());

    assembler.Bind(&return_two53minus1);
    assembler.Return(assembler.NumberConstant(kMaxSafeInteger));

    assembler.Bind(&return_zero);
    assembler.Return(assembler.SmiConstant(Smi::kZero));
  }
}

void Builtins::Generate_ToInteger(compiler::CodeAssemblerState* state) {
  typedef TypeConversionDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  compiler::Node* input = assembler.Parameter(Descriptor::kArgument);
  compiler::Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.Return(assembler.ToInteger(context, input));
}

// ES6 section 7.1.13 ToObject (argument)
void Builtins::Generate_ToObject(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  typedef TypeConversionDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Label if_number(&assembler, Label::kDeferred), if_notsmi(&assembler),
      if_jsreceiver(&assembler), if_noconstructor(&assembler, Label::kDeferred),
      if_wrapjsvalue(&assembler);

  Node* object = assembler.Parameter(Descriptor::kArgument);
  Node* context = assembler.Parameter(Descriptor::kContext);

  Variable constructor_function_index_var(&assembler,
                                          MachineType::PointerRepresentation());

  assembler.Branch(assembler.TaggedIsSmi(object), &if_number, &if_notsmi);

  assembler.Bind(&if_notsmi);
  Node* map = assembler.LoadMap(object);

  assembler.GotoIf(assembler.IsHeapNumberMap(map), &if_number);

  Node* instance_type = assembler.LoadMapInstanceType(map);
  assembler.GotoIf(assembler.IsJSReceiverInstanceType(instance_type),
                   &if_jsreceiver);

  Node* constructor_function_index =
      assembler.LoadMapConstructorFunctionIndex(map);
  assembler.GotoIf(assembler.WordEqual(constructor_function_index,
                                       assembler.IntPtrConstant(
                                           Map::kNoConstructorFunctionIndex)),
                   &if_noconstructor);
  constructor_function_index_var.Bind(constructor_function_index);
  assembler.Goto(&if_wrapjsvalue);

  assembler.Bind(&if_number);
  constructor_function_index_var.Bind(
      assembler.IntPtrConstant(Context::NUMBER_FUNCTION_INDEX));
  assembler.Goto(&if_wrapjsvalue);

  assembler.Bind(&if_wrapjsvalue);
  Node* native_context = assembler.LoadNativeContext(context);
  Node* constructor = assembler.LoadFixedArrayElement(
      native_context, constructor_function_index_var.value());
  Node* initial_map = assembler.LoadObjectField(
      constructor, JSFunction::kPrototypeOrInitialMapOffset);
  Node* js_value = assembler.Allocate(JSValue::kSize);
  assembler.StoreMapNoWriteBarrier(js_value, initial_map);
  assembler.StoreObjectFieldRoot(js_value, JSValue::kPropertiesOffset,
                                 Heap::kEmptyFixedArrayRootIndex);
  assembler.StoreObjectFieldRoot(js_value, JSObject::kElementsOffset,
                                 Heap::kEmptyFixedArrayRootIndex);
  assembler.StoreObjectField(js_value, JSValue::kValueOffset, object);
  assembler.Return(js_value);

  assembler.Bind(&if_noconstructor);
  assembler.TailCallRuntime(
      Runtime::kThrowUndefinedOrNullToObject, context,
      assembler.HeapConstant(
          assembler.factory()->NewStringFromAsciiChecked("ToObject", TENURED)));

  assembler.Bind(&if_jsreceiver);
  assembler.Return(object);
}

// ES6 section 12.5.5 typeof operator
void Builtins::Generate_Typeof(compiler::CodeAssemblerState* state) {
  typedef compiler::Node Node;
  typedef TypeofDescriptor Descriptor;
  CodeStubAssembler assembler(state);

  Node* object = assembler.Parameter(Descriptor::kObject);
  Node* context = assembler.Parameter(Descriptor::kContext);

  assembler.Return(assembler.Typeof(object, context));
}

}  // namespace internal
}  // namespace v8
