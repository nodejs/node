// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-generator.h"

#include <array>
#include <tuple>

#include "src/builtins/builtins-arguments-gen.h"
#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-forin-gen.h"
#include "src/code-events.h"
#include "src/code-factory.h"
#include "src/factory.h"
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
  Node* index = BytecodeOperandIdx(0);
  Node* constant = LoadConstantPoolEntry(index);
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
  Node* reg_index = BytecodeOperandReg(0);
  Node* value = LoadRegister(reg_index);
  SetAccumulator(value);
  Dispatch();
}

// Star <dst>
//
// Store accumulator to register <dst>.
IGNITION_HANDLER(Star, InterpreterAssembler) {
  Node* reg_index = BytecodeOperandReg(0);
  Node* accumulator = GetAccumulator();
  StoreRegister(accumulator, reg_index);
  Dispatch();
}

// Mov <src> <dst>
//
// Stores the value of register <src> to register <dst>.
IGNITION_HANDLER(Mov, InterpreterAssembler) {
  Node* src_index = BytecodeOperandReg(0);
  Node* src_value = LoadRegister(src_index);
  Node* dst_index = BytecodeOperandReg(1);
  StoreRegister(src_value, dst_index);
  Dispatch();
}

class InterpreterLoadGlobalAssembler : public InterpreterAssembler {
 public:
  InterpreterLoadGlobalAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                 OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void LdaGlobal(int slot_operand_index, int name_operand_index,
                 TypeofMode typeof_mode) {
    // Must be kept in sync with AccessorAssembler::LoadGlobalIC.

    // Load the global via the LoadGlobalIC.
    Node* feedback_vector = LoadFeedbackVector();
    Node* feedback_slot = BytecodeOperandIdx(slot_operand_index);

    AccessorAssembler accessor_asm(state());

    Label try_handler(this, Label::kDeferred), miss(this, Label::kDeferred);

    // Fast path without frame construction for the data case.
    {
      Label done(this);
      Variable var_result(this, MachineRepresentation::kTagged);
      ExitPoint exit_point(this, &done, &var_result);

      accessor_asm.LoadGlobalIC_TryPropertyCellCase(
          feedback_vector, feedback_slot, &exit_point, &try_handler, &miss,
          CodeStubAssembler::INTPTR_PARAMETERS);

      BIND(&done);
      SetAccumulator(var_result.value());
      Dispatch();
    }

    // Slow path with frame construction.
    {
      Label done(this);
      Variable var_result(this, MachineRepresentation::kTagged);
      ExitPoint exit_point(this, &done, &var_result);

      BIND(&try_handler);
      {
        Node* context = GetContext();
        Node* smi_slot = SmiTag(feedback_slot);
        Node* name_index = BytecodeOperandIdx(name_operand_index);
        Node* name = LoadConstantPoolEntry(name_index);

        AccessorAssembler::LoadICParameters params(context, nullptr, name,
                                                   smi_slot, feedback_vector);
        accessor_asm.LoadGlobalIC_TryHandlerCase(&params, typeof_mode,
                                                 &exit_point, &miss);
      }

      BIND(&miss);
      {
        Node* context = GetContext();
        Node* smi_slot = SmiTag(feedback_slot);
        Node* name_index = BytecodeOperandIdx(name_operand_index);
        Node* name = LoadConstantPoolEntry(name_index);

        AccessorAssembler::LoadICParameters params(context, nullptr, name,
                                                   smi_slot, feedback_vector);
        accessor_asm.LoadGlobalIC_MissCase(&params, &exit_point);
      }

      BIND(&done);
      {
        SetAccumulator(var_result.value());
        Dispatch();
      }
    }
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

class InterpreterStoreGlobalAssembler : public InterpreterAssembler {
 public:
  InterpreterStoreGlobalAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                  OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void StaGlobal(Callable ic) {
    // Get the global object.
    Node* context = GetContext();
    Node* native_context = LoadNativeContext(context);
    Node* global = LoadContextElement(native_context, Context::EXTENSION_INDEX);

    // Store the global via the StoreIC.
    Node* code_target = HeapConstant(ic.code());
    Node* constant_index = BytecodeOperandIdx(0);
    Node* name = LoadConstantPoolEntry(constant_index);
    Node* value = GetAccumulator();
    Node* raw_slot = BytecodeOperandIdx(1);
    Node* smi_slot = SmiTag(raw_slot);
    Node* feedback_vector = LoadFeedbackVector();
    CallStub(ic.descriptor(), code_target, context, global, name, value,
             smi_slot, feedback_vector);
    Dispatch();
  }
};

// StaGlobalSloppy <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot> in sloppy mode.
IGNITION_HANDLER(StaGlobalSloppy, InterpreterStoreGlobalAssembler) {
  Callable ic = CodeFactory::StoreGlobalICInOptimizedCode(isolate(), SLOPPY);
  StaGlobal(ic);
}

// StaGlobalStrict <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot> in strict mode.
IGNITION_HANDLER(StaGlobalStrict, InterpreterStoreGlobalAssembler) {
  Callable ic = CodeFactory::StoreGlobalICInOptimizedCode(isolate(), STRICT);
  StaGlobal(ic);
}

// LdaContextSlot <context> <slot_index> <depth>
//
// Load the object in |slot_index| of the context at |depth| in the context
// chain starting at |context| into the accumulator.
IGNITION_HANDLER(LdaContextSlot, InterpreterAssembler) {
  Node* reg_index = BytecodeOperandReg(0);
  Node* context = LoadRegister(reg_index);
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
  // Same as LdaContextSlot, should never be called.
  UNREACHABLE();
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
  // Same as LdaCurrentContextSlot, should never be called.
  UNREACHABLE();
}

// StaContextSlot <context> <slot_index> <depth>
//
// Stores the object in the accumulator into |slot_index| of the context at
// |depth| in the context chain starting at |context|.
IGNITION_HANDLER(StaContextSlot, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* reg_index = BytecodeOperandReg(0);
  Node* context = LoadRegister(reg_index);
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
  Node* name_index = BytecodeOperandIdx(0);
  Node* name = LoadConstantPoolEntry(name_index);
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
  Node* name_index = BytecodeOperandIdx(0);
  Node* name = LoadConstantPoolEntry(name_index);
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
    Node* name_index = BytecodeOperandIdx(0);
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
      Node* name = LoadConstantPoolEntry(name_index);
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
      Node* name_index = BytecodeOperandIdx(0);
      Node* name = LoadConstantPoolEntry(name_index);
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

// StaLookupSlotSloppy <name_index>
//
// Store the object in accumulator to the object with the name in constant
// pool entry |name_index| in sloppy mode.
IGNITION_HANDLER(StaLookupSlotSloppy, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* index = BytecodeOperandIdx(0);
  Node* name = LoadConstantPoolEntry(index);
  Node* context = GetContext();
  Node* result =
      CallRuntime(Runtime::kStoreLookupSlot_Sloppy, context, name, value);
  SetAccumulator(result);
  Dispatch();
}

// StaLookupSlotStrict <name_index>
//
// Store the object in accumulator to the object with the name in constant
// pool entry |name_index| in strict mode.
IGNITION_HANDLER(StaLookupSlotStrict, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Node* index = BytecodeOperandIdx(0);
  Node* name = LoadConstantPoolEntry(index);
  Node* context = GetContext();
  Node* result =
      CallRuntime(Runtime::kStoreLookupSlot_Strict, context, name, value);
  SetAccumulator(result);
  Dispatch();
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
  Node* register_index = BytecodeOperandReg(0);
  Node* recv = LoadRegister(register_index);

  // Load the name.
  // TODO(jgruber): Not needed for monomorphic smi handler constant/field case.
  Node* constant_index = BytecodeOperandIdx(1);
  Node* name = LoadConstantPoolEntry(constant_index);

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
  Callable ic = CodeFactory::KeyedLoadICInOptimizedCode(isolate());
  Node* code_target = HeapConstant(ic.code());
  Node* reg_index = BytecodeOperandReg(0);
  Node* object = LoadRegister(reg_index);
  Node* name = GetAccumulator();
  Node* raw_slot = BytecodeOperandIdx(1);
  Node* smi_slot = SmiTag(raw_slot);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();
  Node* result = CallStub(ic.descriptor(), code_target, context, object, name,
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
    Node* object_reg_index = BytecodeOperandReg(0);
    Node* object = LoadRegister(object_reg_index);
    Node* constant_index = BytecodeOperandIdx(1);
    Node* name = LoadConstantPoolEntry(constant_index);
    Node* value = GetAccumulator();
    Node* raw_slot = BytecodeOperandIdx(2);
    Node* smi_slot = SmiTag(raw_slot);
    Node* feedback_vector = LoadFeedbackVector();
    Node* context = GetContext();
    CallStub(ic.descriptor(), code_target, context, object, name, value,
             smi_slot, feedback_vector);
    Dispatch();
  }
};

// StaNamedPropertySloppy <object> <name_index> <slot>
//
// Calls the sloppy mode StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
IGNITION_HANDLER(StaNamedPropertySloppy,
                 InterpreterStoreNamedPropertyAssembler) {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate(), SLOPPY);
  StaNamedProperty(ic);
}

// StaNamedPropertyStrict <object> <name_index> <slot>
//
// Calls the strict mode StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
IGNITION_HANDLER(StaNamedPropertyStrict,
                 InterpreterStoreNamedPropertyAssembler) {
  Callable ic = CodeFactory::StoreICInOptimizedCode(isolate(), STRICT);
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

class InterpreterStoreKeyedPropertyAssembler : public InterpreterAssembler {
 public:
  InterpreterStoreKeyedPropertyAssembler(CodeAssemblerState* state,
                                         Bytecode bytecode,
                                         OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void StaKeyedProperty(Callable ic) {
    Node* code_target = HeapConstant(ic.code());
    Node* object_reg_index = BytecodeOperandReg(0);
    Node* object = LoadRegister(object_reg_index);
    Node* name_reg_index = BytecodeOperandReg(1);
    Node* name = LoadRegister(name_reg_index);
    Node* value = GetAccumulator();
    Node* raw_slot = BytecodeOperandIdx(2);
    Node* smi_slot = SmiTag(raw_slot);
    Node* feedback_vector = LoadFeedbackVector();
    Node* context = GetContext();
    CallStub(ic.descriptor(), code_target, context, object, name, value,
             smi_slot, feedback_vector);
    Dispatch();
  }
};

// StaKeyedPropertySloppy <object> <key> <slot>
//
// Calls the sloppy mode KeyStoreIC at FeedBackVector slot <slot> for <object>
// and the key <key> with the value in the accumulator.
IGNITION_HANDLER(StaKeyedPropertySloppy,
                 InterpreterStoreKeyedPropertyAssembler) {
  Callable ic = CodeFactory::KeyedStoreICInOptimizedCode(isolate(), SLOPPY);
  StaKeyedProperty(ic);
}

// StaKeyedPropertyStrict <object> <key> <slot>
//
// Calls the strict mode KeyStoreIC at FeedBackVector slot <slot> for <object>
// and the key <key> with the value in the accumulator.
IGNITION_HANDLER(StaKeyedPropertyStrict,
                 InterpreterStoreKeyedPropertyAssembler) {
  Callable ic = CodeFactory::KeyedStoreICInOptimizedCode(isolate(), STRICT);
  StaKeyedProperty(ic);
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
  Node* object = LoadRegister(BytecodeOperandReg(0));
  Node* name = LoadRegister(BytecodeOperandReg(1));
  Node* value = GetAccumulator();
  Node* flags = SmiFromWord32(BytecodeOperandFlag(2));
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
    Abort(kUnsupportedModuleOperation);
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
  Node* reg_index = BytecodeOperandReg(0);
  Node* new_context = GetAccumulator();
  Node* old_context = GetContext();
  StoreRegister(old_context, reg_index);
  SetContext(new_context);
  Dispatch();
}

// PopContext <context>
//
// Pops the current context and sets <context> as the new context.
IGNITION_HANDLER(PopContext, InterpreterAssembler) {
  Node* reg_index = BytecodeOperandReg(0);
  Node* context = LoadRegister(reg_index);
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
                                                        Node* vector);

  void BinaryOpWithFeedback(BinaryOpGenerator generator) {
    Node* reg_index = BytecodeOperandReg(0);
    Node* lhs = LoadRegister(reg_index);
    Node* rhs = GetAccumulator();
    Node* context = GetContext();
    Node* slot_index = BytecodeOperandIdx(1);
    Node* feedback_vector = LoadFeedbackVector();

    BinaryOpAssembler binop_asm(state());
    Node* result =
        (binop_asm.*generator)(context, lhs, rhs, slot_index, feedback_vector);
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

// AddSmi <imm>
//
// Adds an immediate value <imm> to the value in the accumulator.
IGNITION_HANDLER(AddSmi, InterpreterAssembler) {
  Variable var_result(this, MachineRepresentation::kTagged);
  Label fastpath(this), slowpath(this, Label::kDeferred), end(this);

  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();

  // {right} is known to be a Smi.
  // Check if the {left} is a Smi take the fast path.
  Branch(TaggedIsSmi(left), &fastpath, &slowpath);
  BIND(&fastpath);
  {
    // Try fast Smi addition first.
    Node* pair = IntPtrAddWithOverflow(BitcastTaggedToWord(left),
                                       BitcastTaggedToWord(right));
    Node* overflow = Projection(1, pair);

    // Check if the Smi additon overflowed.
    Label if_notoverflow(this);
    Branch(overflow, &slowpath, &if_notoverflow);
    BIND(&if_notoverflow);
    {
      UpdateFeedback(SmiConstant(BinaryOperationFeedback::kSignedSmall),
                     feedback_vector, slot_index);
      var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
      Goto(&end);
    }
  }
  BIND(&slowpath);
  {
    Node* context = GetContext();
    // TODO(ishell): pass slot as word-size value.
    var_result.Bind(CallBuiltin(Builtins::kAddWithFeedback, context, left,
                                right, TruncateWordToWord32(slot_index),
                                feedback_vector));
    Goto(&end);
  }
  BIND(&end);
  {
    SetAccumulator(var_result.value());
    Dispatch();
  }
}

// SubSmi <imm>
//
// Subtracts an immediate value <imm> from the value in the accumulator.
IGNITION_HANDLER(SubSmi, InterpreterAssembler) {
  Variable var_result(this, MachineRepresentation::kTagged);
  Label fastpath(this), slowpath(this, Label::kDeferred), end(this);

  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();

  // {right} is known to be a Smi.
  // Check if the {left} is a Smi take the fast path.
  Branch(TaggedIsSmi(left), &fastpath, &slowpath);
  BIND(&fastpath);
  {
    // Try fast Smi subtraction first.
    Node* pair = IntPtrSubWithOverflow(BitcastTaggedToWord(left),
                                       BitcastTaggedToWord(right));
    Node* overflow = Projection(1, pair);

    // Check if the Smi subtraction overflowed.
    Label if_notoverflow(this);
    Branch(overflow, &slowpath, &if_notoverflow);
    BIND(&if_notoverflow);
    {
      UpdateFeedback(SmiConstant(BinaryOperationFeedback::kSignedSmall),
                     feedback_vector, slot_index);
      var_result.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
      Goto(&end);
    }
  }
  BIND(&slowpath);
  {
    Node* context = GetContext();
    // TODO(ishell): pass slot as word-size value.
    var_result.Bind(CallBuiltin(Builtins::kSubtractWithFeedback, context, left,
                                right, TruncateWordToWord32(slot_index),
                                feedback_vector));
    Goto(&end);
  }
  BIND(&end);
  {
    SetAccumulator(var_result.value());
    Dispatch();
  }
}

// MulSmi <imm>
//
// Multiplies an immediate value <imm> to the value in the accumulator.
IGNITION_HANDLER(MulSmi, InterpreterAssembler) {
  Variable var_result(this, MachineRepresentation::kTagged);
  Label fastpath(this), slowpath(this, Label::kDeferred), end(this);

  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();

  // {right} is known to be a Smi.
  // Check if the {left} is a Smi take the fast path.
  Branch(TaggedIsSmi(left), &fastpath, &slowpath);
  BIND(&fastpath);
  {
    // Both {lhs} and {rhs} are Smis. The result is not necessarily a smi,
    // in case of overflow.
    var_result.Bind(SmiMul(left, right));
    Node* feedback = SelectSmiConstant(TaggedIsSmi(var_result.value()),
                                       BinaryOperationFeedback::kSignedSmall,
                                       BinaryOperationFeedback::kNumber);
    UpdateFeedback(feedback, feedback_vector, slot_index);
    Goto(&end);
  }
  BIND(&slowpath);
  {
    Node* context = GetContext();
    // TODO(ishell): pass slot as word-size value.
    var_result.Bind(CallBuiltin(Builtins::kMultiplyWithFeedback, context, left,
                                right, TruncateWordToWord32(slot_index),
                                feedback_vector));
    Goto(&end);
  }

  BIND(&end);
  {
    SetAccumulator(var_result.value());
    Dispatch();
  }
}

// DivSmi <imm>
//
// Divides the value in the accumulator by immediate value <imm>.
IGNITION_HANDLER(DivSmi, InterpreterAssembler) {
  Variable var_result(this, MachineRepresentation::kTagged);
  Label fastpath(this), slowpath(this, Label::kDeferred), end(this);

  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();

  // {right} is known to be a Smi.
  // Check if the {left} is a Smi take the fast path.
  Branch(TaggedIsSmi(left), &fastpath, &slowpath);
  BIND(&fastpath);
  {
    var_result.Bind(TrySmiDiv(left, right, &slowpath));
    UpdateFeedback(SmiConstant(BinaryOperationFeedback::kSignedSmall),
                   feedback_vector, slot_index);
    Goto(&end);
  }
  BIND(&slowpath);
  {
    Node* context = GetContext();
    // TODO(ishell): pass slot as word-size value.
    var_result.Bind(CallBuiltin(Builtins::kDivideWithFeedback, context, left,
                                right, TruncateWordToWord32(slot_index),
                                feedback_vector));
    Goto(&end);
  }

  BIND(&end);
  {
    SetAccumulator(var_result.value());
    Dispatch();
  }
}

// ModSmi <imm>
//
// Modulo accumulator by immediate value <imm>.
IGNITION_HANDLER(ModSmi, InterpreterAssembler) {
  Variable var_result(this, MachineRepresentation::kTagged);
  Label fastpath(this), slowpath(this, Label::kDeferred), end(this);

  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();

  // {right} is known to be a Smi.
  // Check if the {left} is a Smi take the fast path.
  Branch(TaggedIsSmi(left), &fastpath, &slowpath);
  BIND(&fastpath);
  {
    // Both {lhs} and {rhs} are Smis. The result is not necessarily a smi.
    var_result.Bind(SmiMod(left, right));
    Node* feedback = SelectSmiConstant(TaggedIsSmi(var_result.value()),
                                       BinaryOperationFeedback::kSignedSmall,
                                       BinaryOperationFeedback::kNumber);
    UpdateFeedback(feedback, feedback_vector, slot_index);
    Goto(&end);
  }
  BIND(&slowpath);
  {
    Node* context = GetContext();
    // TODO(ishell): pass slot as word-size value.
    var_result.Bind(CallBuiltin(Builtins::kModulusWithFeedback, context, left,
                                right, TruncateWordToWord32(slot_index),
                                feedback_vector));
    Goto(&end);
  }

  BIND(&end);
  {
    SetAccumulator(var_result.value());
    Dispatch();
  }
}

class InterpreterBitwiseBinaryOpAssembler : public InterpreterAssembler {
 public:
  InterpreterBitwiseBinaryOpAssembler(CodeAssemblerState* state,
                                      Bytecode bytecode,
                                      OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void BitwiseBinaryOpWithFeedback(Token::Value bitwise_op) {
    Node* reg_index = BytecodeOperandReg(0);
    Node* lhs = LoadRegister(reg_index);
    Node* rhs = GetAccumulator();
    Node* context = GetContext();
    Node* slot_index = BytecodeOperandIdx(1);
    Node* feedback_vector = LoadFeedbackVector();

    Variable var_lhs_type_feedback(this, MachineRepresentation::kTaggedSigned),
        var_rhs_type_feedback(this, MachineRepresentation::kTaggedSigned);
    Node* lhs_value = TruncateTaggedToWord32WithFeedback(
        context, lhs, &var_lhs_type_feedback);
    Node* rhs_value = TruncateTaggedToWord32WithFeedback(
        context, rhs, &var_rhs_type_feedback);
    Node* result = nullptr;

    switch (bitwise_op) {
      case Token::BIT_OR: {
        Node* value = Word32Or(lhs_value, rhs_value);
        result = ChangeInt32ToTagged(value);
      } break;
      case Token::BIT_AND: {
        Node* value = Word32And(lhs_value, rhs_value);
        result = ChangeInt32ToTagged(value);
      } break;
      case Token::BIT_XOR: {
        Node* value = Word32Xor(lhs_value, rhs_value);
        result = ChangeInt32ToTagged(value);
      } break;
      case Token::SHL: {
        Node* value =
            Word32Shl(lhs_value, Word32And(rhs_value, Int32Constant(0x1f)));
        result = ChangeInt32ToTagged(value);
      } break;
      case Token::SHR: {
        Node* value =
            Word32Shr(lhs_value, Word32And(rhs_value, Int32Constant(0x1f)));
        result = ChangeUint32ToTagged(value);
      } break;
      case Token::SAR: {
        Node* value =
            Word32Sar(lhs_value, Word32And(rhs_value, Int32Constant(0x1f)));
        result = ChangeInt32ToTagged(value);
      } break;
      default:
        UNREACHABLE();
    }

    Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                          BinaryOperationFeedback::kSignedSmall,
                                          BinaryOperationFeedback::kNumber);

    if (FLAG_debug_code) {
      Label ok(this);
      GotoIf(TaggedIsSmi(result), &ok);
      Node* result_map = LoadMap(result);
      AbortIfWordNotEqual(result_map, HeapNumberMapConstant(),
                          kExpectedHeapNumber);
      Goto(&ok);
      BIND(&ok);
    }

    Node* input_feedback =
        SmiOr(var_lhs_type_feedback.value(), var_rhs_type_feedback.value());
    UpdateFeedback(SmiOr(result_type, input_feedback), feedback_vector,
                   slot_index);
    SetAccumulator(result);
    Dispatch();
  }
};

// BitwiseOr <src>
//
// BitwiseOr register <src> to accumulator.
IGNITION_HANDLER(BitwiseOr, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Token::BIT_OR);
}

// BitwiseXor <src>
//
// BitwiseXor register <src> to accumulator.
IGNITION_HANDLER(BitwiseXor, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Token::BIT_XOR);
}

// BitwiseAnd <src>
//
// BitwiseAnd register <src> to accumulator.
IGNITION_HANDLER(BitwiseAnd, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Token::BIT_AND);
}

// ShiftLeft <src>
//
// Left shifts register <src> by the count specified in the accumulator.
// Register <src> is converted to an int32 and the accumulator to uint32
// before the operation. 5 lsb bits from the accumulator are used as count
// i.e. <src> << (accumulator & 0x1F).
IGNITION_HANDLER(ShiftLeft, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Token::SHL);
}

// ShiftRight <src>
//
// Right shifts register <src> by the count specified in the accumulator.
// Result is sign extended. Register <src> is converted to an int32 and the
// accumulator to uint32 before the operation. 5 lsb bits from the accumulator
// are used as count i.e. <src> >> (accumulator & 0x1F).
IGNITION_HANDLER(ShiftRight, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Token::SAR);
}

