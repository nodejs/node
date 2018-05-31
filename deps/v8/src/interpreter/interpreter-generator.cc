// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-generator.h"

#include <array>
#include <tuple>

#include "src/builtins/builtins-arguments-gen.h"
#include "src/builtins/builtins-constructor-gen.h"
#include "src/code-events.h"
#include "src/code-factory.h"
#include "src/debug/debug.h"
#include "src/ic/accessor-assembler.h"
#include "src/ic/binary-op-assembler.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-assembler.h"
#include "src/interpreter/interpreter-intrinsics-generator.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

namespace {

using compiler::Node;
typedef CodeStubAssembler::Label Label;
typedef CodeStubAssembler::Variable Variable;

#define IGNITION_HANDLER(Name, BaseAssembler)                         \
  class Name##Assembler : public BaseAssembler {                      \
   public:                                                            \
    explicit Name##Assembler(compiler::CodeAssemblerState* state,     \
                             Bytecode bytecode, OperandScale scale)   \
        : BaseAssembler(state, bytecode, scale) {}                    \
    static void Generate(compiler::CodeAssemblerState* state,         \
                         OperandScale scale);                         \
                                                                      \
   private:                                                           \
    void GenerateImpl();                                              \
    DISALLOW_COPY_AND_ASSIGN(Name##Assembler);                        \
  };                                                                  \
  void Name##Assembler::Generate(compiler::CodeAssemblerState* state, \
                                 OperandScale scale) {                \
    Name##Assembler assembler(state, Bytecode::k##Name, scale);       \
    state->SetInitialDebugInformation(#Name, __FILE__, __LINE__);     \
    assembler.GenerateImpl();                                         \
  }                                                                   \
  void Name##Assembler::GenerateImpl()

// LdaZero
//
// Load literal '0' into the accumulator.
IGNITION_HANDLER(LdaZero, InterpreterAssembler) {
  Node* zero_value = NumberConstant(0.0);
  SetAccumulator(zero_value);
  Dispatch();
}

// LdaSmi <imm>
//
// Load an integer literal into the accumulator as a Smi.
IGNITION_HANDLER(LdaSmi, InterpreterAssembler) {
  Node* smi_int = BytecodeOperandImmSmi(0);
  SetAccumulator(smi_int);
  Dispatch();
}

// LdaConstant <idx>
//
// Load constant literal at |idx| in the constant pool into the accumulator.
IGNITION_HANDLER(LdaConstant, InterpreterAssembler) {
  Node* constant = LoadConstantPoolEntryAtOperandIndex(0);
  SetAccumulator(constant);
  Dispatch();
}

// LdaUndefined
//
// Load Undefined into the accumulator.
IGNITION_HANDLER(LdaUndefined, InterpreterAssembler) {
  SetAccumulator(UndefinedConstant());
  Dispatch();
}

// LdaNull
//
// Load Null into the accumulator.
IGNITION_HANDLER(LdaNull, InterpreterAssembler) {
  SetAccumulator(NullConstant());
  Dispatch();
}

// LdaTheHole
//
// Load TheHole into the accumulator.
IGNITION_HANDLER(LdaTheHole, InterpreterAssembler) {
  SetAccumulator(TheHoleConstant());
  Dispatch();
}

// LdaTrue
//
// Load True into the accumulator.
IGNITION_HANDLER(LdaTrue, InterpreterAssembler) {
  SetAccumulator(TrueConstant());
  Dispatch();
}

// LdaFalse
//
// Load False into the accumulator.
IGNITION_HANDLER(LdaFalse, InterpreterAssembler) {
  SetAccumulator(FalseConstant());
  Dispatch();
}

// Ldar <src>
//
// Load accumulator with value from register <src>.
IGNITION_HANDLER(Ldar, InterpreterAssembler) {
  Node* value = LoadRegisterAtOperandIndex(0);
  SetAccumulator(value);
  Dispatch();
}

// Star <dst>
//
// Store accumulator to register <dst>.
IGNITION_HANDLER(Star, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  StoreRegisterAtOperandIndex(accumulator, 0);
  Dispatch();
}

// Mov <src> <dst>
//
// Stores the value of register <src> to register <dst>.
IGNITION_HANDLER(Mov, InterpreterAssembler) {
  Node* src_value = LoadRegisterAtOperandIndex(0);
  StoreRegisterAtOperandIndex(src_value, 1);
  Dispatch();
}

class InterpreterLoadGlobalAssembler : public InterpreterAssembler {
 public:
  InterpreterLoadGlobalAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                 OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void LdaGlobal(int slot_operand_index, int name_operand_index,
                 TypeofMode typeof_mode) {
    TNode<FeedbackVector> feedback_vector = CAST(LoadFeedbackVector());
    Node* feedback_slot = BytecodeOperandIdx(slot_operand_index);

    AccessorAssembler accessor_asm(state());
    ExitPoint exit_point(this, [=](Node* result) {
      SetAccumulator(result);
      Dispatch();
    });

    LazyNode<Context> lazy_context = [=] { return CAST(GetContext()); };

    LazyNode<Name> lazy_name = [=] {
      Node* name = LoadConstantPoolEntryAtOperandIndex(name_operand_index);
      return CAST(name);
    };

    accessor_asm.LoadGlobalIC(feedback_vector, feedback_slot, lazy_context,
                              lazy_name, typeof_mode, &exit_point,
                              CodeStubAssembler::INTPTR_PARAMETERS);
  }
};

// LdaGlobal <name_index> <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> outside of a typeof.
IGNITION_HANDLER(LdaGlobal, InterpreterLoadGlobalAssembler) {
  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  LdaGlobal(kSlotOperandIndex, kNameOperandIndex, NOT_INSIDE_TYPEOF);
}

// LdaGlobalInsideTypeof <name_index> <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> inside of a typeof.
IGNITION_HANDLER(LdaGlobalInsideTypeof, InterpreterLoadGlobalAssembler) {
  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  LdaGlobal(kSlotOperandIndex, kNameOperandIndex, INSIDE_TYPEOF);
}

// StaGlobal <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot>.
IGNITION_HANDLER(StaGlobal, InterpreterAssembler) {
  Node* context = GetContext();

  // Store the global via the StoreGlobalIC.
  Node* name = LoadConstantPoolEntryAtOperandIndex(0);
  Node* value = GetAccumulator();
  Node* raw_slot = BytecodeOperandIdx(1);
  Node* smi_slot = SmiTag(raw_slot);
  Node* feedback_vector = LoadFeedbackVector();
  CallBuiltin(Builtins::kStoreGlobalIC, context, name, value, smi_slot,
              feedback_vector);
  Dispatch();
}

// LdaContextSlot <context> <slot_index> <depth>
//
// Load the object in |slot_index| of the context at |depth| in the context
// chain starting at |context| into the accumulator.
IGNITION_HANDLER(LdaContextSlot, InterpreterAssembler) {
  Node* context = LoadRegisterAtOperandIndex(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* depth = BytecodeOperandUImm(2);
  Node* slot_context = GetContextAtDepth(context, depth);
  Node* result = LoadContextElement(slot_context, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// LdaImmutableContextSlot <context> <slot_index> <depth>
//
// Load the object in |slot_index| of the context at |depth| in the context
// chain starting at |context| into the accumulator.
IGNITION_HANDLER(LdaImmutableContextSlot, InterpreterAssembler) {
  Node* context = LoadRegisterAtOperandIndex(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* depth = BytecodeOperandUImm(2);
  Node* slot_context = GetContextAtDepth(context, depth);
  Node* result = LoadContextElement(slot_context, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// LdaCurrentContextSlot <slot_index>
//
// Load the object in |slot_index| of the current context into the accumulator.
IGNITION_HANDLER(LdaCurrentContextSlot, InterpreterAssembler) {
  Node* slot_index = BytecodeOperandIdx(0);
  Node* slot_context = GetContext();
  Node* result = LoadContextElement(slot_context, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// LdaImmutableCurrentContextSlot <slot_index>
//
// Load the object in |slot_index| of the current context into the accumulator.
IGNITION_HANDLER(LdaImmutableCurrentContextSlot, InterpreterAssembler) {
  Node* slot_index = BytecodeOperandIdx(0);
  Node* slot_context = GetContext();
  Node* result = LoadContextElement(slot_context, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// StaContextSlot <context> <slot_index> <depth>
//
// Stores the object in the accumulator into |slot_index| of the context at
// |depth| in the context chain starting at |context|.
IGNITION_HANDLER(StaContextSlot, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* context = LoadRegisterAtOperandIndex(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* depth = BytecodeOperandUImm(2);
  Node* slot_context = GetContextAtDepth(context, depth);
  StoreContextElement(slot_context, slot_index, value);
  Dispatch();
}

// StaCurrentContextSlot <slot_index>
//
// Stores the object in the accumulator into |slot_index| of the current
// context.
IGNITION_HANDLER(StaCurrentContextSlot, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* slot_index = BytecodeOperandIdx(0);
  Node* slot_context = GetContext();
  StoreContextElement(slot_context, slot_index, value);
  Dispatch();
}

// LdaLookupSlot <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
IGNITION_HANDLER(LdaLookupSlot, InterpreterAssembler) {
  Node* name = LoadConstantPoolEntryAtOperandIndex(0);
  Node* context = GetContext();
  Node* result = CallRuntime(Runtime::kLoadLookupSlot, context, name);
  SetAccumulator(result);
  Dispatch();
}

// LdaLookupSlotInsideTypeof <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
IGNITION_HANDLER(LdaLookupSlotInsideTypeof, InterpreterAssembler) {
  Node* name = LoadConstantPoolEntryAtOperandIndex(0);
  Node* context = GetContext();
  Node* result =
      CallRuntime(Runtime::kLoadLookupSlotInsideTypeof, context, name);
  SetAccumulator(result);
  Dispatch();
}

class InterpreterLookupContextSlotAssembler : public InterpreterAssembler {
 public:
  InterpreterLookupContextSlotAssembler(CodeAssemblerState* state,
                                        Bytecode bytecode,
                                        OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void LookupContextSlot(Runtime::FunctionId function_id) {
    Node* context = GetContext();
    Node* slot_index = BytecodeOperandIdx(1);
    Node* depth = BytecodeOperandUImm(2);

    Label slowpath(this, Label::kDeferred);

    // Check for context extensions to allow the fast path.
    GotoIfHasContextExtensionUpToDepth(context, depth, &slowpath);

    // Fast path does a normal load context.
    {
      Node* slot_context = GetContextAtDepth(context, depth);
      Node* result = LoadContextElement(slot_context, slot_index);
      SetAccumulator(result);
      Dispatch();
    }

    // Slow path when we have to call out to the runtime.
    BIND(&slowpath);
    {
      Node* name = LoadConstantPoolEntryAtOperandIndex(0);
      Node* result = CallRuntime(function_id, context, name);
      SetAccumulator(result);
      Dispatch();
    }
  }
};

// LdaLookupSlot <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
IGNITION_HANDLER(LdaLookupContextSlot, InterpreterLookupContextSlotAssembler) {
  LookupContextSlot(Runtime::kLoadLookupSlot);
}

// LdaLookupSlotInsideTypeof <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
IGNITION_HANDLER(LdaLookupContextSlotInsideTypeof,
                 InterpreterLookupContextSlotAssembler) {
  LookupContextSlot(Runtime::kLoadLookupSlotInsideTypeof);
}

class InterpreterLookupGlobalAssembler : public InterpreterLoadGlobalAssembler {
 public:
  InterpreterLookupGlobalAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                   OperandScale operand_scale)
      : InterpreterLoadGlobalAssembler(state, bytecode, operand_scale) {}

  void LookupGlobalSlot(Runtime::FunctionId function_id) {
    Node* context = GetContext();
    Node* depth = BytecodeOperandUImm(2);

    Label slowpath(this, Label::kDeferred);

    // Check for context extensions to allow the fast path
    GotoIfHasContextExtensionUpToDepth(context, depth, &slowpath);

    // Fast path does a normal load global
    {
      static const int kNameOperandIndex = 0;
      static const int kSlotOperandIndex = 1;

      TypeofMode typeof_mode =
          function_id == Runtime::kLoadLookupSlotInsideTypeof
              ? INSIDE_TYPEOF
              : NOT_INSIDE_TYPEOF;

      LdaGlobal(kSlotOperandIndex, kNameOperandIndex, typeof_mode);
    }

    // Slow path when we have to call out to the runtime
    BIND(&slowpath);
    {
      Node* name = LoadConstantPoolEntryAtOperandIndex(0);
      Node* result = CallRuntime(function_id, context, name);
      SetAccumulator(result);
      Dispatch();
    }
  }
};

// LdaLookupGlobalSlot <name_index> <feedback_slot> <depth>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
IGNITION_HANDLER(LdaLookupGlobalSlot, InterpreterLookupGlobalAssembler) {
  LookupGlobalSlot(Runtime::kLoadLookupSlot);
}

// LdaLookupGlobalSlotInsideTypeof <name_index> <feedback_slot> <depth>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
IGNITION_HANDLER(LdaLookupGlobalSlotInsideTypeof,
                 InterpreterLookupGlobalAssembler) {
  LookupGlobalSlot(Runtime::kLoadLookupSlotInsideTypeof);
}

// StaLookupSlotSloppy <name_index> <flags>
//
// Store the object in accumulator to the object with the name in constant
// pool entry |name_index|.
IGNITION_HANDLER(StaLookupSlot, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* name = LoadConstantPoolEntryAtOperandIndex(0);
  Node* bytecode_flags = BytecodeOperandFlag(1);
  Node* context = GetContext();
  Variable var_result(this, MachineRepresentation::kTagged);

  Label sloppy(this), strict(this), end(this);
  DCHECK_EQ(0, LanguageMode::kSloppy);
  DCHECK_EQ(1, LanguageMode::kStrict);
  DCHECK_EQ(0, static_cast<int>(LookupHoistingMode::kNormal));
  DCHECK_EQ(1, static_cast<int>(LookupHoistingMode::kLegacySloppy));
  Branch(IsSetWord32<StoreLookupSlotFlags::LanguageModeBit>(bytecode_flags),
         &strict, &sloppy);

  BIND(&strict);
  {
    CSA_ASSERT(this, IsClearWord32<StoreLookupSlotFlags::LookupHoistingModeBit>(
                         bytecode_flags));
    var_result.Bind(
        CallRuntime(Runtime::kStoreLookupSlot_Strict, context, name, value));
    Goto(&end);
  }

  BIND(&sloppy);
  {
    Label hoisting(this), ordinary(this);
    Branch(IsSetWord32<StoreLookupSlotFlags::LookupHoistingModeBit>(
               bytecode_flags),
           &hoisting, &ordinary);

    BIND(&hoisting);
    {
      var_result.Bind(CallRuntime(Runtime::kStoreLookupSlot_SloppyHoisting,
                                  context, name, value));
      Goto(&end);
    }

    BIND(&ordinary);
    {
      var_result.Bind(
          CallRuntime(Runtime::kStoreLookupSlot_Sloppy, context, name, value));
      Goto(&end);
    }
  }

  BIND(&end);
  {
    SetAccumulator(var_result.value());
    Dispatch();
  }
}

// LdaNamedProperty <object> <name_index> <slot>
//
// Calls the LoadIC at FeedBackVector slot <slot> for <object> and the name at
// constant pool entry <name_index>.
IGNITION_HANDLER(LdaNamedProperty, InterpreterAssembler) {
  Node* feedback_vector = LoadFeedbackVector();
  Node* feedback_slot = BytecodeOperandIdx(2);
  Node* smi_slot = SmiTag(feedback_slot);

  // Load receiver.
  Node* recv = LoadRegisterAtOperandIndex(0);

  // Load the name.
  // TODO(jgruber): Not needed for monomorphic smi handler constant/field case.
  Node* name = LoadConstantPoolEntryAtOperandIndex(1);
  Node* context = GetContext();

  Label done(this);
  Variable var_result(this, MachineRepresentation::kTagged);
  ExitPoint exit_point(this, &done, &var_result);

  AccessorAssembler::LoadICParameters params(context, recv, name, smi_slot,
                                             feedback_vector);
  AccessorAssembler accessor_asm(state());
  accessor_asm.LoadIC_BytecodeHandler(&params, &exit_point);

  BIND(&done);
  {
    SetAccumulator(var_result.value());
    Dispatch();
  }
}

// KeyedLoadIC <object> <slot>
//
// Calls the KeyedLoadIC at FeedBackVector slot <slot> for <object> and the key
// in the accumulator.
IGNITION_HANDLER(LdaKeyedProperty, InterpreterAssembler) {
  Node* object = LoadRegisterAtOperandIndex(0);
  Node* name = GetAccumulator();
  Node* raw_slot = BytecodeOperandIdx(1);
  Node* smi_slot = SmiTag(raw_slot);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();
  Node* result = CallBuiltin(Builtins::kKeyedLoadIC, context, object, name,
                             smi_slot, feedback_vector);
  SetAccumulator(result);
  Dispatch();
}

class InterpreterStoreNamedPropertyAssembler : public InterpreterAssembler {
 public:
  InterpreterStoreNamedPropertyAssembler(CodeAssemblerState* state,
                                         Bytecode bytecode,
                                         OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void StaNamedProperty(Callable ic) {
    Node* code_target = HeapConstant(ic.code());
    Node* object = LoadRegisterAtOperandIndex(0);
    Node* name = LoadConstantPoolEntryAtOperandIndex(1);
    Node* value = GetAccumulator();
    Node* raw_slot = BytecodeOperandIdx(2);
    Node* smi_slot = SmiTag(raw_slot);
    Node* feedback_vector = LoadFeedbackVector();
    Node* context = GetContext();
    Node* result = CallStub(ic.descriptor(), code_target, context, object, name,
                            value, smi_slot, feedback_vector);
    // To avoid special logic in the deoptimizer to re-materialize the value in
    // the accumulator, we overwrite the accumulator after the IC call. It
    // doesn't really matter what we write to the accumulator here, since we
    // restore to the correct value on the outside. Storing the result means we
    // don't need to keep unnecessary state alive across the callstub.
    SetAccumulator(result);
    Dispatch();
  }
};

// StaNamedProperty <object> <name_index> <slot>
//
// Calls the StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
IGNITION_HANDLER(StaNamedProperty, InterpreterStoreNamedPropertyAssembler) {
  Callable ic = Builtins::CallableFor(isolate(), Builtins::kStoreIC);
  StaNamedProperty(ic);
}

// StaNamedOwnProperty <object> <name_index> <slot>
//
// Calls the StoreOwnIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
IGNITION_HANDLER(StaNamedOwnProperty, InterpreterStoreNamedPropertyAssembler) {
  Callable ic = CodeFactory::StoreOwnICInOptimizedCode(isolate());
  StaNamedProperty(ic);
}

// StaKeyedProperty <object> <key> <slot>
//
// Calls the KeyedStoreIC at FeedbackVector slot <slot> for <object> and
// the key <key> with the value in the accumulator.
IGNITION_HANDLER(StaKeyedProperty, InterpreterAssembler) {
  Node* object = LoadRegisterAtOperandIndex(0);
  Node* name = LoadRegisterAtOperandIndex(1);
  Node* value = GetAccumulator();
  Node* raw_slot = BytecodeOperandIdx(2);
  Node* smi_slot = SmiTag(raw_slot);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();
  Node* result = CallBuiltin(Builtins::kKeyedStoreIC, context, object, name,
                             value, smi_slot, feedback_vector);
  // To avoid special logic in the deoptimizer to re-materialize the value in
  // the accumulator, we overwrite the accumulator after the IC call. It
  // doesn't really matter what we write to the accumulator here, since we
  // restore to the correct value on the outside. Storing the result means we
  // don't need to keep unnecessary state alive across the callstub.
  SetAccumulator(result);
  Dispatch();
}

// StaInArrayLiteral <array> <index> <slot>
//
// Calls the StoreInArrayLiteralIC at FeedbackVector slot <slot> for <array> and
// the key <index> with the value in the accumulator.
IGNITION_HANDLER(StaInArrayLiteral, InterpreterAssembler) {
  Node* array = LoadRegisterAtOperandIndex(0);
  Node* index = LoadRegisterAtOperandIndex(1);
  Node* value = GetAccumulator();
  Node* raw_slot = BytecodeOperandIdx(2);
  Node* smi_slot = SmiTag(raw_slot);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();
  Node* result = CallBuiltin(Builtins::kStoreInArrayLiteralIC, context, array,
                             index, value, smi_slot, feedback_vector);
  // To avoid special logic in the deoptimizer to re-materialize the value in
  // the accumulator, we overwrite the accumulator after the IC call. It
  // doesn't really matter what we write to the accumulator here, since we
  // restore to the correct value on the outside. Storing the result means we
  // don't need to keep unnecessary state alive across the callstub.
  SetAccumulator(result);
  Dispatch();
}

// StaDataPropertyInLiteral <object> <name> <flags>
//
// Define a property <name> with value from the accumulator in <object>.
// Property attributes and whether set_function_name are stored in
// DataPropertyInLiteralFlags <flags>.
//
// This definition is not observable and is used only for definitions
// in object or class literals.
IGNITION_HANDLER(StaDataPropertyInLiteral, InterpreterAssembler) {
  Node* object = LoadRegisterAtOperandIndex(0);
  Node* name = LoadRegisterAtOperandIndex(1);
  Node* value = GetAccumulator();
  Node* flags = SmiFromInt32(BytecodeOperandFlag(2));
  Node* vector_index = SmiTag(BytecodeOperandIdx(3));

  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  CallRuntime(Runtime::kDefineDataPropertyInLiteral, context, object, name,
              value, flags, feedback_vector, vector_index);
  Dispatch();
}

IGNITION_HANDLER(CollectTypeProfile, InterpreterAssembler) {
  Node* position = BytecodeOperandImmSmi(0);
  Node* value = GetAccumulator();

  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  CallRuntime(Runtime::kCollectTypeProfile, context, position, value,
              feedback_vector);
  Dispatch();
}

// LdaModuleVariable <cell_index> <depth>
//
// Load the contents of a module variable into the accumulator.  The variable is
// identified by <cell_index>.  <depth> is the depth of the current context
// relative to the module context.
IGNITION_HANDLER(LdaModuleVariable, InterpreterAssembler) {
  Node* cell_index = BytecodeOperandImmIntPtr(0);
  Node* depth = BytecodeOperandUImm(1);

  Node* module_context = GetContextAtDepth(GetContext(), depth);
  Node* module = LoadContextElement(module_context, Context::EXTENSION_INDEX);

  Label if_export(this), if_import(this), end(this);
  Branch(IntPtrGreaterThan(cell_index, IntPtrConstant(0)), &if_export,
         &if_import);

  BIND(&if_export);
  {
    Node* regular_exports =
        LoadObjectField(module, Module::kRegularExportsOffset);
    // The actual array index is (cell_index - 1).
    Node* export_index = IntPtrSub(cell_index, IntPtrConstant(1));
    Node* cell = LoadFixedArrayElement(regular_exports, export_index);
    SetAccumulator(LoadObjectField(cell, Cell::kValueOffset));
    Goto(&end);
  }

  BIND(&if_import);
  {
    Node* regular_imports =
        LoadObjectField(module, Module::kRegularImportsOffset);
    // The actual array index is (-cell_index - 1).
    Node* import_index = IntPtrSub(IntPtrConstant(-1), cell_index);
    Node* cell = LoadFixedArrayElement(regular_imports, import_index);
    SetAccumulator(LoadObjectField(cell, Cell::kValueOffset));
    Goto(&end);
  }

  BIND(&end);
  Dispatch();
}

// StaModuleVariable <cell_index> <depth>
//
// Store accumulator to the module variable identified by <cell_index>.
// <depth> is the depth of the current context relative to the module context.
IGNITION_HANDLER(StaModuleVariable, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* cell_index = BytecodeOperandImmIntPtr(0);
  Node* depth = BytecodeOperandUImm(1);

  Node* module_context = GetContextAtDepth(GetContext(), depth);
  Node* module = LoadContextElement(module_context, Context::EXTENSION_INDEX);

  Label if_export(this), if_import(this), end(this);
  Branch(IntPtrGreaterThan(cell_index, IntPtrConstant(0)), &if_export,
         &if_import);

  BIND(&if_export);
  {
    Node* regular_exports =
        LoadObjectField(module, Module::kRegularExportsOffset);
    // The actual array index is (cell_index - 1).
    Node* export_index = IntPtrSub(cell_index, IntPtrConstant(1));
    Node* cell = LoadFixedArrayElement(regular_exports, export_index);
    StoreObjectField(cell, Cell::kValueOffset, value);
    Goto(&end);
  }

  BIND(&if_import);
  {
    // Not supported (probably never).
    Abort(AbortReason::kUnsupportedModuleOperation);
    Goto(&end);
  }

  BIND(&end);
  Dispatch();
}

// PushContext <context>
//
// Saves the current context in <context>, and pushes the accumulator as the
// new current context.
IGNITION_HANDLER(PushContext, InterpreterAssembler) {
  Node* new_context = GetAccumulator();
  Node* old_context = GetContext();
  StoreRegisterAtOperandIndex(old_context, 0);
  SetContext(new_context);
  Dispatch();
}

// PopContext <context>
//
// Pops the current context and sets <context> as the new context.
IGNITION_HANDLER(PopContext, InterpreterAssembler) {
  Node* context = LoadRegisterAtOperandIndex(0);
  SetContext(context);
  Dispatch();
}

class InterpreterBinaryOpAssembler : public InterpreterAssembler {
 public:
  InterpreterBinaryOpAssembler(CodeAssemblerState* state, Bytecode bytecode,
                               OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  typedef Node* (BinaryOpAssembler::*BinaryOpGenerator)(Node* context,
                                                        Node* left, Node* right,
                                                        Node* slot,
                                                        Node* vector,
                                                        bool lhs_is_smi);

  void BinaryOpWithFeedback(BinaryOpGenerator generator) {
    Node* lhs = LoadRegisterAtOperandIndex(0);
    Node* rhs = GetAccumulator();
    Node* context = GetContext();
    Node* slot_index = BytecodeOperandIdx(1);
    Node* feedback_vector = LoadFeedbackVector();

    BinaryOpAssembler binop_asm(state());
    Node* result = (binop_asm.*generator)(context, lhs, rhs, slot_index,
                                          feedback_vector, false);
    SetAccumulator(result);
    Dispatch();
  }

  void BinaryOpSmiWithFeedback(BinaryOpGenerator generator) {
    Node* lhs = GetAccumulator();
    Node* rhs = BytecodeOperandImmSmi(0);
    Node* context = GetContext();
    Node* slot_index = BytecodeOperandIdx(1);
    Node* feedback_vector = LoadFeedbackVector();

    BinaryOpAssembler binop_asm(state());
    Node* result = (binop_asm.*generator)(context, lhs, rhs, slot_index,
                                          feedback_vector, true);
    SetAccumulator(result);
    Dispatch();
  }
};

// Add <src>
//
// Add register <src> to accumulator.
IGNITION_HANDLER(Add, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_AddWithFeedback);
}

// Sub <src>
//
// Subtract register <src> from accumulator.
IGNITION_HANDLER(Sub, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_SubtractWithFeedback);
}

// Mul <src>
//
// Multiply accumulator by register <src>.
IGNITION_HANDLER(Mul, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_MultiplyWithFeedback);
}

// Div <src>
//
// Divide register <src> by accumulator.
IGNITION_HANDLER(Div, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_DivideWithFeedback);
}

// Mod <src>
//
// Modulo register <src> by accumulator.
IGNITION_HANDLER(Mod, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_ModulusWithFeedback);
}

// Exp <src>
//
// Exponentiate register <src> (base) with accumulator (exponent).
IGNITION_HANDLER(Exp, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_ExponentiateWithFeedback);
}

// AddSmi <imm>
//
// Adds an immediate value <imm> to the value in the accumulator.
IGNITION_HANDLER(AddSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_AddWithFeedback);
}

// SubSmi <imm>
//
// Subtracts an immediate value <imm> from the value in the accumulator.
IGNITION_HANDLER(SubSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_SubtractWithFeedback);
}

// MulSmi <imm>
//
// Multiplies an immediate value <imm> to the value in the accumulator.
IGNITION_HANDLER(MulSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_MultiplyWithFeedback);
}

// DivSmi <imm>
//
// Divides the value in the accumulator by immediate value <imm>.
IGNITION_HANDLER(DivSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_DivideWithFeedback);
}

// ModSmi <imm>
//
// Modulo accumulator by immediate value <imm>.
IGNITION_HANDLER(ModSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_ModulusWithFeedback);
}

// ExpSmi <imm>
//
// Exponentiate accumulator (base) with immediate value <imm> (exponent).
IGNITION_HANDLER(ExpSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(
      &BinaryOpAssembler::Generate_ExponentiateWithFeedback);
}

class InterpreterBitwiseBinaryOpAssembler : public InterpreterAssembler {
 public:
  InterpreterBitwiseBinaryOpAssembler(CodeAssemblerState* state,
                                      Bytecode bytecode,
                                      OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void BitwiseBinaryOpWithFeedback(Operation bitwise_op) {
    Node* left = LoadRegisterAtOperandIndex(0);
    Node* right = GetAccumulator();
    Node* context = GetContext();
    Node* slot_index = BytecodeOperandIdx(1);
    Node* feedback_vector = LoadFeedbackVector();

    VARIABLE(var_left_feedback, MachineRepresentation::kTaggedSigned);
    VARIABLE(var_right_feedback, MachineRepresentation::kTaggedSigned);
    VARIABLE(var_left_word32, MachineRepresentation::kWord32);
    VARIABLE(var_right_word32, MachineRepresentation::kWord32);
    VARIABLE(var_left_bigint, MachineRepresentation::kTagged, left);
    VARIABLE(var_right_bigint, MachineRepresentation::kTagged);
    Label if_left_number(this), do_number_op(this);
    Label if_left_bigint(this), do_bigint_op(this);

    TaggedToWord32OrBigIntWithFeedback(context, left, &if_left_number,
                                       &var_left_word32, &if_left_bigint,
                                       &var_left_bigint, &var_left_feedback);
    BIND(&if_left_number);
    TaggedToWord32OrBigIntWithFeedback(context, right, &do_number_op,
                                       &var_right_word32, &do_bigint_op,
                                       &var_right_bigint, &var_right_feedback);
    BIND(&do_number_op);
    Node* result = BitwiseOp(var_left_word32.value(), var_right_word32.value(),
                             bitwise_op);
    Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                          BinaryOperationFeedback::kSignedSmall,
                                          BinaryOperationFeedback::kNumber);
    Node* input_feedback =
        SmiOr(var_left_feedback.value(), var_right_feedback.value());
    UpdateFeedback(SmiOr(result_type, input_feedback), feedback_vector,
                   slot_index);
    SetAccumulator(result);
    Dispatch();

    // BigInt cases.
    BIND(&if_left_bigint);
    TaggedToNumericWithFeedback(context, right, &do_bigint_op,
                                &var_right_bigint, &var_right_feedback);

    BIND(&do_bigint_op);
    SetAccumulator(
        CallRuntime(Runtime::kBigIntBinaryOp, context, var_left_bigint.value(),
                    var_right_bigint.value(), SmiConstant(bitwise_op)));
    UpdateFeedback(SmiOr(var_left_feedback.value(), var_right_feedback.value()),
                   feedback_vector, slot_index);
    Dispatch();
  }

  void BitwiseBinaryOpWithSmi(Operation bitwise_op) {
    Node* left = GetAccumulator();
    Node* right = BytecodeOperandImmSmi(0);
    Node* slot_index = BytecodeOperandIdx(1);
    Node* feedback_vector = LoadFeedbackVector();
    Node* context = GetContext();

    VARIABLE(var_left_feedback, MachineRepresentation::kTaggedSigned);
    VARIABLE(var_left_word32, MachineRepresentation::kWord32);
    VARIABLE(var_left_bigint, MachineRepresentation::kTagged);
    Label do_smi_op(this), if_bigint_mix(this);

    TaggedToWord32OrBigIntWithFeedback(context, left, &do_smi_op,
                                       &var_left_word32, &if_bigint_mix,
                                       &var_left_bigint, &var_left_feedback);
    BIND(&do_smi_op);
    Node* result =
        BitwiseOp(var_left_word32.value(), SmiToInt32(right), bitwise_op);
    Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                          BinaryOperationFeedback::kSignedSmall,
                                          BinaryOperationFeedback::kNumber);
    UpdateFeedback(SmiOr(result_type, var_left_feedback.value()),
                   feedback_vector, slot_index);
    SetAccumulator(result);
    Dispatch();

    BIND(&if_bigint_mix);
    UpdateFeedback(var_left_feedback.value(), feedback_vector, slot_index);
    ThrowTypeError(context, MessageTemplate::kBigIntMixedTypes);
  }
};

// BitwiseOr <src>
//
// BitwiseOr register <src> to accumulator.
IGNITION_HANDLER(BitwiseOr, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kBitwiseOr);
}

// BitwiseXor <src>
//
// BitwiseXor register <src> to accumulator.
IGNITION_HANDLER(BitwiseXor, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kBitwiseXor);
}

