// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

class ConversionBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit ConversionBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  void Generate_NonPrimitiveToPrimitive(Node* context, Node* input,
                                        ToPrimitiveHint hint);

  void Generate_OrdinaryToPrimitive(Node* context, Node* input,
                                    OrdinaryToPrimitiveHint hint);
};

// ES6 section 7.1.1 ToPrimitive ( input [ , PreferredType ] )
void ConversionBuiltinsAssembler::Generate_NonPrimitiveToPrimitive(
    Node* context, Node* input, ToPrimitiveHint hint) {
  // Lookup the @@toPrimitive property on the {input}.
  Node* exotic_to_prim =
      GetProperty(context, input, factory()->to_primitive_symbol());

  // Check if {exotic_to_prim} is neither null nor undefined.
  Label ordinary_to_primitive(this);
  GotoIf(IsNullOrUndefined(exotic_to_prim), &ordinary_to_primitive);
  {
    // Invoke the {exotic_to_prim} method on the {input} with a string
    // representation of the {hint}.
    Callable callable =
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined);
    Node* hint_string = HeapConstant(factory()->ToPrimitiveHintString(hint));
    Node* result =
        CallJS(callable, context, exotic_to_prim, input, hint_string);

    // Verify that the {result} is actually a primitive.
    Label if_resultisprimitive(this),
        if_resultisnotprimitive(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(result), &if_resultisprimitive);
    Node* result_instance_type = LoadInstanceType(result);
    STATIC_ASSERT(FIRST_PRIMITIVE_TYPE == FIRST_TYPE);
    Branch(Int32LessThanOrEqual(result_instance_type,
                                Int32Constant(LAST_PRIMITIVE_TYPE)),
           &if_resultisprimitive, &if_resultisnotprimitive);

    BIND(&if_resultisprimitive);
    {
      // Just return the {result}.
      Return(result);
    }

    BIND(&if_resultisnotprimitive);
    {
      // Somehow the @@toPrimitive method on {input} didn't yield a primitive.
      TailCallRuntime(Runtime::kThrowCannotConvertToPrimitive, context);
    }
  }

  // Convert using the OrdinaryToPrimitive algorithm instead.
  BIND(&ordinary_to_primitive);
  {
    Callable callable = CodeFactory::OrdinaryToPrimitive(
        isolate(), (hint == ToPrimitiveHint::kString)
                       ? OrdinaryToPrimitiveHint::kString
                       : OrdinaryToPrimitiveHint::kNumber);
    TailCallStub(callable, context, input);
  }
}

TF_BUILTIN(NonPrimitiveToPrimitive_Default, ConversionBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Generate_NonPrimitiveToPrimitive(context, input, ToPrimitiveHint::kDefault);
}

TF_BUILTIN(NonPrimitiveToPrimitive_Number, ConversionBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Generate_NonPrimitiveToPrimitive(context, input, ToPrimitiveHint::kNumber);
}

TF_BUILTIN(NonPrimitiveToPrimitive_String, ConversionBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Generate_NonPrimitiveToPrimitive(context, input, ToPrimitiveHint::kString);
}

TF_BUILTIN(StringToNumber, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Return(StringToNumber(context, input));
}

TF_BUILTIN(ToName, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Return(ToName(context, input));
}

TF_BUILTIN(NonNumberToNumber, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Return(NonNumberToNumber(context, input));
}

TF_BUILTIN(NonNumberToNumeric, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Return(NonNumberToNumeric(context, input));
}

TF_BUILTIN(ToNumeric, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Return(Select(IsNumber(input), [=] { return input; },
                [=] { return NonNumberToNumeric(context, input); },
                MachineRepresentation::kTagged));
}

// ES6 section 7.1.3 ToNumber ( argument )
TF_BUILTIN(ToNumber, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Return(ToNumber(context, input));
}

// ES section #sec-tostring-applied-to-the-number-type
TF_BUILTIN(NumberToString, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Return(NumberToString(context, input));
}

// ES section #sec-tostring
TF_BUILTIN(ToString, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Return(ToString(context, input));
}