// ShiftRightLogical <src>
//
// Right Shifts register <src> by the count specified in the accumulator.
// Result is zero-filled. The accumulator and register <src> are converted to
// uint32 before the operation 5 lsb bits from the accumulator are used as
// count i.e. <src> << (accumulator & 0x1F).
IGNITION_HANDLER(ShiftRightLogical, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Token::SHR);
}

// BitwiseOr <imm>
//
// BitwiseOr accumulator with <imm>.
IGNITION_HANDLER(BitwiseOrSmi, InterpreterAssembler) {
  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  Variable var_lhs_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Node* lhs_value =
      TruncateTaggedToWord32WithFeedback(context, left, &var_lhs_type_feedback);
  Node* rhs_value = SmiToWord32(right);
  Node* value = Word32Or(lhs_value, rhs_value);
  Node* result = ChangeInt32ToTagged(value);
  Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                        BinaryOperationFeedback::kSignedSmall,
                                        BinaryOperationFeedback::kNumber);
  UpdateFeedback(SmiOr(result_type, var_lhs_type_feedback.value()),
                 feedback_vector, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// BitwiseXor <imm>
//
// BitwiseXor accumulator with <imm>.
IGNITION_HANDLER(BitwiseXorSmi, InterpreterAssembler) {
  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  Variable var_lhs_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Node* lhs_value =
      TruncateTaggedToWord32WithFeedback(context, left, &var_lhs_type_feedback);
  Node* rhs_value = SmiToWord32(right);
  Node* value = Word32Xor(lhs_value, rhs_value);
  Node* result = ChangeInt32ToTagged(value);
  Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                        BinaryOperationFeedback::kSignedSmall,
                                        BinaryOperationFeedback::kNumber);
  UpdateFeedback(SmiOr(result_type, var_lhs_type_feedback.value()),
                 feedback_vector, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// BitwiseAnd <imm>
//
// BitwiseAnd accumulator with <imm>.
IGNITION_HANDLER(BitwiseAndSmi, InterpreterAssembler) {
  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  Variable var_lhs_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Node* lhs_value =
      TruncateTaggedToWord32WithFeedback(context, left, &var_lhs_type_feedback);
  Node* rhs_value = SmiToWord32(right);
  Node* value = Word32And(lhs_value, rhs_value);
  Node* result = ChangeInt32ToTagged(value);
  Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                        BinaryOperationFeedback::kSignedSmall,
                                        BinaryOperationFeedback::kNumber);
  UpdateFeedback(SmiOr(result_type, var_lhs_type_feedback.value()),
                 feedback_vector, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// ShiftLeftSmi <imm>
//
// Left shifts accumulator by the count specified in <imm>.
// The accumulator is converted to an int32 before the operation. The 5
// lsb bits from <imm> are used as count i.e. <src> << (<imm> & 0x1F).
IGNITION_HANDLER(ShiftLeftSmi, InterpreterAssembler) {
  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  Variable var_lhs_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Node* lhs_value =
      TruncateTaggedToWord32WithFeedback(context, left, &var_lhs_type_feedback);
  Node* rhs_value = SmiToWord32(right);
  Node* shift_count = Word32And(rhs_value, Int32Constant(0x1f));
  Node* value = Word32Shl(lhs_value, shift_count);
  Node* result = ChangeInt32ToTagged(value);
  Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                        BinaryOperationFeedback::kSignedSmall,
                                        BinaryOperationFeedback::kNumber);
  UpdateFeedback(SmiOr(result_type, var_lhs_type_feedback.value()),
                 feedback_vector, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// ShiftRightSmi <imm>
//
// Right shifts accumulator by the count specified in <imm>. Result is sign
// extended. The accumulator is converted to an int32 before the operation. The
// 5 lsb bits from <imm> are used as count i.e. <src> << (<imm> & 0x1F).
IGNITION_HANDLER(ShiftRightSmi, InterpreterAssembler) {
  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  Variable var_lhs_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Node* lhs_value =
      TruncateTaggedToWord32WithFeedback(context, left, &var_lhs_type_feedback);
  Node* rhs_value = SmiToWord32(right);
  Node* shift_count = Word32And(rhs_value, Int32Constant(0x1f));
  Node* value = Word32Sar(lhs_value, shift_count);
  Node* result = ChangeInt32ToTagged(value);
  Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                        BinaryOperationFeedback::kSignedSmall,
                                        BinaryOperationFeedback::kNumber);
  UpdateFeedback(SmiOr(result_type, var_lhs_type_feedback.value()),
                 feedback_vector, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// ShiftRightLogicalSmi <imm>
//
// Right shifts accumulator by the count specified in <imm>. Result is zero
// extended. The accumulator is converted to an int32 before the operation. The
// 5 lsb bits from <imm> are used as count i.e. <src> << (<imm> & 0x1F).
IGNITION_HANDLER(ShiftRightLogicalSmi, InterpreterAssembler) {
  Node* left = GetAccumulator();
  Node* right = BytecodeOperandImmSmi(0);
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();

  Variable var_lhs_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Node* lhs_value =
      TruncateTaggedToWord32WithFeedback(context, left, &var_lhs_type_feedback);
  Node* rhs_value = SmiToWord32(right);
  Node* shift_count = Word32And(rhs_value, Int32Constant(0x1f));
  Node* value = Word32Shr(lhs_value, shift_count);
  Node* result = ChangeUint32ToTagged(value);
  Node* result_type = SelectSmiConstant(TaggedIsSmi(result),
                                        BinaryOperationFeedback::kSignedSmall,
                                        BinaryOperationFeedback::kNumber);
  UpdateFeedback(SmiOr(result_type, var_lhs_type_feedback.value()),
                 feedback_vector, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// ToName
//
// Convert the object referenced by the accumulator to a name.
IGNITION_HANDLER(ToName, InterpreterAssembler) {
  Node* object = GetAccumulator();
  Node* context = GetContext();
  Node* result = ToName(context, object);
  StoreRegister(result, BytecodeOperandReg(0));
  Dispatch();
}

// ToNumber <dst> <slot>
//
// Convert the object referenced by the accumulator to a number.
IGNITION_HANDLER(ToNumber, InterpreterAssembler) {
  Node* object = GetAccumulator();
  Node* context = GetContext();

  // Convert the {object} to a Number and collect feedback for the {object}.
  Variable var_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Variable var_result(this, MachineRepresentation::kTagged);
  Label if_done(this), if_objectissmi(this), if_objectisnumber(this),
      if_objectisother(this, Label::kDeferred);

  GotoIf(TaggedIsSmi(object), &if_objectissmi);
  Node* object_map = LoadMap(object);
  Branch(IsHeapNumberMap(object_map), &if_objectisnumber, &if_objectisother);

  BIND(&if_objectissmi);
  {
    var_result.Bind(object);
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kSignedSmall));
    Goto(&if_done);
  }

  BIND(&if_objectisnumber);
  {
    var_result.Bind(object);
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNumber));
    Goto(&if_done);
  }

  BIND(&if_objectisother);
  {
    // Convert the {object} to a Number.
    Callable callable = CodeFactory::NonNumberToNumber(isolate());
    var_result.Bind(CallStub(callable, context, object));
    var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
    Goto(&if_done);
  }

  BIND(&if_done);
  StoreRegister(var_result.value(), BytecodeOperandReg(0));

  // Record the type feedback collected for {object}.
  Node* slot_index = BytecodeOperandIdx(1);
  Node* feedback_vector = LoadFeedbackVector();
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_index);

  Dispatch();
}

// ToObject
//
// Convert the object referenced by the accumulator to a JSReceiver.
IGNITION_HANDLER(ToObject, InterpreterAssembler) {
  Callable callable(CodeFactory::ToObject(isolate()));
  Node* target = HeapConstant(callable.code());
  Node* accumulator = GetAccumulator();
  Node* context = GetContext();
  Node* result = CallStub(callable.descriptor(), target, context, accumulator);
  StoreRegister(result, BytecodeOperandReg(0));
  Dispatch();
}

// Inc
//
// Increments value in the accumulator by one.
IGNITION_HANDLER(Inc, InterpreterAssembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* value = GetAccumulator();
  Node* context = GetContext();
  Node* slot_index = BytecodeOperandIdx(0);
  Node* feedback_vector = LoadFeedbackVector();

  // Shared entry for floating point increment.
  Label do_finc(this), end(this);
  Variable var_finc_value(this, MachineRepresentation::kFloat64);

  // We might need to try again due to ToNumber conversion.
  Variable value_var(this, MachineRepresentation::kTagged);
  Variable result_var(this, MachineRepresentation::kTagged);
  Variable var_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Variable* loop_vars[] = {&value_var, &var_type_feedback};
  Label start(this, 2, loop_vars);
  value_var.Bind(value);
  var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNone));
  Goto(&start);
  BIND(&start);
  {
    value = value_var.value();

    Label if_issmi(this), if_isnotsmi(this);
    Branch(TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

    BIND(&if_issmi);
    {
      // Try fast Smi addition first.
      Node* one = SmiConstant(Smi::FromInt(1));
      Node* pair = IntPtrAddWithOverflow(BitcastTaggedToWord(value),
                                         BitcastTaggedToWord(one));
      Node* overflow = Projection(1, pair);

      // Check if the Smi addition overflowed.
      Label if_overflow(this), if_notoverflow(this);
      Branch(overflow, &if_overflow, &if_notoverflow);

      BIND(&if_notoverflow);
      var_type_feedback.Bind(
          SmiOr(var_type_feedback.value(),
                SmiConstant(BinaryOperationFeedback::kSignedSmall)));
      result_var.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
      Goto(&end);

      BIND(&if_overflow);
      {
        var_finc_value.Bind(SmiToFloat64(value));
        Goto(&do_finc);
      }
    }

    BIND(&if_isnotsmi);
    {
      // Check if the value is a HeapNumber.
      Label if_valueisnumber(this), if_valuenotnumber(this, Label::kDeferred);
      Node* value_map = LoadMap(value);
      Branch(IsHeapNumberMap(value_map), &if_valueisnumber, &if_valuenotnumber);

      BIND(&if_valueisnumber);
      {
        // Load the HeapNumber value.
        var_finc_value.Bind(LoadHeapNumberValue(value));
        Goto(&do_finc);
      }

      BIND(&if_valuenotnumber);
      {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a number, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(this, SmiEqual(var_type_feedback.value(),
                                  SmiConstant(BinaryOperationFeedback::kNone)));

        Label if_valueisoddball(this), if_valuenotoddball(this);
        Node* instance_type = LoadMapInstanceType(value_map);
        Node* is_oddball =
            Word32Equal(instance_type, Int32Constant(ODDBALL_TYPE));
        Branch(is_oddball, &if_valueisoddball, &if_valuenotoddball);

        BIND(&if_valueisoddball);
        {
          // Convert Oddball to Number and check again.
          value_var.Bind(LoadObjectField(value, Oddball::kToNumberOffset));
          var_type_feedback.Bind(
              SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
          Goto(&start);
        }

        BIND(&if_valuenotoddball);
        {
          // Convert to a Number first and try again.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
          value_var.Bind(CallStub(callable, context, value));
          Goto(&start);
        }
      }
    }
  }

  BIND(&do_finc);
  {
    Node* finc_value = var_finc_value.value();
    Node* one = Float64Constant(1.0);
    Node* finc_result = Float64Add(finc_value, one);
    var_type_feedback.Bind(
        SmiOr(var_type_feedback.value(),
              SmiConstant(BinaryOperationFeedback::kNumber)));
    result_var.Bind(AllocateHeapNumberWithValue(finc_result));
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_index);

  SetAccumulator(result_var.value());
  Dispatch();
}

// Dec
//
// Decrements value in the accumulator by one.
IGNITION_HANDLER(Dec, InterpreterAssembler) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  Node* value = GetAccumulator();
  Node* context = GetContext();
  Node* slot_index = BytecodeOperandIdx(0);
  Node* feedback_vector = LoadFeedbackVector();

  // Shared entry for floating point decrement.
  Label do_fdec(this), end(this);
  Variable var_fdec_value(this, MachineRepresentation::kFloat64);

  // We might need to try again due to ToNumber conversion.
  Variable value_var(this, MachineRepresentation::kTagged);
  Variable result_var(this, MachineRepresentation::kTagged);
  Variable var_type_feedback(this, MachineRepresentation::kTaggedSigned);
  Variable* loop_vars[] = {&value_var, &var_type_feedback};
  Label start(this, 2, loop_vars);
  var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kNone));
  value_var.Bind(value);
  Goto(&start);
  BIND(&start);
  {
    value = value_var.value();

    Label if_issmi(this), if_isnotsmi(this);
    Branch(TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

    BIND(&if_issmi);
    {
      // Try fast Smi subtraction first.
      Node* one = SmiConstant(Smi::FromInt(1));
      Node* pair = IntPtrSubWithOverflow(BitcastTaggedToWord(value),
                                         BitcastTaggedToWord(one));
      Node* overflow = Projection(1, pair);

      // Check if the Smi subtraction overflowed.
      Label if_overflow(this), if_notoverflow(this);
      Branch(overflow, &if_overflow, &if_notoverflow);

      BIND(&if_notoverflow);
      var_type_feedback.Bind(
          SmiOr(var_type_feedback.value(),
                SmiConstant(BinaryOperationFeedback::kSignedSmall)));
      result_var.Bind(BitcastWordToTaggedSigned(Projection(0, pair)));
      Goto(&end);

      BIND(&if_overflow);
      {
        var_fdec_value.Bind(SmiToFloat64(value));
        Goto(&do_fdec);
      }
    }

    BIND(&if_isnotsmi);
    {
      // Check if the value is a HeapNumber.
      Label if_valueisnumber(this), if_valuenotnumber(this, Label::kDeferred);
      Node* value_map = LoadMap(value);
      Branch(IsHeapNumberMap(value_map), &if_valueisnumber, &if_valuenotnumber);

      BIND(&if_valueisnumber);
      {
        // Load the HeapNumber value.
        var_fdec_value.Bind(LoadHeapNumberValue(value));
        Goto(&do_fdec);
      }

      BIND(&if_valuenotnumber);
      {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a number, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(this, SmiEqual(var_type_feedback.value(),
                                  SmiConstant(BinaryOperationFeedback::kNone)));

        Label if_valueisoddball(this), if_valuenotoddball(this);
        Node* instance_type = LoadMapInstanceType(value_map);
        Node* is_oddball =
            Word32Equal(instance_type, Int32Constant(ODDBALL_TYPE));
        Branch(is_oddball, &if_valueisoddball, &if_valuenotoddball);

        BIND(&if_valueisoddball);
        {
          // Convert Oddball to Number and check again.
          value_var.Bind(LoadObjectField(value, Oddball::kToNumberOffset));
          var_type_feedback.Bind(
              SmiConstant(BinaryOperationFeedback::kNumberOrOddball));
          Goto(&start);
        }

        BIND(&if_valuenotoddball);
        {
          // Convert to a Number first and try again.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_type_feedback.Bind(SmiConstant(BinaryOperationFeedback::kAny));
          value_var.Bind(CallStub(callable, context, value));
          Goto(&start);
        }
      }
    }
  }

  BIND(&do_fdec);
  {
    Node* fdec_value = var_fdec_value.value();
    Node* one = Float64Constant(1.0);
    Node* fdec_result = Float64Sub(fdec_value, one);
    var_type_feedback.Bind(
        SmiOr(var_type_feedback.value(),
              SmiConstant(BinaryOperationFeedback::kNumber)));
    result_var.Bind(AllocateHeapNumberWithValue(fdec_result));
    Goto(&end);
  }

  BIND(&end);
  UpdateFeedback(var_type_feedback.value(), feedback_vector, slot_index);

  SetAccumulator(result_var.value());
  Dispatch();
}

