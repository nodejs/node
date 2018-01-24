// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/base/utils/random-number-generator.h"
#include "src/code-stub-assembler.h"
#include "src/codegen.h"
#include "src/compilation-info.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/instruction.h"
#include "src/compiler/linkage.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/code-assembler-tester.h"
#include "test/cctest/compiler/function-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ assembler.

namespace {

int GetSlotSizeInBytes(MachineRepresentation rep) {
  switch (rep) {
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kFloat32:
      return kPointerSize;
    case MachineRepresentation::kFloat64:
      return kDoubleSize;
    case MachineRepresentation::kSimd128:
      return kSimd128Size;
    default:
      break;
  }
  UNREACHABLE();
}

// Forward declaration.
Handle<Code> BuildTeardownFunction(Isolate* isolate, CallDescriptor* descriptor,
                                   std::vector<AllocatedOperand> parameters);

// Build the `setup` function. It takes a code object and a FixedArray as
// parameters and calls the former while passing it each element of the array as
// arguments:
// ~~~
// FixedArray setup(CodeObject* test, FixedArray state_in) {
//   // `test` will tail-call to its first parameter which will be `teardown`.
//   return test(teardown, state_in[0], state_in[1], state_in[2], ...);
// }
// ~~~
//
// This function needs to convert each element of the FixedArray to raw unboxed
// values to pass to the `test` function. The array will have been created using
// `GenerateInitialState()` and needs to be converted in the following way:
//
// | Parameter type | FixedArray element | Conversion                         |
// |----------------+--------------------+------------------------------------|
// | kTagged        | Smi                | None.                              |
// | kFloat32       | HeapNumber         | Load value and convert to Float32. |
// | kFloat64       | HeapNumber         | Load value.                        |
//
Handle<Code> BuildSetupFunction(Isolate* isolate, CallDescriptor* descriptor,
                                std::vector<AllocatedOperand> parameters) {
  CodeAssemblerTester tester(isolate, 2);
  CodeStubAssembler assembler(tester.state());
  std::vector<Node*> params;
  // The first parameter is always the callee.
  params.push_back(__ Parameter(0));
  params.push_back(
      __ HeapConstant(BuildTeardownFunction(isolate, descriptor, parameters)));
  Node* state_in = __ Parameter(1);
  for (int i = 0; i < static_cast<int>(parameters.size()); i++) {
    Node* element = __ LoadFixedArrayElement(state_in, __ IntPtrConstant(i));
    // Unbox all elements before passing them as arguments.
    switch (parameters[i].representation()) {
      // Tagged parameters are Smis, they do not need unboxing.
      case MachineRepresentation::kTagged:
        break;
      case MachineRepresentation::kFloat32:
        element = __ TruncateFloat64ToFloat32(__ LoadHeapNumberValue(element));
        break;
      case MachineRepresentation::kFloat64:
        element = __ LoadHeapNumberValue(element);
        break;
      default:
        UNREACHABLE();
        break;
    }
    params.push_back(element);
  }
  __ Return(tester.raw_assembler_for_testing()->AddNode(
      tester.raw_assembler_for_testing()->common()->Call(descriptor),
      static_cast<int>(params.size()), params.data()));
  return tester.GenerateCodeCloseAndEscape();
}

// Build the `teardown` function. It allocates and fills a FixedArray with all
// its parameters. The parameters need to be consistent with `parameters`.
// ~~~
// FixedArray teardown(CodeObject* /* unused  */,
//                     // Tagged registers.
//                     Object* r0, Object* r1, ...,
//                     // FP registers.
//                     Float32 s0, Float64 d1, ...,
//                     // Mixed stack slots.
//                     Float64 mem0, Object* mem1, Float32 mem2, ...) {
//   return new FixedArray(r0, r1, ..., s0, d1, ..., mem0, mem1, mem2, ...);
// }
// ~~~
//
// This function needs to convert its parameters into values fit for a
// FixedArray, essentially reverting what the `setup` function did:
//
// | Parameter type | Parameter value   | Conversion                 |
// |----------------+-------------------+----------------------------|
// | kTagged        | Smi or HeapNumber | None.                      |
// | kFloat32       | Raw Float32       | Convert to Float64 and     |
// |                |                   | allocate a new HeapNumber. |
// | kFloat64       | Raw Float64       | Allocate a new HeapNumber. |
//
// Note that it is possible for a `kTagged` value to go from a Smi to a
// HeapNumber. This is because `AssembleMove` will allocate a new HeapNumber if
// it is asked to move a FP constant to a tagged register or slot.
//
Handle<Code> BuildTeardownFunction(Isolate* isolate, CallDescriptor* descriptor,
                                   std::vector<AllocatedOperand> parameters) {
  CodeAssemblerTester tester(isolate, descriptor);
  CodeStubAssembler assembler(tester.state());
  Node* result_array = __ AllocateFixedArray(
      PACKED_ELEMENTS, __ IntPtrConstant(parameters.size()));
  for (int i = 0; i < static_cast<int>(parameters.size()); i++) {
    // The first argument is not used.
    Node* param = __ Parameter(i + 1);
    switch (parameters[i].representation()) {
      case MachineRepresentation::kTagged:
        break;
      // Box FP values into HeapNumbers.
      case MachineRepresentation::kFloat32:
        param =
            tester.raw_assembler_for_testing()->ChangeFloat32ToFloat64(param);
      // Fallthrough
      case MachineRepresentation::kFloat64:
        param = __ AllocateHeapNumberWithValue(param);
        break;
      default:
        UNREACHABLE();
        break;
    }
    __ StoreFixedArrayElement(result_array, i, param);
  }
  __ Return(result_array);
  return tester.GenerateCodeCloseAndEscape();
}

// Print the content of `value`, representing the register or stack slot
// described by `operand`.
void PrintStateValue(std::ostream& os, Handle<Object> value,
                     AllocatedOperand operand) {
  switch (operand.representation()) {
    case MachineRepresentation::kTagged:
      if (value->IsSmi()) {
        os << Smi::cast(*value)->value();
      } else {
        os << value->Number();
      }
      break;
    case MachineRepresentation::kFloat32:
    case MachineRepresentation::kFloat64:
      os << value->Number();
      break;
    default:
      UNREACHABLE();
      break;
  }
  os << " (" << operand.representation() << " ";
  if (operand.location_kind() == AllocatedOperand::REGISTER) {
    os << "register";
  } else {
    DCHECK_EQ(operand.location_kind(), AllocatedOperand::STACK_SLOT);
    os << "stack slot";
  }
  os << ")";
}

}  // namespace