// 7.1.1.1 OrdinaryToPrimitive ( O, hint )
void ConversionBuiltinsAssembler::Generate_OrdinaryToPrimitive(
    Node* context, Node* input, OrdinaryToPrimitiveHint hint) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label return_result(this, &var_result);

  Handle<String> method_names[2];
  switch (hint) {
    case OrdinaryToPrimitiveHint::kNumber:
      method_names[0] = factory()->valueOf_string();
      method_names[1] = factory()->toString_string();
      break;
    case OrdinaryToPrimitiveHint::kString:
      method_names[0] = factory()->toString_string();
      method_names[1] = factory()->valueOf_string();
      break;
  }
  for (Handle<String> name : method_names) {
    // Lookup the {name} on the {input}.
    Node* method = GetProperty(context, input, name);

    // Check if the {method} is callable.
    Label if_methodiscallable(this),
        if_methodisnotcallable(this, Label::kDeferred);
    GotoIf(TaggedIsSmi(method), &if_methodisnotcallable);
    Node* method_map = LoadMap(method);
    Branch(IsCallableMap(method_map), &if_methodiscallable,
           &if_methodisnotcallable);

    BIND(&if_methodiscallable);
    {
      // Call the {method} on the {input}.
      Callable callable = CodeFactory::Call(
          isolate(), ConvertReceiverMode::kNotNullOrUndefined);
      Node* result = CallJS(callable, context, method, input);
      var_result.Bind(result);

      // Return the {result} if it is a primitive.
      GotoIf(TaggedIsSmi(result), &return_result);
      Node* result_instance_type = LoadInstanceType(result);
      STATIC_ASSERT(FIRST_PRIMITIVE_TYPE == FIRST_TYPE);
      GotoIf(Int32LessThanOrEqual(result_instance_type,
                                  Int32Constant(LAST_PRIMITIVE_TYPE)),
             &return_result);
    }

    // Just continue with the next {name} if the {method} is not callable.
    Goto(&if_methodisnotcallable);
    BIND(&if_methodisnotcallable);
  }

  TailCallRuntime(Runtime::kThrowCannotConvertToPrimitive, context);

  BIND(&return_result);
  Return(var_result.value());
}

TF_BUILTIN(OrdinaryToPrimitive_Number, ConversionBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);
  Generate_OrdinaryToPrimitive(context, input,
                               OrdinaryToPrimitiveHint::kNumber);
}

TF_BUILTIN(OrdinaryToPrimitive_String, ConversionBuiltinsAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);
  Generate_OrdinaryToPrimitive(context, input,
                               OrdinaryToPrimitiveHint::kString);
}

// ES6 section 7.1.2 ToBoolean ( argument )
TF_BUILTIN(ToBoolean, CodeStubAssembler) {
  Node* value = Parameter(Descriptor::kArgument);

  Label return_true(this), return_false(this);
  BranchIfToBooleanIsTrue(value, &return_true, &return_false);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

// ES6 section 7.1.2 ToBoolean ( argument )
// Requires parameter on stack so that it can be used as a continuation from a
// LAZY deopt.
TF_BUILTIN(ToBooleanLazyDeoptContinuation, CodeStubAssembler) {
  Node* value = Parameter(Descriptor::kArgument);

  Label return_true(this), return_false(this);
  BranchIfToBooleanIsTrue(value, &return_true, &return_false);

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&return_false);
  Return(FalseConstant());
}