// LogicalNot
//
// Perform logical-not on the accumulator, first casting the
// accumulator to a boolean value if required.
// ToBooleanLogicalNot
IGNITION_HANDLER(ToBooleanLogicalNot, InterpreterAssembler) {
  Node* value = GetAccumulator();
  Variable result(this, MachineRepresentation::kTagged);
  Label if_true(this), if_false(this), end(this);
  Node* true_value = BooleanConstant(true);
  Node* false_value = BooleanConstant(false);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  {
    result.Bind(false_value);
    Goto(&end);
  }
  BIND(&if_false);
  {
    result.Bind(true_value);
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
  Node* true_value = BooleanConstant(true);
  Node* false_value = BooleanConstant(false);
  Branch(WordEqual(value, true_value), &if_true, &if_false);
  BIND(&if_true);
  {
    result.Bind(false_value);
    Goto(&end);
  }
  BIND(&if_false);
  {
    if (FLAG_debug_code) {
      AbortIfWordNotEqual(value, false_value,
                          BailoutReason::kExpectedBooleanValue);
    }
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
  Node* reg_index = BytecodeOperandReg(0);
  Node* object = LoadRegister(reg_index);
  Node* key = GetAccumulator();
  Node* context = GetContext();
  Node* result = CallBuiltin(Builtins::kDeleteProperty, context, object, key,
                             SmiConstant(STRICT));
  SetAccumulator(result);
  Dispatch();
}

// DeletePropertySloppy
//
// Delete the property specified in the accumulator from the object
// referenced by the register operand following sloppy mode semantics.
IGNITION_HANDLER(DeletePropertySloppy, InterpreterAssembler) {
  Node* reg_index = BytecodeOperandReg(0);
  Node* object = LoadRegister(reg_index);
  Node* key = GetAccumulator();
  Node* context = GetContext();
  Node* result = CallBuiltin(Builtins::kDeleteProperty, context, object, key,
                             SmiConstant(SLOPPY));
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
  Node* result = GetSuperConstructor(active_function, context);
  Node* reg = BytecodeOperandReg(0);
  StoreRegister(result, reg);
  Dispatch();
}

class InterpreterJSCallAssembler : public InterpreterAssembler {
 public:
  InterpreterJSCallAssembler(CodeAssemblerState* state, Bytecode bytecode,
                             OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  // Generates code to perform a JS call that collects type feedback.
  void JSCall(ConvertReceiverMode receiver_mode, TailCallMode tail_call_mode) {
    Node* function_reg = BytecodeOperandReg(0);
    Node* function = LoadRegister(function_reg);
    Node* first_arg_reg = BytecodeOperandReg(1);
    Node* first_arg = RegisterLocation(first_arg_reg);
    Node* arg_list_count = BytecodeOperandCount(2);
    Node* args_count;
    if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
      // The receiver is implied, so it is not in the argument list.
      args_count = arg_list_count;
    } else {
      // Subtract the receiver from the argument count.
      Node* receiver_count = Int32Constant(1);
      args_count = Int32Sub(arg_list_count, receiver_count);
    }
    Node* slot_id = BytecodeOperandIdx(3);
    Node* feedback_vector = LoadFeedbackVector();
    Node* context = GetContext();
    Node* result =
        CallJSWithFeedback(function, context, first_arg, args_count, slot_id,
                           feedback_vector, receiver_mode, tail_call_mode);
    SetAccumulator(result);
    Dispatch();
  }

  // Generates code to perform a JS call with a known number of arguments that
  // collects type feedback.
  void JSCallN(int arg_count, ConvertReceiverMode receiver_mode) {
    // Indices and counts of operands on the bytecode.
    const int kFirstArgumentOperandIndex = 1;
    const int kReceiverOperandCount =
        (receiver_mode == ConvertReceiverMode::kNullOrUndefined) ? 0 : 1;
    const int kSlotOperandIndex =
        kFirstArgumentOperandIndex + kReceiverOperandCount + arg_count;
    // Indices and counts of parameters to the call stub.
    const int kBoilerplateParameterCount = 7;
    const int kReceiverParameterIndex = 5;
    const int kReceiverParameterCount = 1;
    // Only used in a DCHECK.
    USE(kReceiverParameterCount);

    Node* function_reg = BytecodeOperandReg(0);
    Node* function = LoadRegister(function_reg);
    std::array<Node*, Bytecodes::kMaxOperands + kBoilerplateParameterCount>
        temp;
    Callable call_ic = CodeFactory::CallIC(isolate());
    temp[0] = HeapConstant(call_ic.code());
    temp[1] = function;
    temp[2] = Int32Constant(arg_count);
    temp[3] = BytecodeOperandIdxInt32(kSlotOperandIndex);
    temp[4] = LoadFeedbackVector();

    int parameter_index = kReceiverParameterIndex;
    if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
      // The first argument parameter (the receiver) is implied to be undefined.
      Node* undefined_value =
          HeapConstant(isolate()->factory()->undefined_value());
      temp[parameter_index++] = undefined_value;
    }
    // The bytecode argument operands are copied into the remaining argument
    // parameters.
    for (int i = 0; i < (kReceiverOperandCount + arg_count); ++i) {
      Node* reg = BytecodeOperandReg(kFirstArgumentOperandIndex + i);
      temp[parameter_index++] = LoadRegister(reg);
    }

    DCHECK_EQ(parameter_index,
              kReceiverParameterIndex + kReceiverParameterCount + arg_count);
    temp[parameter_index] = GetContext();

    Node* result = CallStubN(call_ic.descriptor(), 1,
                             arg_count + kBoilerplateParameterCount, &temp[0]);
    SetAccumulator(result);
    Dispatch();
  }
};

// Call <callable> <receiver> <arg_count> <feedback_slot_id>
//
// Call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers. Collect type feedback
// into |feedback_slot_id|
IGNITION_HANDLER(CallAnyReceiver, InterpreterJSCallAssembler) {
  JSCall(ConvertReceiverMode::kAny, TailCallMode::kDisallow);
}

IGNITION_HANDLER(CallProperty, InterpreterJSCallAssembler) {
  JSCall(ConvertReceiverMode::kNotNullOrUndefined, TailCallMode::kDisallow);
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
  JSCall(ConvertReceiverMode::kNullOrUndefined, TailCallMode::kDisallow);
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

// TailCall <callable> <receiver> <arg_count> <feedback_slot_id>
//
// Tail call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers. Collect type feedback
// into |feedback_slot_id|
IGNITION_HANDLER(TailCall, InterpreterJSCallAssembler) {
  JSCall(ConvertReceiverMode::kAny, TailCallMode::kAllow);
}

// CallRuntime <function_id> <first_arg> <arg_count>
//
// Call the runtime function |function_id| with the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers.
IGNITION_HANDLER(CallRuntime, InterpreterAssembler) {
  Node* function_id = BytecodeOperandRuntimeId(0);
  Node* first_arg_reg = BytecodeOperandReg(1);
  Node* first_arg = RegisterLocation(first_arg_reg);
  Node* args_count = BytecodeOperandCount(2);
  Node* context = GetContext();
  Node* result = CallRuntimeN(function_id, context, first_arg, args_count);
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
  Node* first_arg_reg = BytecodeOperandReg(1);
  Node* arg_count = BytecodeOperandCount(2);
  Node* context = GetContext();
  Node* result = GenerateInvokeIntrinsic(this, function_id, context,
                                         first_arg_reg, arg_count);
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
  Node* first_arg_reg = BytecodeOperandReg(1);
  Node* first_arg = RegisterLocation(first_arg_reg);
  Node* args_count = BytecodeOperandCount(2);
  Node* context = GetContext();
  Node* result_pair =
      CallRuntimeN(function_id, context, first_arg, args_count, 2);
  // Store the results in <first_return> and <first_return + 1>
  Node* first_return_reg = BytecodeOperandReg(3);
  Node* second_return_reg = NextRegister(first_return_reg);
  Node* result0 = Projection(0, result_pair);
  Node* result1 = Projection(1, result_pair);
  StoreRegister(result0, first_return_reg);
  StoreRegister(result1, second_return_reg);
  Dispatch();
}

// CallJSRuntime <context_index> <receiver> <arg_count>
//
// Call the JS runtime function that has the |context_index| with the receiver
// in register |receiver| and |arg_count| arguments in subsequent registers.
IGNITION_HANDLER(CallJSRuntime, InterpreterAssembler) {
  Node* context_index = BytecodeOperandIdx(0);
  Node* receiver_reg = BytecodeOperandReg(1);
  Node* first_arg = RegisterLocation(receiver_reg);
  Node* receiver_args_count = BytecodeOperandCount(2);
  Node* receiver_count = Int32Constant(1);
  Node* args_count = Int32Sub(receiver_args_count, receiver_count);

  // Get the function to call from the native context.
  Node* context = GetContext();
  Node* native_context = LoadNativeContext(context);
  Node* function = LoadContextElement(native_context, context_index);

  // Call the function.
  Node* result = CallJS(function, context, first_arg, args_count,
                        ConvertReceiverMode::kAny, TailCallMode::kDisallow);
  SetAccumulator(result);
  Dispatch();
}

// CallWithSpread <callable> <first_arg> <arg_count>
//
// Call a JSfunction or Callable in |callable| with the receiver in
// |first_arg| and |arg_count - 1| arguments in subsequent registers. The
// final argument is always a spread.
//
IGNITION_HANDLER(CallWithSpread, InterpreterAssembler) {
  Node* callable_reg = BytecodeOperandReg(0);
  Node* callable = LoadRegister(callable_reg);
  Node* receiver_reg = BytecodeOperandReg(1);
  Node* receiver_arg = RegisterLocation(receiver_reg);
  Node* receiver_args_count = BytecodeOperandCount(2);
  Node* receiver_count = Int32Constant(1);
  Node* args_count = Int32Sub(receiver_args_count, receiver_count);
  Node* context = GetContext();

  // Call into Runtime function CallWithSpread which does everything.
  Node* result = CallJSWithSpread(callable, context, receiver_arg, args_count);
  SetAccumulator(result);
  Dispatch();
}

// ConstructWithSpread <first_arg> <arg_count>
//
// Call the constructor in |constructor| with the first argument in register
// |first_arg| and |arg_count| arguments in subsequent registers. The final
// argument is always a spread. The new.target is in the accumulator.
//
IGNITION_HANDLER(ConstructWithSpread, InterpreterAssembler) {
  Node* new_target = GetAccumulator();
  Node* constructor_reg = BytecodeOperandReg(0);
  Node* constructor = LoadRegister(constructor_reg);
  Node* first_arg_reg = BytecodeOperandReg(1);
  Node* first_arg = RegisterLocation(first_arg_reg);
  Node* args_count = BytecodeOperandCount(2);
  Node* context = GetContext();
  Node* result = ConstructWithSpread(constructor, context, new_target,
                                     first_arg, args_count);
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
  Node* constructor_reg = BytecodeOperandReg(0);
  Node* constructor = LoadRegister(constructor_reg);
  Node* first_arg_reg = BytecodeOperandReg(1);
  Node* first_arg = RegisterLocation(first_arg_reg);
  Node* args_count = BytecodeOperandCount(2);
  Node* slot_id = BytecodeOperandIdx(3);
  Node* feedback_vector = LoadFeedbackVector();
  Node* context = GetContext();
  Node* result = Construct(constructor, context, new_target, first_arg,
                           args_count, slot_id, feedback_vector);
  SetAccumulator(result);
  Dispatch();
}

class InterpreterCompareOpAssembler : public InterpreterAssembler {
 public:
  InterpreterCompareOpAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void CompareOpWithFeedback(Token::Value compare_op) {
    Node* reg_index = BytecodeOperandReg(0);
    Node* lhs = LoadRegister(reg_index);
    Node* rhs = GetAccumulator();
    Node* context = GetContext();

    Variable var_type_feedback(this, MachineRepresentation::kTagged);
    Node* result;
    switch (compare_op) {
      case Token::EQ:
        result = Equal(lhs, rhs, context, &var_type_feedback);
        break;
      case Token::EQ_STRICT:
        result = StrictEqual(lhs, rhs, &var_type_feedback);
        break;
      case Token::LT:
        result = RelationalComparison(CodeStubAssembler::kLessThan, lhs, rhs,
                                      context, &var_type_feedback);
        break;
      case Token::GT:
        result = RelationalComparison(CodeStubAssembler::kGreaterThan, lhs, rhs,
                                      context, &var_type_feedback);
        break;
      case Token::LTE:
        result = RelationalComparison(CodeStubAssembler::kLessThanOrEqual, lhs,
                                      rhs, context, &var_type_feedback);
        break;
      case Token::GTE:
        result = RelationalComparison(CodeStubAssembler::kGreaterThanOrEqual,
                                      lhs, rhs, context, &var_type_feedback);
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
  CompareOpWithFeedback(Token::Value::EQ);
}

// TestEqualStrict <src>
//
// Test if the value in the <src> register is strictly equal to the accumulator.
IGNITION_HANDLER(TestEqualStrict, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Token::Value::EQ_STRICT);
}

// TestLessThan <src>
//
// Test if the value in the <src> register is less than the accumulator.
IGNITION_HANDLER(TestLessThan, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Token::Value::LT);
}

// TestGreaterThan <src>
//
// Test if the value in the <src> register is greater than the accumulator.
IGNITION_HANDLER(TestGreaterThan, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Token::Value::GT);
}

// TestLessThanOrEqual <src>
//
// Test if the value in the <src> register is less than or equal to the
// accumulator.
IGNITION_HANDLER(TestLessThanOrEqual, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Token::Value::LTE);
}