#undef __

// Representation of a test environment. It describes a set of registers, stack
// slots and constants available to the CodeGeneratorTester to perform moves
// with. It has the ability to randomly generate lists of moves and run the code
// generated by the CodeGeneratorTester.
//
// At the moment, only the following representations are tested:
//   - kTagged
//   - kFloat32
//   - kFloat64
//   - TODO(planglois): Add support for kSimd128.
// There is no need to test using Word32 or Word64 as they are the same as
// Tagged as far as the code generator is concerned.
//
// Testing the generated code is achieved by wrapping it around `setup` and
// `teardown` functions, written using the CodeStubAssembler. The key idea here
// is that `teardown` and the generated code share the same custom
// CallDescriptor. This descriptor assigns parameters to either registers or
// stack slot of a given representation and therefore essentially describes the
// environment.
//
// What happens is the following:
//
//   - The `setup` function receives a FixedArray as the initial state. It
//     unpacks it and passes each element as arguments to the generated code
//     `test`. We also pass the `teardown` function as a first argument. Thanks
//     to the custom CallDescriptor, registers and stack slots get initialised
//     according to the content of the FixedArray.
//
//   - The `test` function performs the list of moves on its parameters and
//     eventually tail-calls to its first parameter, which is the `teardown`
//     function.
//
//   - The `teardown` function allocates a new FixedArray and fills it with all
//     its parameters. Thanks to the tail-call, this is as if the `setup`
//     function called `teardown` directly, except now moves were performed!
//
// .----------------setup--------------------------.
// | Take a FixedArray as parameters with          |
// | all the initial values of registers           |
// | and stack slots.                              | <- CodeStubAssembler
// |                                               |
// | Call test(teardown, state[0], state[1], ...); |
// '-----------------------------------------------'
//   |
//   V
// .----------------test-----------------------------.
// | - Move(param3, param42);                        |
// | - Swap(param64, param1);                        |
// | - Move(param2, param6);                         | <- CodeGeneratorTester
// | ...                                             |
// |                                                 |
// | // "teardown" is the first parameter as well as |
// | // the callee.                                  |
// | TailCall param0(param0, param1, param2, ...);   |
// '-------------------------------------------------'
//   |
//   V
// .----------------teardown--------------.
// | Create a FixedArray with all         |
// | parameters and return it.            | <- CodeStubAssembler
// '--------------------------------------'

class TestEnvironment : public HandleAndZoneScope {
 public:
  // These constants may be tuned to experiment with different environments.

  static constexpr int kGeneralRegisterCount = 4;
  static constexpr int kDoubleRegisterCount = 6;

  static constexpr int kTaggedSlotCount = 64;
  static constexpr int kFloat32SlotCount = 64;
  static constexpr int kFloat64SlotCount = 64;
  static constexpr int kStackParameterCount =
      kTaggedSlotCount + kFloat32SlotCount + kFloat64SlotCount;

  // TODO(all): Test all types of constants (e.g. ExternalReference and
  // HeapObject).
  static constexpr int kSmiConstantCount = 4;
  static constexpr int kFloatConstantCount = 4;
  static constexpr int kDoubleConstantCount = 4;