// BitwiseAnd <src>
//
// BitwiseAnd register <src> to accumulator.
IGNITION_HANDLER(BitwiseAnd, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kBitwiseAnd);
}

// ShiftLeft <src>
//
// Left shifts register <src> by the count specified in the accumulator.
// Register <src> is converted to an int32 and the accumulator to uint32
// before the operation. 5 lsb bits from the accumulator are used as count
// i.e. <src> << (accumulator & 0x1F).
IGNITION_HANDLER(ShiftLeft, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kShiftLeft);
}

// ShiftRight <src>
//
// Right shifts register <src> by the count specified in the accumulator.
// Result is sign extended. Register <src> is converted to an int32 and the
// accumulator to uint32 before the operation. 5 lsb bits from the accumulator
// are used as count i.e. <src> >> (accumulator & 0x1F).
IGNITION_HANDLER(ShiftRight, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kShiftRight);
}

// ShiftRightLogical <src>
//
// Right Shifts register <src> by the count specified in the accumulator.
// Result is zero-filled. The accumulator and register <src> are converted to
// uint32 before the operation 5 lsb bits from the accumulator are used as
// count i.e. <src> << (accumulator & 0x1F).
IGNITION_HANDLER(ShiftRightLogical, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kShiftRightLogical);
}

// BitwiseOrSmi <imm>
//
// BitwiseOrSmi accumulator with <imm>.
IGNITION_HANDLER(BitwiseOrSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kBitwiseOr);
}