// TestGreaterThanOrEqual <src>
//
// Test if the value in the <src> register is greater than or equal to the
// accumulator.
IGNITION_HANDLER(TestGreaterThanOrEqual, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Token::Value::GTE);
}

// TestEqualStrictNoFeedback <src>
//
// Test if the value in the <src> register is strictly equal to the accumulator.
// Type feedback is not collected.
IGNITION_HANDLER(TestEqualStrictNoFeedback, InterpreterAssembler) {
  Node* reg_index = BytecodeOperandReg(0);
  Node* lhs = LoadRegister(reg_index);
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
  Node* reg_index = BytecodeOperandReg(0);
  Node* property = LoadRegister(reg_index);
  Node* object = GetAccumulator();
  Node* context = GetContext();
  SetAccumulator(HasProperty(object, property, context));
  Dispatch();
}

// TestInstanceOf <src>
//
// Test if the object referenced by the <src> register is an an instance of type
// referenced by the accumulator.
IGNITION_HANDLER(TestInstanceOf, InterpreterAssembler) {
  Node* reg_index = BytecodeOperandReg(0);
  Node* name = LoadRegister(reg_index);
  Node* object = GetAccumulator();
  Node* context = GetContext();
  SetAccumulator(InstanceOf(name, object, context));
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
  SetAccumulator(BooleanConstant(false));
  GotoIf(TaggedIsSmi(object), &end);

  // If it is a HeapObject, load the map and check for undetectable bit.
  Node* map = LoadMap(object);
  Node* map_bitfield = LoadMapBitField(map);
  Node* map_undetectable =
      Word32And(map_bitfield, Int32Constant(1 << Map::kIsUndetectable));
  Node* result =
      SelectBooleanConstant(Word32NotEqual(map_undetectable, Int32Constant(0)));
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
  Node* null_value = HeapConstant(isolate()->factory()->null_value());
  Node* result = SelectBooleanConstant(WordEqual(object, null_value));
  SetAccumulator(result);
  Dispatch();
}