  TestEnvironment()
      : blocks_(main_zone()),
        code_(main_isolate(), main_zone(), &blocks_),
        rng_(CcTest::random_number_generator()),
        // TODO(planglois): Support kSimd128.
        supported_reps_({MachineRepresentation::kTagged,
                         MachineRepresentation::kFloat32,
                         MachineRepresentation::kFloat64}) {
    // The "teardown" and "test" functions share the same descriptor with the
    // following signature:
    // ~~~
    // FixedArray f(CodeObject* teardown,
    //              // Tagged registers.
    //              Object*, Object*, ...,
    //              // FP registers.
    //              Float32, Float64, ...,
    //              // Mixed stack slots.
    //              Float64, Object*, Float32, ...);
    // ~~~
    LocationSignature::Builder test_signature(main_zone(), 1,
                                              1 + kGeneralRegisterCount +
                                                  kDoubleRegisterCount +
                                                  kStackParameterCount);

    // The first parameter will be the code object of the "teardown"
    // function. This way, the "test" function can tail-call to it.
    test_signature.AddParam(LinkageLocation::ForRegister(
        kReturnRegister0.code(), MachineType::AnyTagged()));

    // Initialise registers.

    int32_t general_mask =
        RegisterConfiguration::Default()->allocatable_general_codes_mask();
    // kReturnRegister0 is used to hold the "teardown" code object, do not
    // generate moves using it.
    std::unique_ptr<const RegisterConfiguration> registers(
        RegisterConfiguration::RestrictGeneralRegisters(
            general_mask & ~(1 << kReturnRegister0.code())));

    for (int i = 0; i < kGeneralRegisterCount; i++) {
      int code = registers->GetAllocatableGeneralCode(i);
      AddRegister(&test_signature, MachineRepresentation::kTagged, code);
    }
    // We assume that Double and Float registers alias, depending on
    // kSimpleFPAliasing. For this reason, we allocate a Float and a Double in
    // pairs.
    static_assert((kDoubleRegisterCount % 2) == 0,
                  "kDoubleRegisterCount should be a multiple of two.");
    for (int i = 0; i < kDoubleRegisterCount; i += 2) {
      // Make sure we do not allocate FP registers which alias. We double the
      // index for Float registers if the aliasing is not "Simple":
      //   Simple      -> s0, d1, s2, d3, s4, d5, ...
      //   Arm32-style -> s0, d1, s4, d3, s8, d5, ...
      // This isn't space-efficient at all but suits our need.
      static_assert(kDoubleRegisterCount < 16,
                    "Arm has a d16 register but no overlapping s32 register.");
      int float_code =
          registers->GetAllocatableFloatCode(kSimpleFPAliasing ? i : i * 2);
      int double_code = registers->GetAllocatableDoubleCode(i + 1);
      AddRegister(&test_signature, MachineRepresentation::kFloat32, float_code);
      AddRegister(&test_signature, MachineRepresentation::kFloat64,
                  double_code);
    }

    // Initialise stack slots.

    // Stack parameters start at -1.
    int slot_parameter_n = -1;

    // TODO(planglois): Support kSimd128 stack slots.
    std::map<MachineRepresentation, int> slots = {
        {MachineRepresentation::kTagged, kTaggedSlotCount},
        {MachineRepresentation::kFloat32, kFloat32SlotCount},
        {MachineRepresentation::kFloat64, kFloat64SlotCount}};

    // Allocate new slots until we run out of them.
    while (std::any_of(slots.cbegin(), slots.cend(),
                       [](const std::pair<MachineRepresentation, int>& entry) {
                         // True if there are slots left to allocate for this
                         // representation.
                         return entry.second > 0;
                       })) {
      // Pick a random MachineRepresentation from supported_reps_.
      MachineRepresentation rep = CreateRandomMachineRepresentation();
      auto entry = slots.find(rep);
      DCHECK(entry != slots.end());
      // We may have picked a representation for which all slots have already
      // been allocated.
      if (entry->second > 0) {
        // Keep a map of (MachineRepresentation . std::vector<int>) with
        // allocated slots to pick from for each representation.
        int slot = slot_parameter_n;
        slot_parameter_n -= (GetSlotSizeInBytes(rep) / kPointerSize);
        AddStackSlot(&test_signature, rep, slot);
        entry->second--;
      }
    }

    // Initialise random constants.

    // While constants do not know about Smis, we need to be able to
    // differentiate between a pointer to a HeapNumber and a integer. For this
    // reason, we make sure all integers are Smis, including constants.
    for (int i = 0; i < kSmiConstantCount; i++) {
      intptr_t smi_value = reinterpret_cast<intptr_t>(
          Smi::FromInt(rng_->NextInt(Smi::kMaxValue)));
      Constant constant = kPointerSize == 8
                              ? Constant(static_cast<int64_t>(smi_value))
                              : Constant(static_cast<int32_t>(smi_value));
      AddConstant(MachineRepresentation::kTagged, AllocateConstant(constant));
    }
    // Float and Double constants can be moved to both Tagged and FP registers
    // or slots. Register them as compatible with both FP and Tagged
    // destinations.
    for (int i = 0; i < kFloatConstantCount; i++) {
      int virtual_register =
          AllocateConstant(Constant(DoubleToFloat32(rng_->NextDouble())));
      AddConstant(MachineRepresentation::kTagged, virtual_register);
      AddConstant(MachineRepresentation::kFloat32, virtual_register);
    }
    for (int i = 0; i < kDoubleConstantCount; i++) {
      int virtual_register = AllocateConstant(Constant(rng_->NextDouble()));
      AddConstant(MachineRepresentation::kTagged, virtual_register);
      AddConstant(MachineRepresentation::kFloat64, virtual_register);
    }

    // The "teardown" function returns a FixedArray with the resulting state.
    test_signature.AddReturn(LinkageLocation::ForRegister(
        kReturnRegister0.code(), MachineType::AnyTagged()));

    test_descriptor_ = new (main_zone())
        CallDescriptor(CallDescriptor::kCallCodeObject,  // kind
                       MachineType::AnyTagged(),         // target MachineType
                       LinkageLocation::ForAnyRegister(
                           MachineType::AnyTagged()),  // target location
                       test_signature.Build(),         // location_sig
                       kStackParameterCount,           // stack_parameter_count
                       Operator::kNoProperties,        // properties
                       kNoCalleeSaved,                 // callee-saved registers
                       kNoCalleeSaved,                 // callee-saved fp
                       CallDescriptor::kNoFlags);      // flags
  }

