// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen.h"
#include "src/compilation-info.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/instruction.h"
#include "src/compiler/linkage.h"
#include "src/isolate.h"

#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

class CodeGeneratorTester : public InitializedHandleScope {
 public:
  CodeGeneratorTester()
      : zone_(main_isolate()->allocator(), ZONE_NAME),
        info_(ArrayVector("test"), main_isolate(), &zone_,
              Code::ComputeFlags(Code::STUB)),
        descriptor_(Linkage::GetJSCallDescriptor(&zone_, false, 0,
                                                 CallDescriptor::kNoFlags)),
        linkage_(descriptor_),
        blocks_(&zone_),
        sequence_(main_isolate(), &zone_, &blocks_),
        frame_(descriptor_->CalculateFixedFrameSize()),
        generator_(&zone_, &frame_, &linkage_, &sequence_, &info_,
                   base::Optional<OsrHelper>(), kNoSourcePosition, nullptr) {
    info_.set_prologue_offset(generator_.tasm()->pc_offset());
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

  Handle<Code> Finalize() {
    generator_.FinishCode();
    generator_.safepoints()->Emit(generator_.tasm(),
                                  frame_.GetTotalFrameSlotCount());
    return generator_.FinalizeCode();
  }

  void Disassemble() {
    HandleScope scope(main_isolate());
    Handle<Code> code = Finalize();
    if (FLAG_print_code) {
      code->Print();
    }
  }

  Zone* zone() { return &zone_; }

 private:
  Zone zone_;
  CompilationInfo info_;
  CallDescriptor* descriptor_;
  Linkage linkage_;
  ZoneVector<InstructionBlock*> blocks_;
  InstructionSequence sequence_;
  Frame frame_;
  CodeGenerator generator_;
};

TEST(AssembleTailCallGap) {
  const RegisterConfiguration* conf = RegisterConfiguration::Default();

  // This test assumes at least 4 registers are allocatable.
  CHECK(conf->num_allocatable_general_registers() >= 4);

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
    CodeGeneratorTester c;
    Instruction* instr = Instruction::New(c.zone(), kArchNop);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(r3, slot_0);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(r2, slot_1);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(r1, slot_2);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(r0, slot_3);

    c.CheckAssembleTailCallGaps(instr, first_slot + 4,
                                CodeGeneratorTester::kRegisterPush);
    c.Disassemble();
  }

  {
    // Generate a series of stack pushes only.
    CodeGeneratorTester c;
    Instruction* instr = Instruction::New(c.zone(), kArchNop);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(slot_minus_4, slot_0);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(slot_minus_3, slot_1);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(slot_minus_2, slot_2);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(slot_minus_1, slot_3);

    c.CheckAssembleTailCallGaps(instr, first_slot + 4,
                                CodeGeneratorTester::kStackSlotPush);
    c.Disassemble();
  }

  {
    // Generate a mix of stack and register pushes.
    CodeGeneratorTester c;
    Instruction* instr = Instruction::New(c.zone(), kArchNop);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(slot_minus_2, slot_0);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(r1, slot_1);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(slot_minus_1, slot_2);
    instr->GetOrCreateParallelMove(Instruction::FIRST_GAP_POSITION, c.zone())
        ->AddMove(r0, slot_3);

    c.CheckAssembleTailCallGaps(instr, first_slot + 4,
                                CodeGeneratorTester::kScalarPush);
    c.Disassemble();
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