// TestUndefined
//
// Test if the value in the accumulator is strictly equal to undefined.
IGNITION_HANDLER(TestUndefined, InterpreterAssembler) {
  Node* object = GetAccumulator();
  Node* undefined_value = HeapConstant(isolate()->factory()->undefined_value());
  Node* result = SelectBooleanConstant(WordEqual(object, undefined_value));
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

  Label if_true(this), if_false(this), end(this), abort(this, Label::kDeferred);

  Switch(literal_flag, &abort, cases, labels, arraysize(cases));

  BIND(&abort);
  {
    Comment("Abort");
    Abort(BailoutReason::kUnexpectedTestTypeofLiteralFlag);
    Goto(&if_false);
  }
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
    GotoIf(WordEqual(object, BooleanConstant(true)), &if_true);
    Branch(WordEqual(object, BooleanConstant(false)), &if_true, &if_false);
  }
  BIND(&if_undefined);
  {
    Comment("IfUndefined");
    GotoIf(TaggedIsSmi(object), &if_false);
    // Check it is not null and the map has the undetectable bit set.
    GotoIf(WordEqual(object, NullConstant()), &if_false);
    Node* map_bitfield = LoadMapBitField(LoadMap(object));
    Node* undetectable_bit =
        Word32And(map_bitfield, Int32Constant(1 << Map::kIsUndetectable));
    Branch(Word32Equal(undetectable_bit, Int32Constant(0)), &if_false,
           &if_true);
  }
  BIND(&if_function);
  {
    Comment("IfFunction");
    GotoIf(TaggedIsSmi(object), &if_false);
    // Check if callable bit is set and not undetectable.
    Node* map_bitfield = LoadMapBitField(LoadMap(object));
    Node* callable_undetectable = Word32And(
        map_bitfield,
        Int32Constant(1 << Map::kIsUndetectable | 1 << Map::kIsCallable));
    Branch(Word32Equal(callable_undetectable,
                       Int32Constant(1 << Map::kIsCallable)),
           &if_true, &if_false);
  }
  BIND(&if_object);
  {
    Comment("IfObject");
    GotoIf(TaggedIsSmi(object), &if_false);

    // If the object is null then return true.
    GotoIf(WordEqual(object, NullConstant()), &if_true);

    // Check if the object is a receiver type and is not undefined or callable.
    Node* map = LoadMap(object);
    GotoIfNot(IsJSReceiverMap(map), &if_false);
    Node* map_bitfield = LoadMapBitField(map);
    Node* callable_undetectable = Word32And(
        map_bitfield,
        Int32Constant(1 << Map::kIsUndetectable | 1 << Map::kIsCallable));
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
    SetAccumulator(BooleanConstant(false));
    Goto(&end);
  }
  BIND(&if_true);
  {
    SetAccumulator(BooleanConstant(true));
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
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
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
  Node* true_value = BooleanConstant(true);
  CSA_ASSERT(this, TaggedIsNotSmi(accumulator));
  CSA_ASSERT(this, IsBoolean(accumulator));
  JumpIfWordEqual(accumulator, true_value, relative_jump);
}

// JumpIfTrueConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the accumulator contains true. This only works for boolean inputs,
// and will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfTrueConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
  Node* true_value = BooleanConstant(true);
  CSA_ASSERT(this, TaggedIsNotSmi(accumulator));
  CSA_ASSERT(this, IsBoolean(accumulator));
  JumpIfWordEqual(accumulator, true_value, relative_jump);
}

// JumpIfFalse <imm>
//
// Jump by the number of bytes represented by an immediate operand if the
// accumulator contains false. This only works for boolean inputs, and
// will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfFalse, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* relative_jump = BytecodeOperandUImmWord(0);
  Node* false_value = BooleanConstant(false);
  CSA_ASSERT(this, TaggedIsNotSmi(accumulator));
  CSA_ASSERT(this, IsBoolean(accumulator));
  JumpIfWordEqual(accumulator, false_value, relative_jump);
}

// JumpIfFalseConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the accumulator contains false. This only works for boolean inputs,
// and will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfFalseConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
  Node* false_value = BooleanConstant(false);
  CSA_ASSERT(this, TaggedIsNotSmi(accumulator));
  CSA_ASSERT(this, IsBoolean(accumulator));
  JumpIfWordEqual(accumulator, false_value, relative_jump);
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
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
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
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
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
  Node* null_value = HeapConstant(isolate()->factory()->null_value());
  Node* relative_jump = BytecodeOperandUImmWord(0);
  JumpIfWordEqual(accumulator, null_value, relative_jump);
}

// JumpIfNullConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is the null constant.
IGNITION_HANDLER(JumpIfNullConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* null_value = HeapConstant(isolate()->factory()->null_value());
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
  JumpIfWordEqual(accumulator, null_value, relative_jump);
}

// JumpIfNotNull <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is not the null constant.
IGNITION_HANDLER(JumpIfNotNull, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* null_value = HeapConstant(isolate()->factory()->null_value());
  Node* relative_jump = BytecodeOperandUImmWord(0);
  JumpIfWordNotEqual(accumulator, null_value, relative_jump);
}

// JumpIfNotNullConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is not the null constant.
IGNITION_HANDLER(JumpIfNotNullConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* null_value = HeapConstant(isolate()->factory()->null_value());
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
  JumpIfWordNotEqual(accumulator, null_value, relative_jump);
}