// BitwiseXorSmi <imm>
//
// BitwiseXorSmi accumulator with <imm>.
IGNITION_HANDLER(BitwiseXorSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kBitwiseXor);
}

// BitwiseAndSmi <imm>
//
// BitwiseAndSmi accumulator with <imm>.
IGNITION_HANDLER(BitwiseAndSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kBitwiseAnd);
}

// BitwiseNot <feedback_slot>
//
// Perform bitwise-not on the accumulator.
IGNITION_HANDLER(BitwiseNot, InterpreterAssembler) {
  Node* operand = GetAccumulator();
  Node* slot_index = BytecodeOperandIdx(0);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  VARIABLE(var_word32, MachineRepresentation::kWord32);
  VARIABLE(var_feedback, MachineRepresentation::kTaggedSigned);
  VARIABLE(var_bigint, MachineRepresentation::kTagged);
  Label if_number(this), if_bigint(this);
  TaggedToWord32OrBigIntWithFeedback(context, operand, &if_number, &var_word32,
                                     &if_bigint, &var_bigint, &var_feedback);

  // Number case.
  BIND(&if_number);
  Node* result = ChangeInt32ToTagged(Signed(Word32Not(var_word32.value())));
  Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                        BinaryOperationFeedback::kSignedSmall,
                                        BinaryOperationFeedback::kNumber);
  UpdateFeedback(SmiOr(result_type, var_feedback.value()), feedback_vector,
                 slot_index);
  SetAccumulator(result);
  Dispatch();

  // BigInt case.
  BIND(&if_bigint);
  UpdateFeedback(SmiConstant(BinaryOperationFeedback::kBigInt), feedback_vector,
                 slot_index);
  SetAccumulator(CallRuntime(Runtime::kBigIntUnaryOp, context,
                             var_bigint.value(),
                             SmiConstant(Operation::kBitwiseNot)));
  Dispatch();
}

// ShiftLeftSmi <imm>
//
// Left shifts accumulator by the count specified in <imm>.
// The accumulator is converted to an int32 before the operation. The 5
// lsb bits from <imm> are used as count i.e. <src> << (<imm> & 0x1F).
IGNITION_HANDLER(ShiftLeftSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kShiftLeft);
}

// ShiftRightSmi <imm>
//
// Right shifts accumulator by the count specified in <imm>. Result is sign
// extended. The accumulator is converted to an int32 before the operation. The
// 5 lsb bits from <imm> are used as count i.e. <src> >> (<imm> & 0x1F).
IGNITION_HANDLER(ShiftRightSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kShiftRight);
}

// ShiftRightLogicalSmi <imm>
//
// Right shifts accumulator by the count specified in <imm>. Result is zero
// extended. The accumulator is converted to an int32 before the operation. The
// 5 lsb bits from <imm> are used as count i.e. <src> >>> (<imm> & 0x1F).
IGNITION_HANDLER(ShiftRightLogicalSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kShiftRightLogical);
}

class UnaryNumericOpAssembler : public InterpreterAssembler {
 public:
  UnaryNumericOpAssembler(CodeAssemblerState* state, Bytecode bytecode,
                          OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  virtual ~UnaryNumericOpAssembler() {}

  // Must return a tagged value.
  virtual Node* SmiOp(Node* smi_value, Variable* var_feedback,
                      Label* do_float_op, Variable* var_float) = 0;
  // Must return a Float64 value.
  virtual Node* FloatOp(Node* float_value) = 0;
  // Must return a tagged value.
  virtual Node* BigIntOp(Node* bigint_value) = 0;

  void UnaryOpWithFeedback() {
    VARIABLE(var_value, MachineRepresentation::kTagged, GetAccumulator());
    Node* slot_index = BytecodeOperandIdx(0);
    Node* feedback_vector = LoadFeedbackVector();

    VARIABLE(var_result, MachineRepresentation::kTagged);
    VARIABLE(var_float_value, MachineRepresentation::kFloat64);
    VARIABLE(var_feedback, MachineRepresentation::kTaggedSigned,
             SmiConstant(BinaryOperationFeedback::kNone));
    Variable* loop_vars[] = {&var_value, &var_feedback};
    Label start(this, arraysize(loop_vars), loop_vars), end(this);
    Label do_float_op(this, &var_float_value);
    Goto(&start);
    // We might have to try again after ToNumeric conversion.
    BIND(&start);
    {
      Label if_smi(this), if_heapnumber(this), if_bigint(this);
      Label if_oddball(this), if_other(this);
      Node* value = var_value.value();
      GotoIf(TaggedIsSmi(value), &if_smi);
      Node* map = LoadMap(value);
      GotoIf(IsHeapNumberMap(map), &if_heapnumber);
      Node* instance_type = LoadMapInstanceType(map);
      GotoIf(IsBigIntInstanceType(instance_type), &if_bigint);
      Branch(InstanceTypeEqual(instance_type, ODDBALL_TYPE), &if_oddball,
             &if_other);

      BIND(&if_smi);
      {
        var_result.Bind(
            SmiOp(value, &var_feedback, &do_float_op, &var_float_value));
        Goto(&end);
      }

      BIND(&if_heapnumber);
      {
        var_float_value.Bind(LoadHeapNumberValue(value));
        Goto(&do_float_op);
      }

      BIND(&if_bigint);
      {
        var_result.Bind(BigIntOp(value));
        CombineFeedback(&var_feedback, BinaryOperationFeedback::kBigInt);
        Goto(&end);
      }

      BIND(&if_oddball);
      {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a number, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(this, SmiEqual(var_feedback.value(),
                                  SmiConstant(BinaryOperationFeedback::kNone)));
        OverwriteFeedback(&var_feedback,
                          BinaryOperationFeedback::kNumberOrOddball);
        var_value.Bind(LoadObjectField(value, Oddball::kToNumberOffset));
        Goto(&start);
      }

      BIND(&if_other);
      {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a number, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(this, SmiEqual(var_feedback.value(),
                                  SmiConstant(BinaryOperationFeedback::kNone)));
        OverwriteFeedback(&var_feedback, BinaryOperationFeedback::kAny);
        var_value.Bind(
            CallBuiltin(Builtins::kNonNumberToNumeric, GetContext(), value));
        Goto(&start);
      }
    }

    BIND(&do_float_op);
    {
      CombineFeedback(&var_feedback, BinaryOperationFeedback::kNumber);
      var_result.Bind(
          AllocateHeapNumberWithValue(FloatOp(var_float_value.value())));
      Goto(&end);
    }

    BIND(&end);
    UpdateFeedback(var_feedback.value(), feedback_vector, slot_index);
    SetAccumulator(var_result.value());
    Dispatch();
  }
};

class NegateAssemblerImpl : public UnaryNumericOpAssembler {
 public:
  explicit NegateAssemblerImpl(CodeAssemblerState* state, Bytecode bytecode,
                               OperandScale operand_scale)
      : UnaryNumericOpAssembler(state, bytecode, operand_scale) {}