  int AllocateConstant(Constant constant) {
    int virtual_register = code_.NextVirtualRegister();
    code_.AddConstant(virtual_register, constant);
    return virtual_register;
  }

  // Register a constant referenced by `virtual_register` as compatible with
  // `rep`.
  void AddConstant(MachineRepresentation rep, int virtual_register) {
    auto entry = allocated_constants_.find(rep);
    if (entry == allocated_constants_.end()) {
      allocated_constants_.emplace(
          rep, std::vector<ConstantOperand>{ConstantOperand(virtual_register)});
    } else {
      entry->second.emplace_back(virtual_register);
    }
  }

  // Register a new register or stack slot as compatible with `rep`. As opposed
  // to constants, registers and stack slots are written to on `setup` and read
  // from on `teardown`. Therefore they are part of the environment's layout,
  // and are parameters of the `test` function.

  void AddRegister(LocationSignature::Builder* test_signature,
                   MachineRepresentation rep, int code) {
    AllocatedOperand operand(AllocatedOperand::REGISTER, rep, code);
    layout_.push_back(operand);
    test_signature->AddParam(LinkageLocation::ForRegister(
        code, MachineType::TypeForRepresentation(rep)));
    auto entry = allocated_registers_.find(rep);
    if (entry == allocated_registers_.end()) {
      allocated_registers_.emplace(rep, std::vector<AllocatedOperand>{operand});
    } else {
      entry->second.push_back(operand);
    }
  }

  void AddStackSlot(LocationSignature::Builder* test_signature,
                    MachineRepresentation rep, int slot) {
    AllocatedOperand operand(AllocatedOperand::STACK_SLOT, rep, slot);
    layout_.push_back(operand);
    test_signature->AddParam(LinkageLocation::ForCallerFrameSlot(
        slot, MachineType::TypeForRepresentation(rep)));
    auto entry = allocated_slots_.find(rep);
    if (entry == allocated_slots_.end()) {
      allocated_slots_.emplace(rep, std::vector<AllocatedOperand>{operand});
    } else {
      entry->second.push_back(operand);
    }
  }

  // Generate a random initial state to test moves against. A "state" is a
  // packed FixedArray with Smis and HeapNumbers, according to the layout of the
  // environment.
  Handle<FixedArray> GenerateInitialState() {
    Handle<FixedArray> state = main_isolate()->factory()->NewFixedArray(
        static_cast<int>(layout_.size()));
    for (int i = 0; i < state->length(); i++) {
      switch (layout_[i].representation()) {
        case MachineRepresentation::kTagged:
          state->set(i, Smi::FromInt(rng_->NextInt(Smi::kMaxValue)));
          break;
        case MachineRepresentation::kFloat32:
          // HeapNumbers are Float64 values. However, we will convert it to a
          // Float32 and back inside `setup` and `teardown`. Make sure the value
          // we pick fits in a Float32.
          state->set(
              i, *main_isolate()->factory()->NewHeapNumber(
                     static_cast<double>(DoubleToFloat32(rng_->NextDouble()))));
          break;
        case MachineRepresentation::kFloat64:
          state->set(
              i, *main_isolate()->factory()->NewHeapNumber(rng_->NextDouble()));
          break;
        default:
          UNREACHABLE();
          break;
      }
    }
    return state;
  }

  // Run the code generated by a CodeGeneratorTester against `state_in` and
  // return a new resulting state.
  Handle<FixedArray> Run(Handle<Code> test, Handle<FixedArray> state_in) {
    Handle<FixedArray> state_out = main_isolate()->factory()->NewFixedArray(
        static_cast<int>(layout_.size()));
    {
      Handle<Code> setup =
          BuildSetupFunction(main_isolate(), test_descriptor_, layout_);
      // FunctionTester maintains its own HandleScope which means that its
      // return value will be freed along with it. Copy the result into
      // state_out.
      FunctionTester ft(setup, 2);
      Handle<FixedArray> result = ft.CallChecked<FixedArray>(test, state_in);
      CHECK_EQ(result->length(), state_in->length());
      result->CopyTo(0, *state_out, 0, result->length());
    }
    return state_out;
  }

  // For a given operand representing either a register or a stack slot, return
  // what position it should live in inside a FixedArray state.
  int OperandToStatePosition(const AllocatedOperand& operand) const {
    // Search `layout_` for `operand`.
    auto it = std::find_if(layout_.cbegin(), layout_.cend(),
                           [operand](const AllocatedOperand& this_operand) {
                             return this_operand.Equals(operand);
                           });
    DCHECK_NE(it, layout_.cend());
    return static_cast<int>(std::distance(layout_.cbegin(), it));
  }