// JumpIfUndefined <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the undefined constant.
IGNITION_HANDLER(JumpIfUndefined, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* undefined_value = HeapConstant(isolate()->factory()->undefined_value());
  Node* relative_jump = BytecodeOperandUImmWord(0);
  JumpIfWordEqual(accumulator, undefined_value, relative_jump);
}

// JumpIfUndefinedConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is the undefined constant.
IGNITION_HANDLER(JumpIfUndefinedConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* undefined_value = HeapConstant(isolate()->factory()->undefined_value());
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
  JumpIfWordEqual(accumulator, undefined_value, relative_jump);
}

// JumpIfNotUndefined <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is not the undefined constant.
IGNITION_HANDLER(JumpIfNotUndefined, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* undefined_value = HeapConstant(isolate()->factory()->undefined_value());
  Node* relative_jump = BytecodeOperandUImmWord(0);
  JumpIfWordNotEqual(accumulator, undefined_value, relative_jump);
}

// JumpIfNotUndefinedConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is not the undefined
// constant.
IGNITION_HANDLER(JumpIfNotUndefinedConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* undefined_value = HeapConstant(isolate()->factory()->undefined_value());
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
  JumpIfWordNotEqual(accumulator, undefined_value, relative_jump);
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
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);

  Label if_object(this), if_notobject(this), if_notsmi(this);
  Branch(TaggedIsSmi(accumulator), &if_notobject, &if_notsmi);

  BIND(&if_notsmi);
  Branch(IsJSReceiver(accumulator), &if_object, &if_notobject);

  BIND(&if_object);
  Jump(relative_jump);

  BIND(&if_notobject);
  Dispatch();
}

// JumpIfNotHole <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the hole.
IGNITION_HANDLER(JumpIfNotHole, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* the_hole_value = HeapConstant(isolate()->factory()->the_hole_value());
  Node* relative_jump = BytecodeOperandUImmWord(0);
  JumpIfWordNotEqual(accumulator, the_hole_value, relative_jump);
}

// JumpIfNotHoleConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is the hole constant.
IGNITION_HANDLER(JumpIfNotHoleConstant, InterpreterAssembler) {
  Node* accumulator = GetAccumulator();
  Node* the_hole_value = HeapConstant(isolate()->factory()->the_hole_value());
  Node* index = BytecodeOperandIdx(0);
  Node* relative_jump = LoadAndUntagConstantPoolEntry(index);
  JumpIfWordNotEqual(accumulator, the_hole_value, relative_jump);
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
  Node* index = BytecodeOperandIdx(0);
  Node* pattern = LoadConstantPoolEntry(index);
  Node* literal_index = BytecodeOperandIdxSmi(1);
  Node* flags = SmiFromWord32(BytecodeOperandFlag(2));
  Node* closure = LoadRegister(Register::function_closure());
  Node* context = GetContext();
  ConstructorBuiltinsAssembler constructor_assembler(state());
  Node* result = constructor_assembler.EmitFastCloneRegExp(
      closure, literal_index, pattern, flags, context);
  SetAccumulator(result);
  Dispatch();
}

// CreateArrayLiteral <element_idx> <literal_idx> <flags>
//
// Creates an array literal for literal index <literal_idx> with
// CreateArrayLiteral flags <flags> and constant elements in <element_idx>.
IGNITION_HANDLER(CreateArrayLiteral, InterpreterAssembler) {
  Node* literal_index = BytecodeOperandIdxSmi(1);
  Node* closure = LoadRegister(Register::function_closure());
  Node* context = GetContext();
  Node* bytecode_flags = BytecodeOperandFlag(2);

  Label fast_shallow_clone(this), call_runtime(this, Label::kDeferred);
  Branch(
      IsSetWord32<CreateArrayLiteralFlags::FastShallowCloneBit>(bytecode_flags),
      &fast_shallow_clone, &call_runtime);

  BIND(&fast_shallow_clone);
  {
    ConstructorBuiltinsAssembler constructor_assembler(state());
    Node* result = constructor_assembler.EmitFastCloneShallowArray(
        closure, literal_index, context, &call_runtime, TRACK_ALLOCATION_SITE);
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&call_runtime);
  {
    Node* flags_raw = DecodeWordFromWord32<CreateArrayLiteralFlags::FlagsBits>(
        bytecode_flags);
    Node* flags = SmiTag(flags_raw);
    Node* index = BytecodeOperandIdx(0);
    Node* constant_elements = LoadConstantPoolEntry(index);
    Node* result = CallRuntime(Runtime::kCreateArrayLiteral, context, closure,
                               literal_index, constant_elements, flags);
    SetAccumulator(result);
    Dispatch();
  }
}

// CreateObjectLiteral <element_idx> <literal_idx> <flags>
//
// Creates an object literal for literal index <literal_idx> with
// CreateObjectLiteralFlags <flags> and constant elements in <element_idx>.
IGNITION_HANDLER(CreateObjectLiteral, InterpreterAssembler) {
  Node* literal_index = BytecodeOperandIdxSmi(1);
  Node* bytecode_flags = BytecodeOperandFlag(2);
  Node* closure = LoadRegister(Register::function_closure());

  // Check if we can do a fast clone or have to call the runtime.
  Label if_fast_clone(this), if_not_fast_clone(this, Label::kDeferred);
  Branch(IsSetWord32<CreateObjectLiteralFlags::FastCloneSupportedBit>(
             bytecode_flags),
         &if_fast_clone, &if_not_fast_clone);

  BIND(&if_fast_clone);
  {
    // If we can do a fast clone do the fast-path in FastCloneShallowObjectStub.
    ConstructorBuiltinsAssembler constructor_assembler(state());
    Node* result = constructor_assembler.EmitFastCloneShallowObject(
        &if_not_fast_clone, closure, literal_index);
    StoreRegister(result, BytecodeOperandReg(3));
    Dispatch();
  }

  BIND(&if_not_fast_clone);
  {
    // If we can't do a fast clone, call into the runtime.
    Node* index = BytecodeOperandIdx(0);
    Node* constant_elements = LoadConstantPoolEntry(index);
    Node* context = GetContext();

    Node* flags_raw = DecodeWordFromWord32<CreateObjectLiteralFlags::FlagsBits>(
        bytecode_flags);
    Node* flags = SmiTag(flags_raw);

    Node* result = CallRuntime(Runtime::kCreateObjectLiteral, context, closure,
                               literal_index, constant_elements, flags);
    StoreRegister(result, BytecodeOperandReg(3));
    // TODO(klaasb) build a single dispatch once the call is inlined
    Dispatch();
  }
}

// CreateClosure <index> <slot> <tenured>
//
// Creates a new closure for SharedFunctionInfo at position |index| in the
// constant pool and with the PretenureFlag <tenured>.
IGNITION_HANDLER(CreateClosure, InterpreterAssembler) {
  Node* index = BytecodeOperandIdx(0);
  Node* shared = LoadConstantPoolEntry(index);
  Node* flags = BytecodeOperandFlag(2);
  Node* context = GetContext();

  Label call_runtime(this, Label::kDeferred);
  GotoIfNot(IsSetWord32<CreateClosureFlags::FastNewClosureBit>(flags),
            &call_runtime);
  ConstructorBuiltinsAssembler constructor_assembler(state());
  Node* vector_index = BytecodeOperandIdx(1);
  vector_index = SmiTag(vector_index);
  Node* feedback_vector = LoadFeedbackVector();
  SetAccumulator(constructor_assembler.EmitFastNewClosure(
      shared, feedback_vector, vector_index, context));
  Dispatch();

  BIND(&call_runtime);
  {
    Node* tenured_raw =
        DecodeWordFromWord32<CreateClosureFlags::PretenuredBit>(flags);
    Node* tenured = SmiTag(tenured_raw);
    feedback_vector = LoadFeedbackVector();
    vector_index = BytecodeOperandIdx(1);
    vector_index = SmiTag(vector_index);
    Node* result = CallRuntime(Runtime::kInterpreterNewClosure, context, shared,
                               feedback_vector, vector_index, tenured);
    SetAccumulator(result);
    Dispatch();
  }
}

// CreateBlockContext <index>
//
// Creates a new block context with the scope info constant at |index| and the
// closure in the accumulator.
IGNITION_HANDLER(CreateBlockContext, InterpreterAssembler) {
  Node* index = BytecodeOperandIdx(0);
  Node* scope_info = LoadConstantPoolEntry(index);
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
  Node* exception_reg = BytecodeOperandReg(0);
  Node* exception = LoadRegister(exception_reg);
  Node* name_idx = BytecodeOperandIdx(1);
  Node* name = LoadConstantPoolEntry(name_idx);
  Node* scope_info_idx = BytecodeOperandIdx(2);
  Node* scope_info = LoadConstantPoolEntry(scope_info_idx);
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
  Node* reg_index = BytecodeOperandReg(0);
  Node* object = LoadRegister(reg_index);
  Node* scope_info_idx = BytecodeOperandIdx(1);
  Node* scope_info = LoadConstantPoolEntry(scope_info_idx);
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
  Node* compiler_hints = LoadObjectField(
      shared_info, SharedFunctionInfo::kHasDuplicateParametersByteOffset,
      MachineType::Uint8());
  Node* duplicate_parameters_bit = Int32Constant(
      1 << SharedFunctionInfo::kHasDuplicateParametersBitWithinByte);
  Node* compare = Word32And(compiler_hints, duplicate_parameters_bit);
  Branch(compare, &if_duplicate_parameters, &if_not_duplicate_parameters);

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
  Label ok(this), stack_check_interrupt(this, Label::kDeferred);

  Node* interrupt = StackCheckTriggeredInterrupt();
  Branch(interrupt, &stack_check_interrupt, &ok);

  BIND(&ok);
  Dispatch();

  BIND(&stack_check_interrupt);
  {
    Node* context = GetContext();
    CallRuntime(Runtime::kStackGuard, context);
    Dispatch();
  }
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
  Abort(kUnexpectedReturnFromThrow);
}