  Node* SmiOp(Node* smi_value, Variable* var_feedback, Label* do_float_op,
              Variable* var_float) override {
    VARIABLE(var_result, MachineRepresentation::kTagged);
    Label if_zero(this), if_min_smi(this), end(this);
    // Return -0 if operand is 0.
    GotoIf(SmiEqual(smi_value, SmiConstant(0)), &if_zero);

    // Special-case the minimum Smi to avoid overflow.
    GotoIf(SmiEqual(smi_value, SmiConstant(Smi::kMinValue)), &if_min_smi);

    // Else simply subtract operand from 0.
    CombineFeedback(var_feedback, BinaryOperationFeedback::kSignedSmall);
    var_result.Bind(SmiSub(SmiConstant(0), smi_value));
    Goto(&end);

    BIND(&if_zero);
    CombineFeedback(var_feedback, BinaryOperationFeedback::kNumber);
    var_result.Bind(MinusZeroConstant());
    Goto(&end);

    BIND(&if_min_smi);
    var_float->Bind(SmiToFloat64(smi_value));
    Goto(do_float_op);

    BIND(&end);
    return var_result.value();
  }

  Node* FloatOp(Node* float_value) override { return Float64Neg(float_value); }

  Node* BigIntOp(Node* bigint_value) override {
    return CallRuntime(Runtime::kBigIntUnaryOp, GetContext(), bigint_value,
                       SmiConstant(Operation::kNegate));
  }
};

// Negate <feedback_slot>
//
// Perform arithmetic negation on the accumulator.
IGNITION_HANDLER(Negate, NegateAssemblerImpl) { UnaryOpWithFeedback(); }

// ToName <dst>
//
// Convert the object referenced by the accumulator to a name.
IGNITION_HANDLER(ToName, InterpreterAssembler) {
  Node* object = GetAccumulator();
  Node* context = GetContext();
  Node* result = ToName(context, object);
  StoreRegisterAtOperandIndex(result, 0);
  Dispatch();
}

// ToNumber <slot>
//
// Convert the object referenced by the accumulator to a number.
IGNITION_HANDLER(ToNumber, InterpreterAssembler) {
  ToNumberOrNumeric(Object::Conversion::kToNumber);
}

// ToNumeric <slot>
//
// Convert the object referenced by the accumulator to a numeric.
IGNITION_HANDLER(ToNumeric, InterpreterAssembler) {
  ToNumberOrNumeric(Object::Conversion::kToNumeric);
}

// ToObject <dst>
//
// Convert the object referenced by the accumulator to a JSReceiver.
IGNITION_HANDLER(ToObject, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* context = GetContext();
  Node* result = CallBuiltin(Builtins::kToObject, context, accumulator);
  StoreRegisterAtOperandIndex(result, 0);
  Dispatch();
}

// ToString
//
// Convert the accumulator to a String.
IGNITION_HANDLER(ToString, InterpreterAssembler) {
  SetAccumulator(ToString_Inline(GetContext(), GetAccumulator()));
  Dispatch();
}

class IncDecAssembler : public UnaryNumericOpAssembler {
 public:
  explicit IncDecAssembler(CodeAssemblerState* state, Bytecode bytecode,
                           OperandScale operand_scale)
      : UnaryNumericOpAssembler(state, bytecode, operand_scale) {}

  Operation op() {
    DCHECK(op_ == Operation::kIncrement || op_ == Operation::kDecrement);
    return op_;
  }

  Node* SmiOp(Node* smi_value, Variable* var_feedback, Label* do_float_op,
              Variable* var_float) override {
    // Try fast Smi operation first.
    Node* value = BitcastTaggedToWord(smi_value);
    Node* one = BitcastTaggedToWord(SmiConstant(1));
    Node* pair = op() == Operation::kIncrement
                     ? IntPtrAddWithOverflow(value, one)
                     : IntPtrSubWithOverflow(value, one);
    Node* overflow = Projection(1, pair);

    // Check if the Smi operation overflowed.
    Label if_overflow(this), if_notoverflow(this);
    Branch(overflow, &if_overflow, &if_notoverflow);

    BIND(&if_overflow);
    {
      var_float->Bind(SmiToFloat64(smi_value));
      Goto(do_float_op);
    }

    BIND(&if_notoverflow);
    CombineFeedback(var_feedback, BinaryOperationFeedback::kSignedSmall);
    return BitcastWordToTaggedSigned(Projection(0, pair));
  }

  Node* FloatOp(Node* float_value) override {
    return op() == Operation::kIncrement
               ? Float64Add(float_value, Float64Constant(1.0))
               : Float64Sub(float_value, Float64Constant(1.0));
  }

  Node* BigIntOp(Node* bigint_value) override {
    return CallRuntime(Runtime::kBigIntUnaryOp, GetContext(), bigint_value,
                       SmiConstant(op()));
  }

  void IncWithFeedback() {
    op_ = Operation::kIncrement;
    UnaryOpWithFeedback();
  }

  void DecWithFeedback() {
    op_ = Operation::kDecrement;
    UnaryOpWithFeedback();
  }

 private:
  Operation op_ = Operation::kEqual;  // Dummy initialization.
};

// Inc
//
// Increments value in the accumulator by one.
IGNITION_HANDLER(Inc, IncDecAssembler) { IncWithFeedback(); }

// Dec
//
// Decrements value in the accumulator by one.
IGNITION_HANDLER(Dec, IncDecAssembler) { DecWithFeedback(); }

// LogicalNot
//
// Perform logical-not on the accumulator, first casting the
// accumulator to a boolean value if required.
// ToBooleanLogicalNot
IGNITION_HANDLER(ToBooleanLogicalNot, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Variable result(this, MachineRepresentation::kTagged);
  Label if_true(this), if_false(this), end(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  {
    result.Bind(FalseConstant());
    Goto(&end);
  }
  BIND(&if_false);
  {
    result.Bind(TrueConstant());
    Goto(&end);
  }
  BIND(&end);
  SetAccumulator(result.value());
  Dispatch();
}

// LogicalNot
//
// Perform logical-not on the accumulator, which must already be a boolean
// value.
IGNITION_HANDLER(LogicalNot, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Variable result(this, MachineRepresentation::kTagged);
  Label if_true(this), if_false(this), end(this);
  Node* true_value = TrueConstant();
  Node* false_value = FalseConstant();
  Branch(WordEqual(value, true_value), &if_true, &if_false);
  BIND(&if_true);
  {
    result.Bind(false_value);
    Goto(&end);
  }
  BIND(&if_false);
  {
    CSA_ASSERT(this, WordEqual(value, false_value));
    result.Bind(true_value);
    Goto(&end);
  }
  BIND(&end);
  SetAccumulator(result.value());
  Dispatch();
}

// TypeOf
//
// Load the accumulator with the string representating type of the
// object in the accumulator.
IGNITION_HANDLER(TypeOf, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* result = Typeof(value);
  SetAccumulator(result);
  Dispatch();
}

// DeletePropertyStrict
//
// Delete the property specified in the accumulator from the object
// referenced by the register operand following strict mode semantics.
IGNITION_HANDLER(DeletePropertyStrict, InterpreterAssembler) {
  Node* object = LoadRegisterAtOperandIndex(0);
  Node* key = GetAccumulator();
  Node* context = GetContext();
  Node* result = CallBuiltin(Builtins::kDeleteProperty, context, object, key,
                             SmiConstant(Smi::FromEnum(LanguageMode::kStrict)));
  SetAccumulator(result);
  Dispatch();
}

// DeletePropertySloppy
//
// Delete the property specified in the accumulator from the object
// referenced by the register operand following sloppy mode semantics.
IGNITION_HANDLER(DeletePropertySloppy, InterpreterAssembler) {
  Node* object = LoadRegisterAtOperandIndex(0);
  Node* key = GetAccumulator();
  Node* context = GetContext();
  Node* result = CallBuiltin(Builtins::kDeleteProperty, context, object, key,
                             SmiConstant(Smi::FromEnum(LanguageMode::kSloppy)));
  SetAccumulator(result);
  Dispatch();
}

// GetSuperConstructor
//
// Get the super constructor from the object referenced by the accumulator.
// The result is stored in register |reg|.
IGNITION_HANDLER(GetSuperConstructor, InterpreterAssembler) {
  Node* active_function = GetAccumulator();
  Node* context = GetContext();
  Node* result = GetSuperConstructor(context, active_function);
  StoreRegisterAtOperandIndex(result, 0);
  Dispatch();
}

class InterpreterJSCallAssembler : public InterpreterAssembler {
 public:
  InterpreterJSCallAssembler(CodeAssemblerState* state, Bytecode bytecode,
                             OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  // Generates code to perform a JS call that collects type feedback.
  void JSCall(ConvertReceiverMode receiver_mode) {
    Node* function = LoadRegisterAtOperandIndex(0);
    RegListNodePair args = GetRegisterListAtOperandIndex(1);
    Node* slot_id = BytecodeOperandIdx(3);
    Node* feedback_vector = LoadFeedbackVector();
    Node* context = GetContext();

    // Collect the {function} feedback.
    CollectCallFeedback(function, context, feedback_vector, slot_id);

    // Call the function and dispatch to the next handler.
    CallJSAndDispatch(function, context, args, receiver_mode);
  }

  // Generates code to perform a JS call with a known number of arguments that
  // collects type feedback.
  void JSCallN(int arg_count, ConvertReceiverMode receiver_mode) {
    // Indices and counts of operands on the bytecode.
    const int kFirstArgumentOperandIndex = 1;
    const int kReceiverOperandCount =
        (receiver_mode == ConvertReceiverMode::kNullOrUndefined) ? 0 : 1;
    const int kRecieverAndArgOperandCount = kReceiverOperandCount + arg_count;
    const int kSlotOperandIndex =
        kFirstArgumentOperandIndex + kRecieverAndArgOperandCount;

    Node* function = LoadRegisterAtOperandIndex(0);
    Node* slot_id = BytecodeOperandIdx(kSlotOperandIndex);
    Node* feedback_vector = LoadFeedbackVector();
    Node* context = GetContext();

    // Collect the {function} feedback.
    CollectCallFeedback(function, context, feedback_vector, slot_id);

    switch (kRecieverAndArgOperandCount) {
      case 0:
        CallJSAndDispatch(function, context, Int32Constant(arg_count),
                          receiver_mode);
        break;
      case 1:
        CallJSAndDispatch(
            function, context, Int32Constant(arg_count), receiver_mode,
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex));
        break;
      case 2:
        CallJSAndDispatch(
            function, context, Int32Constant(arg_count), receiver_mode,
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex),
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex + 1));
        break;
      case 3:
        CallJSAndDispatch(
            function, context, Int32Constant(arg_count), receiver_mode,
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex),
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex + 1),
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex + 2));
        break;
      default:
        UNREACHABLE();
    }
  }
};

// Call <callable> <receiver> <arg_count> <feedback_slot_id>
//
// Call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers. Collect type feedback
// into |feedback_slot_id|
IGNITION_HANDLER(CallAnyReceiver, InterpreterJSCallAssembler) {
  JSCall(ConvertReceiverMode::kAny);
}

IGNITION_HANDLER(CallProperty, InterpreterJSCallAssembler) {
  JSCall(ConvertReceiverMode::kNotNullOrUndefined);
}

IGNITION_HANDLER(CallProperty0, InterpreterJSCallAssembler) {
  JSCallN(0, ConvertReceiverMode::kNotNullOrUndefined);
}

IGNITION_HANDLER(CallProperty1, InterpreterJSCallAssembler) {
  JSCallN(1, ConvertReceiverMode::kNotNullOrUndefined);
}

IGNITION_HANDLER(CallProperty2, InterpreterJSCallAssembler) {
  JSCallN(2, ConvertReceiverMode::kNotNullOrUndefined);
}

IGNITION_HANDLER(CallUndefinedReceiver, InterpreterJSCallAssembler) {
  JSCall(ConvertReceiverMode::kNullOrUndefined);
}

IGNITION_HANDLER(CallUndefinedReceiver0, InterpreterJSCallAssembler) {
  JSCallN(0, ConvertReceiverMode::kNullOrUndefined);
}

IGNITION_HANDLER(CallUndefinedReceiver1, InterpreterJSCallAssembler) {
  JSCallN(1, ConvertReceiverMode::kNullOrUndefined);
}

IGNITION_HANDLER(CallUndefinedReceiver2, InterpreterJSCallAssembler) {
  JSCallN(2, ConvertReceiverMode::kNullOrUndefined);
}