TF_BUILTIN(ToLength, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);

  // We might need to loop once for ToNumber conversion.
  VARIABLE(var_len, MachineRepresentation::kTagged,
           Parameter(Descriptor::kArgument));
  Label loop(this, &var_len);
  Goto(&loop);
  BIND(&loop);
  {
    // Shared entry points.
    Label return_len(this), return_two53minus1(this, Label::kDeferred),
        return_zero(this, Label::kDeferred);

    // Load the current {len} value.
    Node* len = var_len.value();

    // Check if {len} is a positive Smi.
    GotoIf(TaggedIsPositiveSmi(len), &return_len);

    // Check if {len} is a (negative) Smi.
    GotoIf(TaggedIsSmi(len), &return_zero);

    // Check if {len} is a HeapNumber.
    Label if_lenisheapnumber(this),
        if_lenisnotheapnumber(this, Label::kDeferred);
    Branch(IsHeapNumber(len), &if_lenisheapnumber, &if_lenisnotheapnumber);

    BIND(&if_lenisheapnumber);
    {
      // Load the floating-point value of {len}.
      Node* len_value = LoadHeapNumberValue(len);

      // Check if {len} is not greater than zero.
      GotoIfNot(Float64GreaterThan(len_value, Float64Constant(0.0)),
                &return_zero);

      // Check if {len} is greater than or equal to 2^53-1.
      GotoIf(Float64GreaterThanOrEqual(len_value,
                                       Float64Constant(kMaxSafeInteger)),
             &return_two53minus1);

      // Round the {len} towards -Infinity.
      Node* value = Float64Floor(len_value);
      Node* result = ChangeFloat64ToTagged(value);
      Return(result);
    }

    BIND(&if_lenisnotheapnumber);
    {
      // Need to convert {len} to a Number first.
      var_len.Bind(CallBuiltin(Builtins::kNonNumberToNumber, context, len));
      Goto(&loop);
    }

    BIND(&return_len);
    Return(var_len.value());

    BIND(&return_two53minus1);
    Return(NumberConstant(kMaxSafeInteger));

    BIND(&return_zero);
    Return(SmiConstant(0));
  }
}

TF_BUILTIN(ToInteger, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* input = Parameter(Descriptor::kArgument);

  Return(ToInteger(context, input));
}

// ES6 section 7.1.13 ToObject (argument)
TF_BUILTIN(ToObject, CodeStubAssembler) {
  Label if_smi(this, Label::kDeferred), if_jsreceiver(this),
      if_noconstructor(this, Label::kDeferred), if_wrapjsvalue(this);

  Node* context = Parameter(Descriptor::kContext);
  Node* object = Parameter(Descriptor::kArgument);

  VARIABLE(constructor_function_index_var,
           MachineType::PointerRepresentation());

  GotoIf(TaggedIsSmi(object), &if_smi);

  Node* map = LoadMap(object);
  Node* instance_type = LoadMapInstanceType(map);
  GotoIf(IsJSReceiverInstanceType(instance_type), &if_jsreceiver);

  Node* constructor_function_index = LoadMapConstructorFunctionIndex(map);
  GotoIf(WordEqual(constructor_function_index,
                   IntPtrConstant(Map::kNoConstructorFunctionIndex)),
         &if_noconstructor);
  constructor_function_index_var.Bind(constructor_function_index);
  Goto(&if_wrapjsvalue);

  BIND(&if_smi);
  constructor_function_index_var.Bind(
      IntPtrConstant(Context::NUMBER_FUNCTION_INDEX));
  Goto(&if_wrapjsvalue);

  BIND(&if_wrapjsvalue);
  Node* native_context = LoadNativeContext(context);
  Node* constructor = LoadFixedArrayElement(
      native_context, constructor_function_index_var.value());
  Node* initial_map =
      LoadObjectField(constructor, JSFunction::kPrototypeOrInitialMapOffset);
  Node* js_value = Allocate(JSValue::kSize);
  StoreMapNoWriteBarrier(js_value, initial_map);
  StoreObjectFieldRoot(js_value, JSValue::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(js_value, JSObject::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectField(js_value, JSValue::kValueOffset, object);
  Return(js_value);

  BIND(&if_noconstructor);
  TailCallRuntime(Runtime::kThrowUndefinedOrNullToObject, context,
                  StringConstant("ToObject"));

  BIND(&if_jsreceiver);
  Return(object);
}

// Deprecated ES5 [[Class]] internal property (used to implement %_ClassOf).
TF_BUILTIN(ClassOf, CodeStubAssembler) {
  Node* object = Parameter(TypeofDescriptor::kObject);

  Return(ClassOf(object));
}

// ES6 section 12.5.5 typeof operator
TF_BUILTIN(Typeof, CodeStubAssembler) {
  Node* object = Parameter(TypeofDescriptor::kObject);

  Return(Typeof(object));
}

}  // namespace internal
}  // namespace v8