  // Perform the given list of moves on `state_in` and return a newly allocated
  // state with the results.
  Handle<FixedArray> SimulateMoves(ParallelMove* moves,
                                   Handle<FixedArray> state_in) {
    Handle<FixedArray> state_out = main_isolate()->factory()->NewFixedArray(
        static_cast<int>(layout_.size()));
    // We do not want to modify `state_in` in place so perform the moves on a
    // copy.
    state_in->CopyTo(0, *state_out, 0, state_in->length());
    for (auto move : *moves) {
      int to_index =
          OperandToStatePosition(AllocatedOperand::cast(move->destination()));
      InstructionOperand from = move->source();
      if (from.IsConstant()) {
        Constant constant =
            code_.GetConstant(ConstantOperand::cast(from).virtual_register());
        Handle<Object> constant_value;
        switch (constant.type()) {
          case Constant::kInt32:
            constant_value =
                Handle<Smi>(reinterpret_cast<Smi*>(
                                static_cast<intptr_t>(constant.ToInt32())),
                            main_isolate());
            break;
          case Constant::kInt64:
            constant_value =
                Handle<Smi>(reinterpret_cast<Smi*>(
                                static_cast<intptr_t>(constant.ToInt64())),
                            main_isolate());
            break;
          case Constant::kFloat32:
            constant_value = main_isolate()->factory()->NewHeapNumber(
                static_cast<double>(constant.ToFloat32()));
            break;
          case Constant::kFloat64:
            constant_value = main_isolate()->factory()->NewHeapNumber(
                constant.ToFloat64().value());
            break;
          default:
            UNREACHABLE();
            break;
        }
        state_out->set(to_index, *constant_value);
      } else {
        int from_index = OperandToStatePosition(AllocatedOperand::cast(from));
        state_out->set(to_index, *state_out->GetValueChecked<Object>(
                                     main_isolate(), from_index));
      }
    }
    return state_out;
  }

  // Perform the given list of swaps on `state_in` and return a newly allocated
  // state with the results.
  Handle<FixedArray> SimulateSwaps(ParallelMove* swaps,
                                   Handle<FixedArray> state_in) {
    Handle<FixedArray> state_out = main_isolate()->factory()->NewFixedArray(
        static_cast<int>(layout_.size()));
    // We do not want to modify `state_in` in place so perform the swaps on a
    // copy.
    state_in->CopyTo(0, *state_out, 0, state_in->length());
    for (auto swap : *swaps) {
      int lhs_index =
          OperandToStatePosition(AllocatedOperand::cast(swap->destination()));
      int rhs_index =
          OperandToStatePosition(AllocatedOperand::cast(swap->source()));
      Handle<Object> lhs =
          state_out->GetValueChecked<Object>(main_isolate(), lhs_index);
      Handle<Object> rhs =
          state_out->GetValueChecked<Object>(main_isolate(), rhs_index);
      state_out->set(lhs_index, *rhs);
      state_out->set(rhs_index, *lhs);
    }
    return state_out;
  }

  // Compare the given state with a reference.
  void CheckState(Handle<FixedArray> actual, Handle<FixedArray> expected) {
    for (int i = 0; i < static_cast<int>(layout_.size()); i++) {
      Handle<Object> actual_value =
          actual->GetValueChecked<Object>(main_isolate(), i);
      Handle<Object> expected_value =
          expected->GetValueChecked<Object>(main_isolate(), i);
      if (!actual_value->StrictEquals(*expected_value)) {
        std::ostringstream expected_str;
        PrintStateValue(expected_str, expected_value, layout_[i]);
        std::ostringstream actual_str;
        PrintStateValue(actual_str, actual_value, layout_[i]);
        V8_Fatal(__FILE__, __LINE__, "Expected: '%s' but got '%s'",
                 expected_str.str().c_str(), actual_str.str().c_str());
      }
    }
  }

  enum OperandConstraint {
    kNone,
    // Restrict operands to non-constants. This is useful when generating a
    // destination.
    kCannotBeConstant
  };

  // Generate parallel moves at random. Note that they may not be compatible
  // between each other as this doesn't matter to the code generator.
  ParallelMove* GenerateRandomMoves(int size) {
    ParallelMove* parallel_move = new (main_zone()) ParallelMove(main_zone());

    for (int i = 0; i < size;) {
      MachineRepresentation rep = CreateRandomMachineRepresentation();
      MoveOperands mo(CreateRandomOperand(kNone, rep),
                      CreateRandomOperand(kCannotBeConstant, rep));
      // It isn't valid to call `AssembleMove` and `AssembleSwap` with redundant
      // moves.
      if (mo.IsRedundant()) continue;
      parallel_move->AddMove(mo.source(), mo.destination());
      // Iterate only when a move was created.
      i++;
    }

    return parallel_move;
  }