// CallRuntime <function_id> <first_arg> <arg_count>
//
// Call the runtime function |function_id| with the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers.
IGNITION_HANDLER(CallRuntime, InterpreterAssembler) {
  Node* function_id = BytecodeOperandRuntimeId(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  Node* context = GetContext();
  Node* result = CallRuntimeN(function_id, context, args);
  SetAccumulator(result);
  Dispatch();
}

// InvokeIntrinsic <function_id> <first_arg> <arg_count>
//
// Implements the semantic equivalent of calling the runtime function
// |function_id| with the first argument in |first_arg| and |arg_count|
// arguments in subsequent registers.
IGNITION_HANDLER(InvokeIntrinsic, InterpreterAssembler) {
  Node* function_id = BytecodeOperandIntrinsicId(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  Node* context = GetContext();
  Node* result = GenerateInvokeIntrinsic(this, function_id, context, args);
  SetAccumulator(result);
  Dispatch();
}

// CallRuntimeForPair <function_id> <first_arg> <arg_count> <first_return>
//
// Call the runtime function |function_id| which returns a pair, with the
// first argument in register |first_arg| and |arg_count| arguments in
// subsequent registers. Returns the result in <first_return> and
// <first_return + 1>
IGNITION_HANDLER(CallRuntimeForPair, InterpreterAssembler) {
  // Call the runtime function.
  Node* function_id = BytecodeOperandRuntimeId(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  Node* context = GetContext();
  Node* result_pair = CallRuntimeN(function_id, context, args, 2);
  // Store the results in <first_return> and <first_return + 1>
  Node* result0 = Projection(0, result_pair);
  Node* result1 = Projection(1, result_pair);
  StoreRegisterPairAtOperandIndex(result0, result1, 3);
  Dispatch();
}

// CallJSRuntime <context_index> <receiver> <arg_count>
//
// Call the JS runtime function that has the |context_index| with the receiver
// in register |receiver| and |arg_count| arguments in subsequent registers.
IGNITION_HANDLER(CallJSRuntime, InterpreterAssembler) {
  Node* context_index = BytecodeOperandNativeContextIndex(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);

  // Get the function to call from the native context.
  Node* context = GetContext();
  Node* native_context = LoadNativeContext(context);
  Node* function = LoadContextElement(native_context, context_index);

  // Call the function.
  CallJSAndDispatch(function, context, args,
                    ConvertReceiverMode::kNullOrUndefined);
}

// CallWithSpread <callable> <first_arg> <arg_count>
//
// Call a JSfunction or Callable in |callable| with the receiver in
// |first_arg| and |arg_count - 1| arguments in subsequent registers. The
// final argument is always a spread.
//
IGNITION_HANDLER(CallWithSpread, InterpreterAssembler) {
  Node* callable = LoadRegisterAtOperandIndex(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  Node* slot_id = BytecodeOperandIdx(3);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  // Call into Runtime function CallWithSpread which does everything.
  CallJSWithSpreadAndDispatch(callable, context, args, slot_id,
                              feedback_vector);
}

// ConstructWithSpread <first_arg> <arg_count>
//
// Call the constructor in |constructor| with the first argument in register
// |first_arg| and |arg_count| arguments in subsequent registers. The final
// argument is always a spread. The new.target is in the accumulator.
//
IGNITION_HANDLER(ConstructWithSpread, InterpreterAssembler) {
  Node* new_target = GetAccumulator();
  Node* constructor = LoadRegisterAtOperandIndex(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  Node* slot_id = BytecodeOperandIdx(3);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();
  Node* result = ConstructWithSpread(constructor, context, new_target, args,
                                     slot_id, feedback_vector);
  SetAccumulator(result);
  Dispatch();
}

// Construct <constructor> <first_arg> <arg_count>
//
// Call operator construct with |constructor| and the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers. The new.target is in the accumulator.
//
IGNITION_HANDLER(Construct, InterpreterAssembler) {
  Node* new_target = GetAccumulator();
  Node* constructor = LoadRegisterAtOperandIndex(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  Node* slot_id = BytecodeOperandIdx(3);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();
  Node* result = Construct(constructor, context, new_target, args, slot_id,
                           feedback_vector);
  SetAccumulator(result);
  Dispatch();
}

class InterpreterCompareOpAssembler : public InterpreterAssembler {
 public:
  InterpreterCompareOpAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void CompareOpWithFeedback(Operation compare_op) {
    Node* lhs = LoadRegisterAtOperandIndex(0);
    Node* rhs = GetAccumulator();
    Node* context = GetContext();

    Variable var_type_feedback(this, MachineRepresentation::kTagged);
    Node* result;
    switch (compare_op) {
      case Operation::kEqual:
        result = Equal(lhs, rhs, context, &var_type_feedback);
        break;
      case Operation::kStrictEqual:
        result = StrictEqual(lhs, rhs, &var_type_feedback);
        break;
      case Operation::kLessThan:
      case Operation::kGreaterThan:
      case Operation::kLessThanOrEqual:
      case Operation::kGreaterThanOrEqual:
        result = RelationalComparison(compare_op, lhs, rhs, context,
                                      &var_type_feedback);
        break;
      default:
        UNREACHABLE();
    }

    Node* slot_index = BytecodeOperandIdx(1);
    Node* feedback_vector = LoadFeedbackVector();
    UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_index);
    SetAccumulator(result);
    Dispatch();
  }
};

// TestEqual <src>
//
// Test if the value in the <src> register equals the accumulator.
IGNITION_HANDLER(TestEqual, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kEqual);
}

// TestEqualStrict <src>
//
// Test if the value in the <src> register is strictly equal to the accumulator.
IGNITION_HANDLER(TestEqualStrict, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kStrictEqual);
}

// TestLessThan <src>
//
// Test if the value in the <src> register is less than the accumulator.
IGNITION_HANDLER(TestLessThan, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kLessThan);
}

// TestGreaterThan <src>
//
// Test if the value in the <src> register is greater than the accumulator.
IGNITION_HANDLER(TestGreaterThan, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kGreaterThan);
}

// TestLessThanOrEqual <src>
//
// Test if the value in the <src> register is less than or equal to the
// accumulator.
IGNITION_HANDLER(TestLessThanOrEqual, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kLessThanOrEqual);
}

// TestGreaterThanOrEqual <src>
//
// Test if the value in the <src> register is greater than or equal to the
// accumulator.
IGNITION_HANDLER(TestGreaterThanOrEqual, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kGreaterThanOrEqual);
}

// TestEqualStrictNoFeedback <src>
//
// Test if the value in the <src> register is strictly equal to the accumulator.
// Type feedback is not collected.
IGNITION_HANDLER(TestEqualStrictNoFeedback, InterpreterAssembler) {
  Node* lhs = LoadRegisterAtOperandIndex(0);
  Node* rhs = GetAccumulator();
  // TODO(5310): This is called only when lhs and rhs are Smis (for ex:
  // try-finally or generators) or strings (only when visiting
  // ClassLiteralProperties). We should be able to optimize this and not perform
  // the full strict equality.
  Node* result = StrictEqual(lhs, rhs);
  SetAccumulator(result);
  Dispatch();
}

// TestIn <src>
//
// Test if the object referenced by the register operand is a property of the
// object referenced by the accumulator.
IGNITION_HANDLER(TestIn, InterpreterAssembler) {
  Node* property = LoadRegisterAtOperandIndex(0);
  Node* object = GetAccumulator();
  Node* context = GetContext();

  SetAccumulator(HasProperty(object, property, context, kHasProperty));
  Dispatch();
}

// TestInstanceOf <src> <feedback_slot>
//
// Test if the object referenced by the <src> register is an an instance of type
// referenced by the accumulator.
IGNITION_HANDLER(TestInstanceOf, InterpreterAssembler) {
  Node* object = LoadRegisterAtOperandIndex(0);
  Node* callable = GetAccumulator();
  Node* slot_id = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  // Record feedback for the {callable} in the {feedback_vector}.
  CollectCallableFeedback(callable, context, feedback_vector, slot_id);

  // Perform the actual instanceof operation.
  SetAccumulator(InstanceOf(object, callable, context));
  Dispatch();
}

// TestUndetectable
//
// Test if the value in the accumulator is undetectable (null, undefined or
// document.all).
IGNITION_HANDLER(TestUndetectable, InterpreterAssembler) {
  Label return_false(this), end(this);
  Node* object = GetAccumulator();

  // If the object is an Smi then return false.
  SetAccumulator(FalseConstant());
  GotoIf(TaggedIsSmi(object), &end);

  // If it is a HeapObject, load the map and check for undetectable bit.
  Node* result = SelectBooleanConstant(IsUndetectableMap(LoadMap(object)));
  SetAccumulator(result);
  Goto(&end);

  BIND(&end);
  Dispatch();
}

// TestNull
//
// Test if the value in accumulator is strictly equal to null.
IGNITION_HANDLER(TestNull, InterpreterAssembler) {
  Node* object = GetAccumulator();
  Node* result = SelectBooleanConstant(WordEqual(object, NullConstant()));
  SetAccumulator(result);
  Dispatch();
}

// TestUndefined
//
// Test if the value in the accumulator is strictly equal to undefined.
IGNITION_HANDLER(TestUndefined, InterpreterAssembler) {
  Node* object = GetAccumulator();
  Node* result = SelectBooleanConstant(WordEqual(object, UndefinedConstant()));
  SetAccumulator(result);
  Dispatch();
}

// TestTypeOf <literal_flag>
//
// Tests if the object in the <accumulator> is typeof the literal represented
// by |literal_flag|.
IGNITION_HANDLER(TestTypeOf, InterpreterAssembler) {
  Node* object = GetAccumulator();
  Node* literal_flag = BytecodeOperandFlag(0);

#define MAKE_LABEL(name, lower_case) Label if_##lower_case(this);
  TYPEOF_LITERAL_LIST(MAKE_LABEL)
#undef MAKE_LABEL

#define LABEL_POINTER(name, lower_case) &if_##lower_case,
  Label* labels[] = {TYPEOF_LITERAL_LIST(LABEL_POINTER)};
#undef LABEL_POINTER

#define CASE(name, lower_case) \
  static_cast<int32_t>(TestTypeOfFlags::LiteralFlag::k##name),
  int32_t cases[] = {TYPEOF_LITERAL_LIST(CASE)};
#undef CASE

  Label if_true(this), if_false(this), end(this);

  // We juse use the final label as the default and properly CSA_ASSERT
  // that the {literal_flag} is valid here; this significantly improves
  // the generated code (compared to having a default label that aborts).
  unsigned const num_cases = arraysize(cases);
  CSA_ASSERT(this, Uint32LessThan(literal_flag, Int32Constant(num_cases)));
  Switch(literal_flag, labels[num_cases - 1], cases, labels, num_cases - 1);

  BIND(&if_number);
  {
    Comment("IfNumber");
    GotoIfNumber(object, &if_true);
    Goto(&if_false);
  }
  BIND(&if_string);
  {
    Comment("IfString");
    GotoIf(TaggedIsSmi(object), &if_false);
    Branch(IsString(object), &if_true, &if_false);
  }
  BIND(&if_symbol);
  {
    Comment("IfSymbol");
    GotoIf(TaggedIsSmi(object), &if_false);
    Branch(IsSymbol(object), &if_true, &if_false);
  }
  BIND(&if_boolean);
  {
    Comment("IfBoolean");
    GotoIf(WordEqual(object, TrueConstant()), &if_true);
    Branch(WordEqual(object, FalseConstant()), &if_true, &if_false);
  }
  BIND(&if_bigint);
  {
    Comment("IfBigInt");
    GotoIf(TaggedIsSmi(object), &if_false);
    Branch(IsBigInt(object), &if_true, &if_false);
  }
  BIND(&if_undefined);
  {
    Comment("IfUndefined");
    GotoIf(TaggedIsSmi(object), &if_false);
    // Check it is not null and the map has the undetectable bit set.
    GotoIf(IsNull(object), &if_false);
    Branch(IsUndetectableMap(LoadMap(object)), &if_true, &if_false);
  }
  BIND(&if_function);
  {
    Comment("IfFunction");
    GotoIf(TaggedIsSmi(object), &if_false);
    // Check if callable bit is set and not undetectable.
    Node* map_bitfield = LoadMapBitField(LoadMap(object));
    Node* callable_undetectable =
        Word32And(map_bitfield, Int32Constant(Map::IsUndetectableBit::kMask |
                                              Map::IsCallableBit::kMask));
    Branch(Word32Equal(callable_undetectable,
                       Int32Constant(Map::IsCallableBit::kMask)),
           &if_true, &if_false);
  }
  BIND(&if_object);
  {
    Comment("IfObject");
    GotoIf(TaggedIsSmi(object), &if_false);

    // If the object is null then return true.
    GotoIf(IsNull(object), &if_true);

    // Check if the object is a receiver type and is not undefined or callable.
    Node* map = LoadMap(object);
    GotoIfNot(IsJSReceiverMap(map), &if_false);
    Node* map_bitfield = LoadMapBitField(map);
    Node* callable_undetectable =
        Word32And(map_bitfield, Int32Constant(Map::IsUndetectableBit::kMask |
                                              Map::IsCallableBit::kMask));
    Branch(Word32Equal(callable_undetectable, Int32Constant(0)), &if_true,
           &if_false);
  }
  BIND(&if_other);
  {
    // Typeof doesn't return any other string value.
    Goto(&if_false);
  }

  BIND(&if_false);
  {
    SetAccumulator(FalseConstant());
    Goto(&end);
  }
  BIND(&if_true);
  {
    SetAccumulator(TrueConstant());
    Goto(&end);
  }
  BIND(&end);
  Dispatch();
}

// Jump <imm>
//
// Jump by the number of bytes represented by the immediate operand |imm|.
IGNITION_HANDLER(Jump, InterpreterAssembler) {
  Node* relative_jump = BytecodeOperandUImmWord(0);
  Jump(relative_jump);
}

// JumpConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool.
IGNITION_HANDLER(JumpConstant, InterpreterAssembler) {
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  Jump(relative_jump);
}

// JumpIfTrue <imm>
//
// Jump by the number of bytes represented by an immediate operand if the
// accumulator contains true. This only works for boolean inputs, and
// will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfTrue, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);
  CSA_ASSERT(this, TaggedIsNotSmi(accumulator));
  CSA_ASSERT(this, IsBoolean(accumulator));
  JumpIfWordEqual(accumulator, TrueConstant(), relative_jump);
}

// JumpIfTrueConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the accumulator contains true. This only works for boolean inputs,
// and will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfTrueConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  CSA_ASSERT(this, TaggedIsNotSmi(accumulator));
  CSA_ASSERT(this, IsBoolean(accumulator));
  JumpIfWordEqual(accumulator, TrueConstant(), relative_jump);
}

// JumpIfFalse <imm>
//
// Jump by the number of bytes represented by an immediate operand if the
// accumulator contains false. This only works for boolean inputs, and
// will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfFalse, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);
  CSA_ASSERT(this, TaggedIsNotSmi(accumulator));
  CSA_ASSERT(this, IsBoolean(accumulator));
  JumpIfWordEqual(accumulator, FalseConstant(), relative_jump);
}

// JumpIfFalseConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the accumulator contains false. This only works for boolean inputs,
// and will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfFalseConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  CSA_ASSERT(this, TaggedIsNotSmi(accumulator));
  CSA_ASSERT(this, IsBoolean(accumulator));
  JumpIfWordEqual(accumulator, FalseConstant(), relative_jump);
}

// JumpIfToBooleanTrue <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is true when the object is cast to boolean.
IGNITION_HANDLER(JumpIfToBooleanTrue, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  Jump(relative_jump);
  BIND(&if_false);
  Dispatch();
}

// JumpIfToBooleanTrueConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is true when the object is
// cast to boolean.
IGNITION_HANDLER(JumpIfToBooleanTrueConstant, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  Jump(relative_jump);
  BIND(&if_false);
  Dispatch();
}

// JumpIfToBooleanFalse <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is false when the object is cast to boolean.
IGNITION_HANDLER(JumpIfToBooleanFalse, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  Dispatch();
  BIND(&if_false);
  Jump(relative_jump);
}

// JumpIfToBooleanFalseConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is false when the object is
// cast to boolean.
IGNITION_HANDLER(JumpIfToBooleanFalseConstant, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  Dispatch();
  BIND(&if_false);
  Jump(relative_jump);
}

// JumpIfNull <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the null constant.
IGNITION_HANDLER(JumpIfNull, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);
  JumpIfWordEqual(accumulator, NullConstant(), relative_jump);
}

// JumpIfNullConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is the null constant.
IGNITION_HANDLER(JumpIfNullConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  JumpIfWordEqual(accumulator, NullConstant(), relative_jump);
}

// JumpIfNotNull <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is not the null constant.
IGNITION_HANDLER(JumpIfNotNull, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);
  JumpIfWordNotEqual(accumulator, NullConstant(), relative_jump);
}

// JumpIfNotNullConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is not the null constant.
IGNITION_HANDLER(JumpIfNotNullConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  JumpIfWordNotEqual(accumulator, NullConstant(), relative_jump);
}

// JumpIfUndefined <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the undefined constant.
IGNITION_HANDLER(JumpIfUndefined, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);
  JumpIfWordEqual(accumulator, UndefinedConstant(), relative_jump);
}

// JumpIfUndefinedConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is the undefined constant.
IGNITION_HANDLER(JumpIfUndefinedConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  JumpIfWordEqual(accumulator, UndefinedConstant(), relative_jump);
}

// JumpIfNotUndefined <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is not the undefined constant.
IGNITION_HANDLER(JumpIfNotUndefined, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);
  JumpIfWordNotEqual(accumulator, UndefinedConstant(), relative_jump);
}

// JumpIfNotUndefinedConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is not the undefined
// constant.
IGNITION_HANDLER(JumpIfNotUndefinedConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  JumpIfWordNotEqual(accumulator, UndefinedConstant(), relative_jump);
}

// JumpIfJSReceiver <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is a JSReceiver.
IGNITION_HANDLER(JumpIfJSReceiver, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);

  Label if_object(this), if_notobject(this, Label::kDeferred), if_notsmi(this);
  Branch(TaggedIsSmi(accumulator), &if_notobject, &if_notsmi);

  BIND(&if_notsmi);
  Branch(IsJSReceiver(accumulator), &if_object, &if_notobject);
  BIND(&if_object);
  Jump(relative_jump);

  BIND(&if_notobject);
  Dispatch();
}

// JumpIfJSReceiverConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is a JSReceiver.
IGNITION_HANDLER(JumpIfJSReceiverConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);

  Label if_object(this), if_notobject(this), if_notsmi(this);
  Branch(TaggedIsSmi(accumulator), &if_notobject, &if_notsmi);

  BIND(&if_notsmi);
  Branch(IsJSReceiver(accumulator), &if_object, &if_notobject);

  BIND(&if_object);
  Jump(relative_jump);

  BIND(&if_notobject);
  Dispatch();
}

// JumpLoop <imm> <loop_depth>
//
// Jump by the number of bytes represented by the immediate operand |imm|. Also
// performs a loop nesting check and potentially triggers OSR in case the
// current OSR level matches (or exceeds) the specified |loop_depth|.
IGNITION_HANDLER(JumpLoop, InterpreterAssembler) {
  Node* relative_jump = BytecodeOperandUImmWord(0);
  Node* loop_depth = BytecodeOperandImm(1);
  Node* osr_level = LoadOSRNestingLevel();

  // Check if OSR points at the given {loop_depth} are armed by comparing it to
  // the current {osr_level} loaded from the header of the BytecodeArray.
  Label ok(this), osr_armed(this, Label::kDeferred);
  Node* condition = Int32GreaterThanOrEqual(loop_depth, osr_level);
  Branch(condition, &ok, &osr_armed);

  BIND(&ok);
  JumpBackward(relative_jump);

  BIND(&osr_armed);
  {
    Callable callable = CodeFactory::InterpreterOnStackReplacement(isolate());
    Node* target = HeapConstant(callable.code());
    Node* context = GetContext();
    CallStub(callable.descriptor(), target, context);
    JumpBackward(relative_jump);
  }
}

// SwitchOnSmiNoFeedback <table_start> <table_length> <case_value_base>
//
// Jump by the number of bytes defined by a Smi in a table in the constant pool,
// where the table starts at |table_start| and has |table_length| entries.
// The table is indexed by the accumulator, minus |case_value_base|. If the
// case_value falls outside of the table |table_length|, fall-through to the
// next bytecode.
IGNITION_HANDLER(SwitchOnSmiNoFeedback, InterpreterAssembler) {
  Node* acc = GetAccumulator();
  Node* table_start = BytecodeOperandIdx(0);
  Node* table_length = BytecodeOperandUImmWord(1);
  Node* case_value_base = BytecodeOperandImmIntPtr(2);

  Label fall_through(this);

  // The accumulator must be a Smi.
  // TODO(leszeks): Add a bytecode with type feedback that allows other
  // accumulator values.
  CSA_ASSERT(this, TaggedIsSmi(acc));

  Node* case_value = IntPtrSub(SmiUntag(acc), case_value_base);
  GotoIf(IntPtrLessThan(case_value, IntPtrConstant(0)), &fall_through);
  GotoIf(IntPtrGreaterThanOrEqual(case_value, table_length), &fall_through);
  Node* entry = IntPtrAdd(table_start, case_value);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(entry);
  Jump(relative_jump);

  BIND(&fall_through);
  Dispatch();
}

// CreateRegExpLiteral <pattern_idx> <literal_idx> <flags>
//
// Creates a regular expression literal for literal index <literal_idx> with
// <flags> and the pattern in <pattern_idx>.
IGNITION_HANDLER(CreateRegExpLiteral, InterpreterAssembler) {
  Node* pattern = LoadConstantPoolEntryAtOperandIndex(0);
  Node* feedback_vector = LoadFeedbackVector();
  Node* slot_id = BytecodeOperandIdx(1);
  Node* flags = SmiFromInt32(BytecodeOperandFlag(2));
  Node* context = GetContext();
  ConstructorBuiltinsAssembler constructor_assembler(state());
  Node* result = constructor_assembler.EmitCreateRegExpLiteral(
      feedback_vector, slot_id, pattern, flags, context);
  SetAccumulator(result);
  Dispatch();
}

// CreateArrayLiteral <element_idx> <literal_idx> <flags>
//
// Creates an array literal for literal index <literal_idx> with
// CreateArrayLiteral flags <flags> and constant elements in <element_idx>.
IGNITION_HANDLER(CreateArrayLiteral, InterpreterAssembler) {
  Node* feedback_vector = LoadFeedbackVector();
  Node* slot_id = BytecodeOperandIdx(1);
  Node* context = GetContext();
  Node* bytecode_flags = BytecodeOperandFlag(2);

  Label fast_shallow_clone(this), call_runtime(this, Label::kDeferred);
  Branch(IsSetWord32<CreateArrayLiteralFlags::FastCloneSupportedBit>(
             bytecode_flags),
         &fast_shallow_clone, &call_runtime);

  BIND(&fast_shallow_clone);
  {
    ConstructorBuiltinsAssembler constructor_assembler(state());
    Node* result = constructor_assembler.EmitCreateShallowArrayLiteral(
        feedback_vector, slot_id, context, &call_runtime,
        TRACK_ALLOCATION_SITE);
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&call_runtime);
  {
    Node* flags_raw = DecodeWordFromWord32<CreateArrayLiteralFlags::FlagsBits>(
        bytecode_flags);
    Node* flags = SmiTag(flags_raw);
    Node* constant_elements = LoadConstantPoolEntryAtOperandIndex(0);
    Node* result =
        CallRuntime(Runtime::kCreateArrayLiteral, context, feedback_vector,
                    SmiTag(slot_id), constant_elements, flags);
    SetAccumulator(result);
    Dispatch();
  }
}

// CreateEmptyArrayLiteral <literal_idx>
//
// Creates an empty JSArray literal for literal index <literal_idx>.
IGNITION_HANDLER(CreateEmptyArrayLiteral, InterpreterAssembler) {
  Node* feedback_vector = LoadFeedbackVector();
  Node* slot_id = BytecodeOperandIdx(0);
  Node* context = GetContext();
  ConstructorBuiltinsAssembler constructor_assembler(state());
  Node* result = constructor_assembler.EmitCreateEmptyArrayLiteral(
      feedback_vector, slot_id, context);
  SetAccumulator(result);
  Dispatch();
}

// CreateObjectLiteral <element_idx> <literal_idx> <flags>
//
// Creates an object literal for literal index <literal_idx> with
// CreateObjectLiteralFlags <flags> and constant elements in <element_idx>.
IGNITION_HANDLER(CreateObjectLiteral, InterpreterAssembler) {
  Node* feedback_vector = LoadFeedbackVector();
  Node* slot_id = BytecodeOperandIdx(1);
  Node* bytecode_flags = BytecodeOperandFlag(2);

  // Check if we can do a fast clone or have to call the runtime.
  Label if_fast_clone(this), if_not_fast_clone(this, Label::kDeferred);
  Branch(IsSetWord32<CreateObjectLiteralFlags::FastCloneSupportedBit>(
             bytecode_flags),
         &if_fast_clone, &if_not_fast_clone);

  BIND(&if_fast_clone);
  {
    // If we can do a fast clone do the fast-path in CreateShallowObjectLiteral.
    ConstructorBuiltinsAssembler constructor_assembler(state());
    Node* result = constructor_assembler.EmitCreateShallowObjectLiteral(
        feedback_vector, slot_id, &if_not_fast_clone);
    StoreRegisterAtOperandIndex(result, 3);
    Dispatch();
  }

  BIND(&if_not_fast_clone);
  {
    // If we can't do a fast clone, call into the runtime.
    Node* boilerplate_description = LoadConstantPoolEntryAtOperandIndex(0);
    Node* context = GetContext();

    Node* flags_raw = DecodeWordFromWord32<CreateObjectLiteralFlags::FlagsBits>(
        bytecode_flags);
    Node* flags = SmiTag(flags_raw);

    Node* result =
        CallRuntime(Runtime::kCreateObjectLiteral, context, feedback_vector,
                    SmiTag(slot_id), boilerplate_description, flags);
    StoreRegisterAtOperandIndex(result, 3);
    // TODO(klaasb) build a single dispatch once the call is inlined
    Dispatch();
  }
}

// CreateEmptyObjectLiteral
//
// Creates an empty JSObject literal.
IGNITION_HANDLER(CreateEmptyObjectLiteral, InterpreterAssembler) {
  Node* context = GetContext();
  ConstructorBuiltinsAssembler constructor_assembler(state());
  Node* result = constructor_assembler.EmitCreateEmptyObjectLiteral(context);
  SetAccumulator(result);
  Dispatch();
}

// GetTemplateObject <descriptor_idx> <literal_idx>
//
// Creates the template to pass for tagged templates and returns it in the
// accumulator, creating and caching the site object on-demand as per the
// specification.
IGNITION_HANDLER(GetTemplateObject, InterpreterAssembler) {
  Node* feedback_vector = LoadFeedbackVector();
  Node* slot = BytecodeOperandIdx(1);
  Node* cached_value =
      LoadFeedbackVectorSlot(feedback_vector, slot, 0, INTPTR_PARAMETERS);

  Label call_runtime(this, Label::kDeferred);
  GotoIf(WordEqual(cached_value, SmiConstant(0)), &call_runtime);

  SetAccumulator(cached_value);
  Dispatch();

  BIND(&call_runtime);
  {
    Node* description = LoadConstantPoolEntryAtOperandIndex(0);
    Node* context = GetContext();
    Node* result =
        CallRuntime(Runtime::kCreateTemplateObject, context, description);
    StoreFeedbackVectorSlot(feedback_vector, slot, result, UPDATE_WRITE_BARRIER,
                            0, INTPTR_PARAMETERS);
    SetAccumulator(result);
    Dispatch();
  }
}

