// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"
#include "src/code-factory.h"

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
    assembler->GotoIf(assembler->WordIsSmi(result), &if_resultisprimitive);
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
    CodeStubAssembler* assembler) {
  Generate_NonPrimitiveToPrimitive(assembler, ToPrimitiveHint::kDefault);
}

void Builtins::Generate_NonPrimitiveToPrimitive_Number(
    CodeStubAssembler* assembler) {
  Generate_NonPrimitiveToPrimitive(assembler, ToPrimitiveHint::kNumber);
}

void Builtins::Generate_NonPrimitiveToPrimitive_String(
    CodeStubAssembler* assembler) {
  Generate_NonPrimitiveToPrimitive(assembler, ToPrimitiveHint::kString);
}

void Builtins::Generate_StringToNumber(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;

  Node* input = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label runtime(assembler);

  // Check if string has a cached array index.
  Node* hash = assembler->LoadNameHashField(input);
  Node* bit = assembler->Word32And(
      hash, assembler->Int32Constant(String::kContainsCachedArrayIndexMask));
  assembler->GotoIf(assembler->Word32NotEqual(bit, assembler->Int32Constant(0)),
                    &runtime);

  assembler->Return(assembler->SmiTag(
      assembler->BitFieldDecode<String::ArrayIndexValueBits>(hash)));

  assembler->Bind(&runtime);
  {
    // Note: We cannot tail call to the runtime here, as js-to-wasm
    // trampolines also use this code currently, and they declare all
    // outgoing parameters as untagged, while we would push a tagged
    // object here.
    Node* result =
        assembler->CallRuntime(Runtime::kStringToNumber, context, input);
    assembler->Return(result);
  }
}

// ES6 section 7.1.3 ToNumber ( argument )
void Builtins::Generate_NonNumberToNumber(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;
  typedef TypeConversionDescriptor Descriptor;

  Node* input = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);

  // We might need to loop once here due to ToPrimitive conversions.
  Variable var_input(assembler, MachineRepresentation::kTagged);
  Label loop(assembler, &var_input);
  var_input.Bind(input);
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {input} value (known to be a HeapObject).
    Node* input = var_input.value();

    // Dispatch on the {input} instance type.
    Node* input_instance_type = assembler->LoadInstanceType(input);
    Label if_inputisstring(assembler), if_inputisoddball(assembler),
        if_inputisreceiver(assembler, Label::kDeferred),
        if_inputisother(assembler, Label::kDeferred);
    assembler->GotoIf(assembler->Int32LessThan(
                          input_instance_type,
                          assembler->Int32Constant(FIRST_NONSTRING_TYPE)),
                      &if_inputisstring);
    assembler->GotoIf(
        assembler->Word32Equal(input_instance_type,
                               assembler->Int32Constant(ODDBALL_TYPE)),
        &if_inputisoddball);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    assembler->Branch(assembler->Int32GreaterThanOrEqual(
                          input_instance_type,
                          assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE)),
                      &if_inputisreceiver, &if_inputisother);

    assembler->Bind(&if_inputisstring);
    {
      // The {input} is a String, use the fast stub to convert it to a Number.
      // TODO(bmeurer): Consider inlining the StringToNumber logic here.
      Callable callable = CodeFactory::StringToNumber(assembler->isolate());
      assembler->TailCallStub(callable, context, input);
    }

    assembler->Bind(&if_inputisoddball);
    {
      // The {input} is an Oddball, we just need to the Number value of it.
      Node* result =
          assembler->LoadObjectField(input, Oddball::kToNumberOffset);
      assembler->Return(result);
    }

    assembler->Bind(&if_inputisreceiver);
    {
      // The {input} is a JSReceiver, we need to convert it to a Primitive first
      // using the ToPrimitive type conversion, preferably yielding a Number.
      Callable callable = CodeFactory::NonPrimitiveToPrimitive(
          assembler->isolate(), ToPrimitiveHint::kNumber);
      Node* result = assembler->CallStub(callable, context, input);

      // Check if the {result} is already a Number.
      Label if_resultisnumber(assembler), if_resultisnotnumber(assembler);
      assembler->GotoIf(assembler->WordIsSmi(result), &if_resultisnumber);
      Node* result_map = assembler->LoadMap(result);
      assembler->Branch(
          assembler->WordEqual(result_map, assembler->HeapNumberMapConstant()),
          &if_resultisnumber, &if_resultisnotnumber);

      assembler->Bind(&if_resultisnumber);
      {
        // The ToPrimitive conversion already gave us a Number, so we're done.
        assembler->Return(result);
      }

      assembler->Bind(&if_resultisnotnumber);
      {
        // We now have a Primitive {result}, but it's not yet a Number.
        var_input.Bind(result);
        assembler->Goto(&loop);
      }
    }

    assembler->Bind(&if_inputisother);
    {
      // The {input} is something else (i.e. Symbol or Simd128Value), let the
      // runtime figure out the correct exception.
      // Note: We cannot tail call to the runtime here, as js-to-wasm
      // trampolines also use this code currently, and they declare all
      // outgoing parameters as untagged, while we would push a tagged
      // object here.
      Node* result = assembler->CallRuntime(Runtime::kToNumber, context, input);
      assembler->Return(result);
    }
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
    assembler->GotoIf(assembler->WordIsSmi(method), &if_methodisnotcallable);
    Node* method_map = assembler->LoadMap(method);
    Node* method_bit_field = assembler->LoadMapBitField(method_map);
    assembler->Branch(
        assembler->Word32Equal(
            assembler->Word32And(method_bit_field, assembler->Int32Constant(
                                                       1 << Map::kIsCallable)),
            assembler->Int32Constant(0)),
        &if_methodisnotcallable, &if_methodiscallable);

    assembler->Bind(&if_methodiscallable);
    {
      // Call the {method} on the {input}.
      Callable callable = CodeFactory::Call(assembler->isolate());
      Node* result = assembler->CallJS(callable, context, method, input);
      var_result.Bind(result);

      // Return the {result} if it is a primitive.
      assembler->GotoIf(assembler->WordIsSmi(result), &return_result);
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
    CodeStubAssembler* assembler) {
  Generate_OrdinaryToPrimitive(assembler, OrdinaryToPrimitiveHint::kNumber);
}

void Builtins::Generate_OrdinaryToPrimitive_String(
    CodeStubAssembler* assembler) {
  Generate_OrdinaryToPrimitive(assembler, OrdinaryToPrimitiveHint::kString);
}

// ES6 section 7.1.2 ToBoolean ( argument )
void Builtins::Generate_ToBoolean(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef TypeConversionDescriptor Descriptor;

  Node* value = assembler->Parameter(Descriptor::kArgument);

  Label return_true(assembler), return_false(assembler);
  assembler->BranchIfToBooleanIsTrue(value, &return_true, &return_false);

  assembler->Bind(&return_true);
  assembler->Return(assembler->BooleanConstant(true));

  assembler->Bind(&return_false);
  assembler->Return(assembler->BooleanConstant(false));
}

}  // namespace internal
}  // namespace v8