  ParallelMove* GenerateRandomSwaps(int size) {
    ParallelMove* parallel_move = new (main_zone()) ParallelMove(main_zone());

    for (int i = 0; i < size;) {
      MachineRepresentation rep = CreateRandomMachineRepresentation();
      InstructionOperand lhs = CreateRandomOperand(kCannotBeConstant, rep);
      InstructionOperand rhs = CreateRandomOperand(kCannotBeConstant, rep);
      MoveOperands mo(lhs, rhs);
      // It isn't valid to call `AssembleMove` and `AssembleSwap` with redundant
      // moves.
      if (mo.IsRedundant()) continue;
      // Canonicalize the swap: the register operand has to be the left hand
      // side.
      if (lhs.IsStackSlot() || lhs.IsFPStackSlot()) {
        std::swap(lhs, rhs);
      }
      parallel_move->AddMove(lhs, rhs);
      // Iterate only when a swap was created.
      i++;
    }

    return parallel_move;
  }

  MachineRepresentation CreateRandomMachineRepresentation() {
    int index = rng_->NextInt(static_cast<int>(supported_reps_.size()));
    return supported_reps_[index];
  }

  InstructionOperand CreateRandomOperand(OperandConstraint constraint,
                                         MachineRepresentation rep) {
    // Only generate a Constant if the operand is a source and we have a
    // constant with a compatible representation in stock.
    bool generate_constant =
        (constraint != kCannotBeConstant) &&
        (allocated_constants_.find(rep) != allocated_constants_.end());
    switch (rng_->NextInt(generate_constant ? 3 : 2)) {
      case 0:
        return CreateRandomStackSlotOperand(rep);
      case 1:
        return CreateRandomRegisterOperand(rep);
      case 2:
        return CreateRandomConstant(rep);
    }
    UNREACHABLE();
  }

  AllocatedOperand CreateRandomRegisterOperand(MachineRepresentation rep) {
    int index =
        rng_->NextInt(static_cast<int>(allocated_registers_[rep].size()));
    return allocated_registers_[rep][index];
  }

  AllocatedOperand CreateRandomStackSlotOperand(MachineRepresentation rep) {
    int index = rng_->NextInt(static_cast<int>(allocated_slots_[rep].size()));
    return allocated_slots_[rep][index];
  }

  ConstantOperand CreateRandomConstant(MachineRepresentation rep) {
    int index =
        rng_->NextInt(static_cast<int>(allocated_constants_[rep].size()));
    return allocated_constants_[rep][index];
  }

  v8::base::RandomNumberGenerator* rng() const { return rng_; }
  InstructionSequence* code() { return &code_; }
  CallDescriptor* test_descriptor() { return test_descriptor_; }

 private:
  ZoneVector<InstructionBlock*> blocks_;
  InstructionSequence code_;
  v8::base::RandomNumberGenerator* rng_;
  // The layout describes the type of each element in the environment, in order.
  std::vector<AllocatedOperand> layout_;
  CallDescriptor* test_descriptor_;
  // Allocated constants, registers and stack slots that we can generate moves
  // with. Each per compatible representation.
  std::vector<MachineRepresentation> supported_reps_;
  std::map<MachineRepresentation, std::vector<ConstantOperand>>
      allocated_constants_;
  std::map<MachineRepresentation, std::vector<AllocatedOperand>>
      allocated_registers_;
  std::map<MachineRepresentation, std::vector<AllocatedOperand>>
      allocated_slots_;
};

// static
constexpr int TestEnvironment::kGeneralRegisterCount;
constexpr int TestEnvironment::kDoubleRegisterCount;
constexpr int TestEnvironment::kTaggedSlotCount;
constexpr int TestEnvironment::kFloat32SlotCount;
constexpr int TestEnvironment::kFloat64SlotCount;
constexpr int TestEnvironment::kStackParameterCount;
constexpr int TestEnvironment::kSmiConstantCount;
constexpr int TestEnvironment::kFloatConstantCount;
constexpr int TestEnvironment::kDoubleConstantCount;

// Wrapper around the CodeGenerator. Code generated by this can only be called
// using the given `TestEnvironment`.
//
// TODO(planglois): We execute moves on stack parameters only which restricts
// ourselves to small positive offsets relative to the frame pointer. We should
// test large and negative offsets too. A way to do this would be to move some
// stack parameters to local spill slots and create artificial stack space
// between them.
class CodeGeneratorTester {
 public:
  explicit CodeGeneratorTester(TestEnvironment* environment)
      : zone_(environment->main_zone()),
        info_(ArrayVector("test"), environment->main_zone(), Code::STUB),
        linkage_(environment->test_descriptor()),
        frame_(environment->test_descriptor()->CalculateFixedFrameSize()),
        generator_(environment->main_zone(), &frame_, &linkage_,
                   environment->code(), &info_, environment->main_isolate(),
                   base::Optional<OsrHelper>(), kNoSourcePosition, nullptr,
                   nullptr) {
    // Force a frame to be created.
    generator_.frame_access_state()->MarkHasFrame(true);
    generator_.AssembleConstructFrame();
    // TODO(all): Generate a stack check here so that we fail gracefully if the
    // frame is too big.
  }

  enum PushTypeFlag {
    kRegisterPush = CodeGenerator::kRegisterPush,
    kStackSlotPush = CodeGenerator::kStackSlotPush,
    kScalarPush = CodeGenerator::kScalarPush
  };