// CreateClosure <index> <slot> <tenured>
//
// Creates a new closure for SharedFunctionInfo at position |index| in the
// constant pool and with the PretenureFlag <tenured>.
IGNITION_HANDLER(CreateClosure, InterpreterAssembler) {
  Node* shared = LoadConstantPoolEntryAtOperandIndex(0);
  Node* flags = BytecodeOperandFlag(2);
  Node* context = GetContext();
  Node* slot = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();
  Node* feedback_cell = LoadFeedbackVectorSlot(feedback_vector, slot);

  Label if_fast(this), if_slow(this, Label::kDeferred);
  Branch(IsSetWord32<CreateClosureFlags::FastNewClosureBit>(flags), &if_fast,
         &if_slow);

  BIND(&if_fast);
  {
    Node* result =
        CallBuiltin(Builtins::kFastNewClosure, context, shared, feedback_cell);
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&if_slow);
  {
    Label if_newspace(this), if_oldspace(this);
    Branch(IsSetWord32<CreateClosureFlags::PretenuredBit>(flags), &if_oldspace,
           &if_newspace);

    BIND(&if_newspace);
    {
      Node* result =
          CallRuntime(Runtime::kNewClosure, context, shared, feedback_cell);
      SetAccumulator(result);
      Dispatch();
    }

    BIND(&if_oldspace);
    {
      Node* result = CallRuntime(Runtime::kNewClosure_Tenured, context, shared,
                                 feedback_cell);
      SetAccumulator(result);
      Dispatch();
    }
  }
}

// CreateBlockContext <index>
//
// Creates a new block context with the scope info constant at |index| and the
// closure in the accumulator.
IGNITION_HANDLER(CreateBlockContext, InterpreterAssembler) {
  Node* scope_info = LoadConstantPoolEntryAtOperandIndex(0);
  Node* closure = GetAccumulator();
  Node* context = GetContext();
  SetAccumulator(
      CallRuntime(Runtime::kPushBlockContext, context, scope_info, closure));
  Dispatch();
}

// CreateCatchContext <exception> <name_idx> <scope_info_idx>
//
// Creates a new context for a catch block with the |exception| in a register,
// the variable name at |name_idx|, the ScopeInfo at |scope_info_idx|, and the
// closure in the accumulator.
IGNITION_HANDLER(CreateCatchContext, InterpreterAssembler) {
  Node* exception = LoadRegisterAtOperandIndex(0);
  Node* name = LoadConstantPoolEntryAtOperandIndex(1);
  Node* scope_info = LoadConstantPoolEntryAtOperandIndex(2);
  Node* closure = GetAccumulator();
  Node* context = GetContext();
  SetAccumulator(CallRuntime(Runtime::kPushCatchContext, context, name,
                             exception, scope_info, closure));
  Dispatch();
}

// CreateFunctionContext <slots>
//
// Creates a new context with number of |slots| for the function closure.
IGNITION_HANDLER(CreateFunctionContext, InterpreterAssembler) {
  Node* closure = LoadRegister(Register::function_closure());
  Node* slots = BytecodeOperandUImm(0);
  Node* context = GetContext();
  ConstructorBuiltinsAssembler constructor_assembler(state());
  SetAccumulator(constructor_assembler.EmitFastNewFunctionContext(
      closure, slots, context, FUNCTION_SCOPE));
  Dispatch();
}

// CreateEvalContext <slots>
//
// Creates a new context with number of |slots| for an eval closure.
IGNITION_HANDLER(CreateEvalContext, InterpreterAssembler) {
  Node* closure = LoadRegister(Register::function_closure());
  Node* slots = BytecodeOperandUImm(0);
  Node* context = GetContext();
  ConstructorBuiltinsAssembler constructor_assembler(state());
  SetAccumulator(constructor_assembler.EmitFastNewFunctionContext(
      closure, slots, context, EVAL_SCOPE));
  Dispatch();
}

// CreateWithContext <register> <scope_info_idx>
//
// Creates a new context with the ScopeInfo at |scope_info_idx| for a
// with-statement with the object in |register| and the closure in the
// accumulator.
IGNITION_HANDLER(CreateWithContext, InterpreterAssembler) {
  Node* object = LoadRegisterAtOperandIndex(0);
  Node* scope_info = LoadConstantPoolEntryAtOperandIndex(1);
  Node* closure = GetAccumulator();
  Node* context = GetContext();
  SetAccumulator(CallRuntime(Runtime::kPushWithContext, context, object,
                             scope_info, closure));
  Dispatch();
}

// CreateMappedArguments
//
// Creates a new mapped arguments object.
IGNITION_HANDLER(CreateMappedArguments, InterpreterAssembler) {
  Node* closure = LoadRegister(Register::function_closure());
  Node* context = GetContext();

  Label if_duplicate_parameters(this, Label::kDeferred);
  Label if_not_duplicate_parameters(this);

  // Check if function has duplicate parameters.
  // TODO(rmcilroy): Remove this check when FastNewSloppyArgumentsStub supports
  // duplicate parameters.
  Node* shared_info =
      LoadObjectField(closure, JSFunction::kSharedFunctionInfoOffset);
  Node* flags = LoadObjectField(shared_info, SharedFunctionInfo::kFlagsOffset,
                                MachineType::Uint32());
  Node* has_duplicate_parameters =
      IsSetWord32<SharedFunctionInfo::HasDuplicateParametersBit>(flags);
  Branch(has_duplicate_parameters, &if_duplicate_parameters,
         &if_not_duplicate_parameters);

  BIND(&if_not_duplicate_parameters);
  {
    ArgumentsBuiltinsAssembler constructor_assembler(state());
    Node* result =
        constructor_assembler.EmitFastNewSloppyArguments(context, closure);
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&if_duplicate_parameters);
  {
    Node* result =
        CallRuntime(Runtime::kNewSloppyArguments_Generic, context, closure);
    SetAccumulator(result);
    Dispatch();
  }
}

// CreateUnmappedArguments
//
// Creates a new unmapped arguments object.
IGNITION_HANDLER(CreateUnmappedArguments, InterpreterAssembler) {
  Node* context = GetContext();
  Node* closure = LoadRegister(Register::function_closure());
  ArgumentsBuiltinsAssembler builtins_assembler(state());
  Node* result =
      builtins_assembler.EmitFastNewStrictArguments(context, closure);
  SetAccumulator(result);
  Dispatch();
}

// CreateRestParameter
//
// Creates a new rest parameter array.
IGNITION_HANDLER(CreateRestParameter, InterpreterAssembler) {
  Node* closure = LoadRegister(Register::function_closure());
  Node* context = GetContext();
  ArgumentsBuiltinsAssembler builtins_assembler(state());
  Node* result = builtins_assembler.EmitFastNewRestParameter(context, closure);
  SetAccumulator(result);
  Dispatch();
}

// StackCheck
//
// Performs a stack guard check.
IGNITION_HANDLER(StackCheck, InterpreterAssembler) {
  Node* context = GetContext();
  PerformStackCheck(context);
  Dispatch();
}

// SetPendingMessage
//
// Sets the pending message to the value in the accumulator, and returns the
// previous pending message in the accumulator.
IGNITION_HANDLER(SetPendingMessage, InterpreterAssembler) {
  Node* pending_message = ExternalConstant(
      ExternalReference::address_of_pending_message_obj(isolate()));
  Node* previous_message = Load(MachineType::TaggedPointer(), pending_message);
  Node* new_message = GetAccumulator();
  StoreNoWriteBarrier(MachineRepresentation::kTaggedPointer, pending_message,
                      new_message);
  SetAccumulator(previous_message);
  Dispatch();
}

// Throw
//
// Throws the exception in the accumulator.
IGNITION_HANDLER(Throw, InterpreterAssembler) {
  Node* exception = GetAccumulator();
  Node* context = GetContext();
  CallRuntime(Runtime::kThrow, context, exception);
  // We shouldn't ever return from a throw.
  Abort(AbortReason::kUnexpectedReturnFromThrow);
}

// ReThrow
//
// Re-throws the exception in the accumulator.
IGNITION_HANDLER(ReThrow, InterpreterAssembler) {
  Node* exception = GetAccumulator();
  Node* context = GetContext();
  CallRuntime(Runtime::kReThrow, context, exception);
  // We shouldn't ever return from a throw.
  Abort(AbortReason::kUnexpectedReturnFromThrow);
}

// Abort <abort_reason>
//
// Aborts execution (via a call to the runtime function).
IGNITION_HANDLER(Abort, InterpreterAssembler) {
  Node* reason = BytecodeOperandIdx(0);
  CallRuntime(Runtime::kAbort, NoContextConstant(), SmiTag(reason));
  Unreachable();
}

// Return
//
// Return the value in the accumulator.
IGNITION_HANDLER(Return, InterpreterAssembler) {
  UpdateInterruptBudgetOnReturn();
  Node* accumulator = GetAccumulator();
  Return(accumulator);
}

// ThrowReferenceErrorIfHole <variable_name>
//
// Throws an exception if the value in the accumulator is TheHole.
IGNITION_HANDLER(ThrowReferenceErrorIfHole, InterpreterAssembler) {
  Node* value = GetAccumulator();

  Label throw_error(this, Label::kDeferred);
  GotoIf(WordEqual(value, TheHoleConstant()), &throw_error);
  Dispatch();

  BIND(&throw_error);
  {
    Node* name = LoadConstantPoolEntryAtOperandIndex(0);
    CallRuntime(Runtime::kThrowReferenceError, GetContext(), name);
    // We shouldn't ever return from a throw.
    Abort(AbortReason::kUnexpectedReturnFromThrow);
  }
}

// ThrowSuperNotCalledIfHole
//
// Throws an exception if the value in the accumulator is TheHole.
IGNITION_HANDLER(ThrowSuperNotCalledIfHole, InterpreterAssembler) {
  Node* value = GetAccumulator();

  Label throw_error(this, Label::kDeferred);
  GotoIf(WordEqual(value, TheHoleConstant()), &throw_error);
  Dispatch();

  BIND(&throw_error);
  {
    CallRuntime(Runtime::kThrowSuperNotCalled, GetContext());
    // We shouldn't ever return from a throw.
    Abort(AbortReason::kUnexpectedReturnFromThrow);
  }
}

// ThrowSuperAlreadyCalledIfNotHole
//
// Throws SuperAleradyCalled exception if the value in the accumulator is not
// TheHole.
IGNITION_HANDLER(ThrowSuperAlreadyCalledIfNotHole, InterpreterAssembler) {
  Node* value = GetAccumulator();

  Label throw_error(this, Label::kDeferred);
  GotoIf(WordNotEqual(value, TheHoleConstant()), &throw_error);
  Dispatch();

  BIND(&throw_error);
  {
    CallRuntime(Runtime::kThrowSuperAlreadyCalledError, GetContext());
    // We shouldn't ever return from a throw.
    Abort(AbortReason::kUnexpectedReturnFromThrow);
  }
}

// Debugger
//
// Call runtime to handle debugger statement.
IGNITION_HANDLER(Debugger, InterpreterAssembler) {
  Node* context = GetContext();
  CallStub(CodeFactory::HandleDebuggerStatement(isolate()), context);
  Dispatch();
}

// DebugBreak
//
// Call runtime to handle a debug break.
#define DEBUG_BREAK(Name, ...)                                             \
  IGNITION_HANDLER(Name, InterpreterAssembler) {                           \
    Node* context = GetContext();                                          \
    Node* accumulator = GetAccumulator();                                  \
    Node* result_pair =                                                    \
        CallRuntime(Runtime::kDebugBreakOnBytecode, context, accumulator); \
    Node* return_value = Projection(0, result_pair);                       \
    Node* original_bytecode = SmiUntag(Projection(1, result_pair));        \
    MaybeDropFrames(context);                                              \
    SetAccumulator(return_value);                                          \
    DispatchToBytecode(original_bytecode, BytecodeOffset());               \
  }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK);
#undef DEBUG_BREAK

// IncBlockCounter <slot>
//
// Increment the execution count for the given slot. Used for block code
// coverage.
IGNITION_HANDLER(IncBlockCounter, InterpreterAssembler) {
  Node* closure = LoadRegister(Register::function_closure());
  Node* coverage_array_slot = BytecodeOperandIdxSmi(0);
  Node* context = GetContext();

  CallRuntime(Runtime::kIncBlockCounter, context, closure, coverage_array_slot);

  Dispatch();
}

// ForInEnumerate <receiver>
//
// Enumerates the enumerable keys of the |receiver| and either returns the
// map of the |receiver| if it has a usable enum cache or a fixed array
// with the keys to enumerate in the accumulator.
IGNITION_HANDLER(ForInEnumerate, InterpreterAssembler) {
  Node* receiver = LoadRegisterAtOperandIndex(0);
  Node* context = GetContext();

  Label if_empty(this), if_runtime(this, Label::kDeferred);
  Node* receiver_map = CheckEnumCache(receiver, &if_empty, &if_runtime);
  SetAccumulator(receiver_map);
  Dispatch();

  BIND(&if_empty);
  {
    Node* result = EmptyFixedArrayConstant();
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&if_runtime);
  {
    Node* result = CallRuntime(Runtime::kForInEnumerate, context, receiver);
    SetAccumulator(result);
    Dispatch();
  }
}