// ReThrow
//
// Re-throws the exception in the accumulator.
IGNITION_HANDLER(ReThrow, InterpreterAssembler) {
  Node* exception = GetAccumulator();
  Node* context = GetContext();
  CallRuntime(Runtime::kReThrow, context, exception);
  // We shouldn't ever return from a throw.
  Abort(kUnexpectedReturnFromThrow);
}

// Return
//
// Return the value in the accumulator.
IGNITION_HANDLER(Return, InterpreterAssembler) {
  UpdateInterruptBudgetOnReturn();
  Node* accumulator = GetAccumulator();
  Return(accumulator);
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
    Node* original_handler =                                               \
        CallRuntime(Runtime::kDebugBreakOnBytecode, context, accumulator); \
    MaybeDropFrames(context);                                              \
    DispatchToBytecodeHandler(original_handler);                           \
  }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK);
#undef DEBUG_BREAK

class InterpreterForInPrepareAssembler : public InterpreterAssembler {
 public:
  InterpreterForInPrepareAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                   OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void BuildForInPrepareResult(Node* output_register, Node* cache_type,
                               Node* cache_array, Node* cache_length) {
    StoreRegister(cache_type, output_register);
    output_register = NextRegister(output_register);
    StoreRegister(cache_array, output_register);
    output_register = NextRegister(output_register);
    StoreRegister(cache_length, output_register);
  }
};

// ForInPrepare <receiver> <cache_info_triple>
//
// Returns state for for..in loop execution based on the object in the register
// |receiver|. The object must not be null or undefined and must have been
// converted to a receiver already.
// The result is output in registers |cache_info_triple| to
// |cache_info_triple + 2|, with the registers holding cache_type, cache_array,
// and cache_length respectively.
IGNITION_HANDLER(ForInPrepare, InterpreterForInPrepareAssembler) {
  Node* object_register = BytecodeOperandReg(0);
  Node* output_register = BytecodeOperandReg(1);
  Node* receiver = LoadRegister(object_register);
  Node* context = GetContext();

  Node* cache_type;
  Node* cache_array;
  Node* cache_length;
  Label call_runtime(this, Label::kDeferred),
      nothing_to_iterate(this, Label::kDeferred);

  ForInBuiltinsAssembler forin_assembler(state());
  std::tie(cache_type, cache_array, cache_length) =
      forin_assembler.EmitForInPrepare(receiver, context, &call_runtime,
                                       &nothing_to_iterate);

  BuildForInPrepareResult(output_register, cache_type, cache_array,
                          cache_length);
  Dispatch();

  BIND(&call_runtime);
  {
    Node* result_triple =
        CallRuntime(Runtime::kForInPrepare, context, receiver);
    Node* cache_type = Projection(0, result_triple);
    Node* cache_array = Projection(1, result_triple);
    Node* cache_length = Projection(2, result_triple);
    BuildForInPrepareResult(output_register, cache_type, cache_array,
                            cache_length);
    Dispatch();
  }
  BIND(&nothing_to_iterate);
  {
    // Receiver is null or undefined or descriptors are zero length.
    Node* zero = SmiConstant(0);
    BuildForInPrepareResult(output_register, zero, zero, zero);
    Dispatch();
  }
}

// ForInNext <receiver> <index> <cache_info_pair>
//
// Returns the next enumerable property in the the accumulator.
IGNITION_HANDLER(ForInNext, InterpreterAssembler) {
  Node* receiver_reg = BytecodeOperandReg(0);
  Node* receiver = LoadRegister(receiver_reg);
  Node* index_reg = BytecodeOperandReg(1);
  Node* index = LoadRegister(index_reg);
  Node* cache_type_reg = BytecodeOperandReg(2);
  Node* cache_type = LoadRegister(cache_type_reg);
  Node* cache_array_reg = NextRegister(cache_type_reg);
  Node* cache_array = LoadRegister(cache_array_reg);

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
    // Record the fact that we hit the for-in slow path.
    Node* vector_index = BytecodeOperandIdx(3);
    Node* feedback_vector = LoadFeedbackVector();
    Node* megamorphic_sentinel =
        HeapConstant(FeedbackVector::MegamorphicSentinel(isolate()));
    StoreFixedArrayElement(feedback_vector, vector_index, megamorphic_sentinel,
                           SKIP_WRITE_BARRIER);

    // Need to filter the {key} for the {receiver}.
    Node* context = GetContext();
    Callable callable = CodeFactory::ForInFilter(isolate());
    Node* result = CallStub(callable, context, key, receiver);
    SetAccumulator(result);
    Dispatch();
  }
}

// ForInContinue <index> <cache_length>
//
// Returns false if the end of the enumerable properties has been reached.
IGNITION_HANDLER(ForInContinue, InterpreterAssembler) {
  Node* index_reg = BytecodeOperandReg(0);
  Node* index = LoadRegister(index_reg);
  Node* cache_length_reg = BytecodeOperandReg(1);
  Node* cache_length = LoadRegister(cache_length_reg);

  // Check if {index} is at {cache_length} already.
  Label if_true(this), if_false(this), end(this);
  Branch(WordEqual(index, cache_length), &if_true, &if_false);
  BIND(&if_true);
  {
    SetAccumulator(BooleanConstant(false));
    Goto(&end);
  }
  BIND(&if_false);
  {
    SetAccumulator(BooleanConstant(true));
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
  Node* index_reg = BytecodeOperandReg(0);
  Node* index = LoadRegister(index_reg);
  Node* one = SmiConstant(Smi::FromInt(1));
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
IGNITION_HANDLER(Illegal, InterpreterAssembler) { Abort(kInvalidBytecode); }

// Nop
//
// No operation.
IGNITION_HANDLER(Nop, InterpreterAssembler) { Dispatch(); }

// SuspendGenerator <generator>
//
// Exports the register file and stores it into the generator.  Also stores the
// current context, the state given in the accumulator, and the current bytecode
// offset (for debugging purposes) into the generator.
IGNITION_HANDLER(SuspendGenerator, InterpreterAssembler) {
  Node* generator_reg = BytecodeOperandReg(0);
  Node* flags = BytecodeOperandFlag(1);
  Node* generator = LoadRegister(generator_reg);

  Label if_stepping(this, Label::kDeferred), ok(this);
  Node* step_action_address = ExternalConstant(
      ExternalReference::debug_last_step_action_address(isolate()));
  Node* step_action = Load(MachineType::Int8(), step_action_address);
  STATIC_ASSERT(StepIn > StepNext);
  STATIC_ASSERT(LastStepAction == StepIn);
  Node* step_next = Int32Constant(StepNext);
  Branch(Int32LessThanOrEqual(step_next, step_action), &if_stepping, &ok);
  BIND(&ok);

  Node* array =
      LoadObjectField(generator, JSGeneratorObject::kRegisterFileOffset);
  Node* context = GetContext();
  Node* state = GetAccumulator();

  ExportRegisterFile(array);
  StoreObjectField(generator, JSGeneratorObject::kContextOffset, context);
  StoreObjectField(generator, JSGeneratorObject::kContinuationOffset, state);

  Label if_asyncgeneratorawait(this), if_notasyncgeneratorawait(this),
      merge(this);

  // Calculate bytecode offset to store in the [input_or_debug_pos] or
  // [await_input_or_debug_pos] fields, to be used by the inspector.
  Node* offset = SmiTag(BytecodeOffset());

  using AsyncGeneratorAwaitBits = SuspendGeneratorBytecodeFlags::FlagsBits;
  Branch(Word32Equal(DecodeWord32<AsyncGeneratorAwaitBits>(flags),
                     Int32Constant(
                         static_cast<int>(SuspendFlags::kAsyncGeneratorAwait))),
         &if_asyncgeneratorawait, &if_notasyncgeneratorawait);

  BIND(&if_notasyncgeneratorawait);
  {
    // For ordinary yields (and for AwaitExpressions in Async Functions, which
    // are implemented as ordinary yields), it is safe to write over the
    // [input_or_debug_pos] field.
    StoreObjectField(generator, JSGeneratorObject::kInputOrDebugPosOffset,
                     offset);
    Goto(&merge);
  }

  BIND(&if_asyncgeneratorawait);
  {
    // An AwaitExpression in an Async Generator requires writing to the
    // [await_input_or_debug_pos] field.
    CSA_ASSERT(this,
               HasInstanceType(generator, JS_ASYNC_GENERATOR_OBJECT_TYPE));
    StoreObjectField(
        generator, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset, offset);
    Goto(&merge);
  }

  BIND(&merge);
  Dispatch();

  BIND(&if_stepping);
  {
    Node* context = GetContext();
    CallRuntime(Runtime::kDebugRecordGenerator, context, generator);
    Goto(&ok);
  }
}

// ResumeGenerator <generator>
//
// Imports the register file stored in the generator. Also loads the
// generator's state and stores it in the accumulator, before overwriting it
// with kGeneratorExecuting.
IGNITION_HANDLER(ResumeGenerator, InterpreterAssembler) {
  Node* generator_reg = BytecodeOperandReg(0);
  Node* generator = LoadRegister(generator_reg);

  ImportRegisterFile(
      LoadObjectField(generator, JSGeneratorObject::kRegisterFileOffset));

  Node* old_state =
      LoadObjectField(generator, JSGeneratorObject::kContinuationOffset);
  Node* new_state = Int32Constant(JSGeneratorObject::kGeneratorExecuting);
  StoreObjectField(generator, JSGeneratorObject::kContinuationOffset,
                   SmiTag(new_state));
  SetAccumulator(old_state);

  Dispatch();
}

}  // namespace

Handle<Code> GenerateBytecodeHandler(Isolate* isolate, Bytecode bytecode,
                                     OperandScale operand_scale) {
  Zone zone(isolate->allocator(), ZONE_NAME);
  InterpreterDispatchDescriptor descriptor(isolate);
  compiler::CodeAssemblerState state(
      isolate, &zone, descriptor, Code::ComputeFlags(Code::BYTECODE_HANDLER),
      Bytecodes::ToString(bytecode), Bytecodes::ReturnCount(bytecode));

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

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