  void CheckAssembleTailCallGaps(Instruction* instr,
                                 int first_unused_stack_slot,
                                 CodeGeneratorTester::PushTypeFlag push_type) {
    generator_.AssembleTailCallBeforeGap(instr, first_unused_stack_slot);
#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_S390) || \
    defined(V8_TARGET_ARCH_PPC)
    // Only folding register pushes is supported on ARM.
    bool supported = ((push_type & CodeGenerator::kRegisterPush) == push_type);
#elif defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_IA32) || \
    defined(V8_TARGET_ARCH_X87)
    bool supported = ((push_type & CodeGenerator::kScalarPush) == push_type);
#else
    bool supported = false;
#endif
    if (supported) {
      // Architectures supporting folding adjacent pushes should now have
      // resolved all moves.
      for (const auto& move :
           *instr->parallel_moves()[Instruction::FIRST_GAP_POSITION]) {
        CHECK(move->IsEliminated());
      }
    }
    generator_.AssembleGaps(instr);
    generator_.AssembleTailCallAfterGap(instr, first_unused_stack_slot);
  }

  void CheckAssembleMove(InstructionOperand* source,
                         InstructionOperand* destination) {
    int start = generator_.tasm()->pc_offset();
    generator_.AssembleMove(source, destination);
    CHECK(generator_.tasm()->pc_offset() > start);
  }

  void CheckAssembleSwap(InstructionOperand* source,
                         InstructionOperand* destination) {
    int start = generator_.tasm()->pc_offset();
    generator_.AssembleSwap(source, destination);
    CHECK(generator_.tasm()->pc_offset() > start);
  }

  Handle<Code> Finalize() {
    // The environment expects this code to tail-call to it's first parameter
    // placed in `kReturnRegister0`.
    generator_.AssembleArchInstruction(
        Instruction::New(zone_, kArchPrepareTailCall));

    InstructionOperand callee[] = {
        AllocatedOperand(LocationOperand::REGISTER,
                         MachineRepresentation::kTagged,
                         kReturnRegister0.code()),
        ImmediateOperand(
            ImmediateOperand::INLINE,
            V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK ? 1 : 0)};
    Instruction* tail_call = Instruction::New(zone_, kArchTailCallCodeObject, 0,
                                              nullptr, 2, callee, 0, nullptr);
    int first_unused_stack_slot;
    if (generator_.GetSlotAboveSPBeforeTailCall(tail_call,
                                                &first_unused_stack_slot)) {
      generator_.AssembleTailCallBeforeGap(tail_call, first_unused_stack_slot);
      generator_.AssembleTailCallAfterGap(tail_call, first_unused_stack_slot);
    }
    generator_.AssembleArchInstruction(tail_call);

    generator_.FinishCode();
    generator_.safepoints()->Emit(generator_.tasm(),
                                  frame_.GetTotalFrameSlotCount());
    return generator_.FinalizeCode();
  }

 private:
  Zone* zone_;
  CompilationInfo info_;
  Linkage linkage_;
  Frame frame_;
  CodeGenerator generator_;
};

// The following fuzz tests will assemble a lot of moves, wrap them in
// executable native code and run them. In order to check that moves were
// performed correctly, we need to setup an environment with an initial state
// and get it back after the list of moves were performed.
//
// We have two components to do this: TestEnvironment and CodeGeneratorTester.
//
// The TestEnvironment is in charge of bringing up an environment consisting of
// a set of registers, stack slots and constants, with initial values in
// them. The CodeGeneratorTester is a wrapper around the CodeGenerator and its
// only purpose is to generate code for a list of moves. The TestEnvironment is
// then able to run this code against the environment and return a resulting
// state.
//
// A "state" here is a packed FixedArray with tagged values which can either be
// Smis or HeapNumbers. When calling TestEnvironment::Run(...), registers and
// stack slots will be initialised according to this FixedArray. A new
// FixedArray is returned containing values that were moved by the generated
// code.
//
// And finally, we are able to compare the resulting FixedArray against a
// reference, computed with a simulation of AssembleMove and AssembleSwap. See
// SimulateMoves and SimulateSwaps.

TEST(FuzzAssembleMove) {
  TestEnvironment env;
  CodeGeneratorTester c(&env);

  Handle<FixedArray> state_in = env.GenerateInitialState();
  ParallelMove* moves = env.GenerateRandomMoves(1000);

  for (auto m : *moves) {
    c.CheckAssembleMove(&m->source(), &m->destination());
  }

  Handle<Code> test = c.Finalize();
  if (FLAG_print_code) {
    test->Print();
  }

  Handle<FixedArray> actual = env.Run(test, state_in);
  Handle<FixedArray> expected = env.SimulateMoves(moves, state_in);
  env.CheckState(actual, expected);
}

TEST(FuzzAssembleSwap) {
  TestEnvironment env;
  CodeGeneratorTester c(&env);

  Handle<FixedArray> state_in = env.GenerateInitialState();
  ParallelMove* swaps = env.GenerateRandomSwaps(1000);

  for (auto s : *swaps) {
    c.CheckAssembleSwap(&s->source(), &s->destination());
  }

  Handle<Code> test = c.Finalize();
  if (FLAG_print_code) {
    test->Print();
  }

  Handle<FixedArray> actual = env.Run(test, state_in);
  Handle<FixedArray> expected = env.SimulateSwaps(swaps, state_in);
  env.CheckState(actual, expected);
}