// ForInPrepare <cache_info_triple>
//
// Returns state for for..in loop execution based on the enumerator in
// the accumulator register, which is the result of calling ForInEnumerate
// on a JSReceiver object.
// The result is output in registers |cache_info_triple| to
// |cache_info_triple + 2|, with the registers holding cache_type, cache_array,
// and cache_length respectively.
IGNITION_HANDLER(ForInPrepare, InterpreterAssembler) {
  Node* enumerator = GetAccumulator();
  Node* vector_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();

  // The {enumerator} is either a Map or a FixedArray.
  CSA_ASSERT(this, TaggedIsNotSmi(enumerator));

  // Check if we're using an enum cache.
  Label if_fast(this), if_slow(this);
  Branch(IsMap(enumerator), &if_fast, &if_slow);

  BIND(&if_fast);
  {
    // Load the enumeration length and cache from the {enumerator}.
    Node* enum_length = LoadMapEnumLength(enumerator);
    CSA_ASSERT(this, WordNotEqual(enum_length,
                                  IntPtrConstant(kInvalidEnumCacheSentinel)));
    Node* descriptors = LoadMapDescriptors(enumerator);
    Node* enum_cache =
        LoadObjectField(descriptors, DescriptorArray::kEnumCacheOffset);
    Node* enum_keys = LoadObjectField(enum_cache, EnumCache::kKeysOffset);

    // Check if we have enum indices available.
    Node* enum_indices = LoadObjectField(enum_cache, EnumCache::kIndicesOffset);
    Node* enum_indices_length = LoadAndUntagFixedArrayBaseLength(enum_indices);
    Node* feedback = SelectSmiConstant(
        IntPtrLessThanOrEqual(enum_length, enum_indices_length),
        ForInFeedback::kEnumCacheKeysAndIndices, ForInFeedback::kEnumCacheKeys);
    UpdateFeedback(feedback, feedback_vector, vector_index);

    // Construct the cache info triple.
    Node* cache_type = enumerator;
    Node* cache_array = enum_keys;
    Node* cache_length = SmiTag(enum_length);
    StoreRegisterTripleAtOperandIndex(cache_type, cache_array, cache_length, 0);
    Dispatch();
  }

  BIND(&if_slow);
  {
    // The {enumerator} is a FixedArray with all the keys to iterate.
    CSA_ASSERT(this, IsFixedArray(enumerator));

    // Record the fact that we hit the for-in slow-path.
    UpdateFeedback(SmiConstant(ForInFeedback::kAny), feedback_vector,
                   vector_index);

    // Construct the cache info triple.
    Node* cache_type = enumerator;
    Node* cache_array = enumerator;
    Node* cache_length = LoadFixedArrayBaseLength(enumerator);
    StoreRegisterTripleAtOperandIndex(cache_type, cache_array, cache_length, 0);
    Dispatch();
  }
}

// ForInNext <receiver> <index> <cache_info_pair>
//
// Returns the next enumerable property in the the accumulator.
IGNITION_HANDLER(ForInNext, InterpreterAssembler) {
  Node* receiver = LoadRegisterAtOperandIndex(0);
  Node* index = LoadRegisterAtOperandIndex(1);
  Node* cache_type;
  Node* cache_array;
  std::tie(cache_type, cache_array) = LoadRegisterPairAtOperandIndex(2);
  Node* vector_index = BytecodeOperandIdx(3);
  Node* feedback_vector = LoadFeedbackVector();

  // Load the next key from the enumeration array.
  Node* key = LoadFixedArrayElement(cache_array, index, 0,
                                    CodeStubAssembler::SMI_PARAMETERS);

  // Check if we can use the for-in fast path potentially using the enum cache.
  Label if_fast(this), if_slow(this, Label::kDeferred);
  Node* receiver_map = LoadMap(receiver);
  Branch(WordEqual(receiver_map, cache_type), &if_fast, &if_slow);
  BIND(&if_fast);
  {
    // Enum cache in use for {receiver}, the {key} is definitely valid.
    SetAccumulator(key);
    Dispatch();
  }
  BIND(&if_slow);
  {
    // Record the fact that we hit the for-in slow-path.
    UpdateFeedback(SmiConstant(ForInFeedback::kAny), feedback_vector,
                   vector_index);

    // Need to filter the {key} for the {receiver}.
    Node* context = GetContext();
    Node* result = CallBuiltin(Builtins::kForInFilter, context, key, receiver);
    SetAccumulator(result);
    Dispatch();
  }
}

// ForInContinue <index> <cache_length>
//
// Returns false if the end of the enumerable properties has been reached.
IGNITION_HANDLER(ForInContinue, InterpreterAssembler) {
  Node* index = LoadRegisterAtOperandIndex(0);
  Node* cache_length = LoadRegisterAtOperandIndex(1);

  // Check if {index} is at {cache_length} already.
  Label if_true(this), if_false(this), end(this);
  Branch(WordEqual(index, cache_length), &if_true, &if_false);
  BIND(&if_true);
  {
    SetAccumulator(FalseConstant());
    Goto(&end);
  }
  BIND(&if_false);
  {
    SetAccumulator(TrueConstant());
    Goto(&end);
  }
  BIND(&end);
  Dispatch();
}

// ForInStep <index>
//
// Increments the loop counter in register |index| and stores the result
// in the accumulator.
IGNITION_HANDLER(ForInStep, InterpreterAssembler) {
  Node* index = LoadRegisterAtOperandIndex(0);
  Node* one = SmiConstant(1);
  Node* result = SmiAdd(index, one);
  SetAccumulator(result);
  Dispatch();
}

// Wide
//
// Prefix bytecode indicating next bytecode has wide (16-bit) operands.
IGNITION_HANDLER(Wide, InterpreterAssembler) {
  DispatchWide(OperandScale::kDouble);
}

// ExtraWide
//
// Prefix bytecode indicating next bytecode has extra-wide (32-bit) operands.
IGNITION_HANDLER(ExtraWide, InterpreterAssembler) {
  DispatchWide(OperandScale::kQuadruple);
}

// Illegal
//
// An invalid bytecode aborting execution if dispatched.
IGNITION_HANDLER(Illegal, InterpreterAssembler) {
  Abort(AbortReason::kInvalidBytecode);
}

// SuspendGenerator <generator> <first input register> <register count>
// <suspend_id>
//
// Exports the register file and stores it into the generator.  Also stores the
// current context, |suspend_id|, and the current bytecode offset (for debugging
// purposes) into the generator. Then, returns the value in the accumulator.
IGNITION_HANDLER(SuspendGenerator, InterpreterAssembler) {
  Node* generator = LoadRegisterAtOperandIndex(0);
  Node* array =
      LoadObjectField(generator, JSGeneratorObject::kRegisterFileOffset);
  Node* context = GetContext();
  RegListNodePair registers = GetRegisterListAtOperandIndex(1);
  Node* suspend_id = BytecodeOperandUImmSmi(3);

  ExportRegisterFile(array, registers);
  StoreObjectField(generator, JSGeneratorObject::kContextOffset, context);
  StoreObjectField(generator, JSGeneratorObject::kContinuationOffset,
                   suspend_id);

  // Store the bytecode offset in the [input_or_debug_pos] field, to be used by
  // the inspector.
  Node* offset = SmiTag(BytecodeOffset());
  StoreObjectField(generator, JSGeneratorObject::kInputOrDebugPosOffset,
                   offset);

  UpdateInterruptBudgetOnReturn();
  Return(GetAccumulator());
}

// SwitchOnGeneratorState <generator> <table_start> <table_length>
//
// If |generator| is undefined, falls through. Otherwise, loads the
// generator's state (overwriting it with kGeneratorExecuting), sets the context
// to the generator's resume context, and performs state dispatch on the
// generator's state by looking up the generator state in a jump table in the
// constant pool, starting at |table_start|, and of length |table_length|.
IGNITION_HANDLER(SwitchOnGeneratorState, InterpreterAssembler) {
  Node* generator = LoadRegisterAtOperandIndex(0);

  Label fallthrough(this);
  GotoIf(WordEqual(generator, UndefinedConstant()), &fallthrough);

  Node* state =
      LoadObjectField(generator, JSGeneratorObject::kContinuationOffset);
  Node* new_state = SmiConstant(JSGeneratorObject::kGeneratorExecuting);
  StoreObjectField(generator, JSGeneratorObject::kContinuationOffset,
                   new_state);

  Node* context = LoadObjectField(generator, JSGeneratorObject::kContextOffset);
  SetContext(context);

  Node* table_start = BytecodeOperandIdx(1);
  // TODO(leszeks): table_length is only used for a CSA_ASSERT, we don't
  // actually need it otherwise.
  Node* table_length = BytecodeOperandUImmWord(2);

  // The state must be a Smi.
  CSA_ASSERT(this, TaggedIsSmi(state));

  Node* case_value = SmiUntag(state);

  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(case_value, IntPtrConstant(0)));
  CSA_ASSERT(this, IntPtrLessThan(case_value, table_length));
  USE(table_length);

  Node* entry = IntPtrAdd(table_start, case_value);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(entry);
  Jump(relative_jump);

  BIND(&fallthrough);
  Dispatch();
}

// ResumeGenerator <generator> <first output register> <register count>
//
// Imports the register file stored in the generator and marks the generator
// state as executing.
IGNITION_HANDLER(ResumeGenerator, InterpreterAssembler) {
  Node* generator = LoadRegisterAtOperandIndex(0);
  RegListNodePair registers = GetRegisterListAtOperandIndex(1);

  ImportRegisterFile(
      LoadObjectField(generator, JSGeneratorObject::kRegisterFileOffset),
      registers);

  // Return the generator's input_or_debug_pos in the accumulator.
  SetAccumulator(
      LoadObjectField(generator, JSGeneratorObject::kInputOrDebugPosOffset));

  Dispatch();
}

}  // namespace

Handle<Code> GenerateBytecodeHandler(Isolate* isolate, Bytecode bytecode,
                                     OperandScale operand_scale) {
  Zone zone(isolate->allocator(), ZONE_NAME);
  InterpreterDispatchDescriptor descriptor(isolate);
  compiler::CodeAssemblerState state(
      isolate, &zone, descriptor, Code::BYTECODE_HANDLER,
      Bytecodes::ToString(bytecode),
      FLAG_untrusted_code_mitigations ? PoisoningMitigationLevel::kOn
                                      : PoisoningMitigationLevel::kOff,
      Bytecodes::ReturnCount(bytecode));

  switch (bytecode) {
#define CALL_GENERATOR(Name, ...)                     \
  case Bytecode::k##Name:                             \
    Name##Assembler::Generate(&state, operand_scale); \
    break;
    BYTECODE_LIST(CALL_GENERATOR);
#undef CALL_GENERATOR
  }

  Handle<Code> code = compiler::CodeAssembler::GenerateCode(&state);
  PROFILE(isolate, CodeCreateEvent(
                       CodeEventListener::BYTECODE_HANDLER_TAG,
                       AbstractCode::cast(*code),
                       Bytecodes::ToString(bytecode, operand_scale).c_str()));
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_trace_ignition_codegen) {
    OFStream os(stdout);
    code->Disassemble(Bytecodes::ToString(bytecode), os);
    os << std::flush;
  }
#endif  // ENABLE_DISASSEMBLER
  return code;
}

namespace {

// DeserializeLazy
//
// Deserialize the bytecode handler, store it in the dispatch table, and
// finally jump there (preserving existing args).
// We manually create a custom assembler instead of using the helper macros
// above since no corresponding bytecode exists.
class DeserializeLazyAssembler : public InterpreterAssembler {
 public:
  static const Bytecode kFakeBytecode = Bytecode::kIllegal;

  explicit DeserializeLazyAssembler(compiler::CodeAssemblerState* state,
                                    OperandScale operand_scale)
      : InterpreterAssembler(state, kFakeBytecode, operand_scale) {}

  static void Generate(compiler::CodeAssemblerState* state,
                       OperandScale operand_scale) {
    DeserializeLazyAssembler assembler(state, operand_scale);
    state->SetInitialDebugInformation("DeserializeLazy", __FILE__, __LINE__);
    assembler.GenerateImpl();
  }

 private:
  void GenerateImpl() { DeserializeLazyAndDispatch(); }

  DISALLOW_COPY_AND_ASSIGN(DeserializeLazyAssembler);
};

}  // namespace

Handle<Code> GenerateDeserializeLazyHandler(Isolate* isolate,
                                            OperandScale operand_scale) {
  Zone zone(isolate->allocator(), ZONE_NAME);
  const size_t return_count = 0;

  std::string debug_name = std::string("DeserializeLazy");
  if (operand_scale > OperandScale::kSingle) {
    Bytecode prefix_bytecode =
        Bytecodes::OperandScaleToPrefixBytecode(operand_scale);
    debug_name = debug_name.append(Bytecodes::ToString(prefix_bytecode));
  }

  InterpreterDispatchDescriptor descriptor(isolate);
  compiler::CodeAssemblerState state(
      isolate, &zone, descriptor, Code::BYTECODE_HANDLER, debug_name.c_str(),
      FLAG_untrusted_code_mitigations ? PoisoningMitigationLevel::kOn
                                      : PoisoningMitigationLevel::kOff,
      return_count);

  DeserializeLazyAssembler::Generate(&state, operand_scale);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(&state);
  PROFILE(isolate,
          CodeCreateEvent(CodeEventListener::BYTECODE_HANDLER_TAG,
                          AbstractCode::cast(*code), debug_name.c_str()));

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_trace_ignition_codegen) {
    OFStream os(stdout);
    code->Disassemble(debug_name.c_str(), os);
    os << std::flush;
  }
#endif  // ENABLE_DISASSEMBLER

  return code;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
