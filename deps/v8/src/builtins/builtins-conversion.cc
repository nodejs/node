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
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;

  Node* input = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(assembler->StringToNumber(context, input));
}

void Builtins::Generate_ToName(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;

  Node* input = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(assembler->ToName(context, input));
}

// static
void Builtins::Generate_NonNumberToNumber(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;

  Node* input = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(assembler->NonNumberToNumber(context, input));
}

// ES6 section 7.1.3 ToNumber ( argument )
void Builtins::Generate_ToNumber(CodeStubAssembler* assembler) {
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;

  Node* input = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(assembler->ToNumber(context, input));
}

void Builtins::Generate_ToString(CodeStubAssembler* assembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef TypeConversionDescriptor Descriptor;

  Node* input = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label is_number(assembler);
  Label runtime(assembler);

  assembler->GotoIf(assembler->WordIsSmi(input), &is_number);

  Node* input_map = assembler->LoadMap(input);
  Node* input_instance_type = assembler->LoadMapInstanceType(input_map);

  Label not_string(assembler);
  assembler->GotoUnless(assembler->IsStringInstanceType(input_instance_type),
                        &not_string);
  assembler->Return(input);

  Label not_heap_number(assembler);

  assembler->Bind(&not_string);
  {
    assembler->GotoUnless(
        assembler->WordEqual(input_map, assembler->HeapNumberMapConstant()),
        &not_heap_number);
    assembler->Goto(&is_number);
  }

  assembler->Bind(&is_number);
  {
    // TODO(tebbi): inline as soon as NumberToString is in the CodeStubAssembler
    Callable callable = CodeFactory::NumberToString(assembler->isolate());
    assembler->Return(assembler->CallStub(callable, context, input));
  }

  assembler->Bind(&not_heap_number);
  {
    assembler->GotoIf(
        assembler->Word32NotEqual(input_instance_type,
                                  assembler->Int32Constant(ODDBALL_TYPE)),
        &runtime);
    assembler->Return(
        assembler->LoadObjectField(input, Oddball::kToStringOffset));
  }

  assembler->Bind(&runtime);
  {
    assembler->Return(
        assembler->CallRuntime(Runtime::kToString, context, input));
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