TEST(FuzzAssembleMoveAndSwap) {
  TestEnvironment env;
  CodeGeneratorTester c(&env);

  Handle<FixedArray> state_in = env.GenerateInitialState();
  Handle<FixedArray> expected =
      env.main_isolate()->factory()->NewFixedArray(state_in->length());
  state_in->CopyTo(0, *expected, 0, state_in->length());

  for (int i = 0; i < 1000; i++) {
    // Randomly alternate between swaps and moves.
    if (env.rng()->NextInt(2) == 0) {
      ParallelMove* move = env.GenerateRandomMoves(1);
      expected = env.SimulateMoves(move, expected);
      c.CheckAssembleMove(&move->at(0)->source(), &move->at(0)->destination());
    } else {
      ParallelMove* swap = env.GenerateRandomSwaps(1);
      expected = env.SimulateSwaps(swap, expected);
      c.CheckAssembleSwap(&swap->at(0)->source(), &swap->at(0)->destination());
    }
  }

  Handle<Code> test = c.Finalize();
  if (FLAG_print_code) {
    test->Print();
  }

  Handle<FixedArray> actual = env.Run(test, state_in);
  env.CheckState(actual, expected);
}

TEST(AssembleTailCallGap) {
  const RegisterConfiguration* conf = RegisterConfiguration::Default();
  TestEnvironment env;

  // This test assumes at least 4 registers are allocatable.
  CHECK_LE(4, conf->num_allocatable_general_registers());

  auto r0 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kTagged,
                             conf->GetAllocatableGeneralCode(0));
  auto r1 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kTagged,
                             conf->GetAllocatableGeneralCode(1));
  auto r2 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kTagged,
                             conf->GetAllocatableGeneralCode(2));
  auto r3 = AllocatedOperand(LocationOperand::REGISTER,
                             MachineRepresentation::kTagged,
                             conf->GetAllocatableGeneralCode(3));

  auto slot_minus_4 = AllocatedOperand(LocationOperand::STACK_SLOT,
                                       MachineRepresentation::kTagged, -4);
  auto slot_minus_3 = AllocatedOperand(LocationOperand::STACK_SLOT,
                                       MachineRepresentation::kTagged, -3);
  auto slot_minus_2 = AllocatedOperand(LocationOperand::STACK_SLOT,
                                       MachineRepresentation::kTagged, -2);
  auto slot_minus_1 = AllocatedOperand(LocationOperand::STACK_SLOT,
                                       MachineRepresentation::kTagged, -1);

  // Avoid slot 0 for architectures which use it store the return address.
  int first_slot = V8_TARGET_ARCH_STORES_RETURN_ADDRESS_ON_STACK ? 1 : 0;
  auto slot_0 = AllocatedOperand(LocationOperand::STACK_SLOT,
                                 MachineRepresentation::kTagged, first_slot);
  auto slot_1 =
      AllocatedOperand(LocationOperand::STACK_SLOT,
                       MachineRepresentation::kTagged, first_slot + 1);
  auto slot_2 =
      AllocatedOperand(LocationOperand::STACK_SLOT,
                       MachineRepresentation::kTagged, first_slot + 2);
  auto slot_3 =
      AllocatedOperand(LocationOperand::STACK_SLOT,
                       MachineRepresentation::kTagged, first_slot + 3);

  // These tests all generate series of moves that the code generator should
  // detect as adjacent pushes. Depending on the architecture, we make sure
  // these moves get eliminated.
  // Also, disassembling with `--print-code` is useful when debugging.

  {
    // Generate a series of register pushes only.
    CodeGeneratorTester c(&env);
    Instruction* instr = Instruction::New(env.main_zone(), kArchNop);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(r3, slot_0);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(r2, slot_1);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(r1, slot_2);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(r0, slot_3);

    c.CheckAssembleTailCallGaps(instr, first_slot + 4,
                                CodeGeneratorTester::kRegisterPush);
    Handle<Code> code = c.Finalize();
    if (FLAG_print_code) {
      code->Print();
    }
  }

  {
    // Generate a series of stack pushes only.
    CodeGeneratorTester c(&env);
    Instruction* instr = Instruction::New(env.main_zone(), kArchNop);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(slot_minus_4, slot_0);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(slot_minus_3, slot_1);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(slot_minus_2, slot_2);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(slot_minus_1, slot_3);

    c.CheckAssembleTailCallGaps(instr, first_slot + 4,
                                CodeGeneratorTester::kStackSlotPush);
    Handle<Code> code = c.Finalize();
    if (FLAG_print_code) {
      code->Print();
    }
  }

  {
    // Generate a mix of stack and register pushes.
    CodeGeneratorTester c(&env);
    Instruction* instr = Instruction::New(env.main_zone(), kArchNop);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(slot_minus_2, slot_0);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(r1, slot_1);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(slot_minus_1, slot_2);
    instr
        ->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION,
                                  env.main_zone())
        ->AddMove(r0, slot_3);

    c.CheckAssembleTailCallGaps(instr, first_slot + 4,
                                CodeGeneratorTester::kScalarPush);
    Handle<Code> code = c.Finalize();
    if (FLAG_print_code) {
      code->Print();
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
