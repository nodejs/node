// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/jump-threading.h"
#include "src/compiler/backend/code-generator-impl.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                                \
  do {                                            \
    if (FLAG_trace_turbo_jt) PrintF(__VA_ARGS__); \
  } while (false)

namespace {

struct JumpThreadingState {
  bool forwarded;
  ZoneVector<RpoNumber>& result;
  ZoneStack<RpoNumber>& stack;

  void Clear(size_t count) { result.assign(count, unvisited()); }
  void PushIfUnvisited(RpoNumber num) {
    if (result[num.ToInt()] == unvisited()) {
      stack.push(num);
      result[num.ToInt()] = onstack();
    }
  }
  void Forward(RpoNumber to) {
    RpoNumber from = stack.top();
    RpoNumber to_to = result[to.ToInt()];
    bool pop = true;
    if (to == from) {
      TRACE("  xx %d\n", from.ToInt());
      result[from.ToInt()] = from;
    } else if (to_to == unvisited()) {
      TRACE("  fw %d -> %d (recurse)\n", from.ToInt(), to.ToInt());
      stack.push(to);
      result[to.ToInt()] = onstack();
      pop = false;  // recurse.
    } else if (to_to == onstack()) {
      TRACE("  fw %d -> %d (cycle)\n", from.ToInt(), to.ToInt());
      result[from.ToInt()] = to;  // break the cycle.
      forwarded = true;
    } else {
      TRACE("  fw %d -> %d (forward)\n", from.ToInt(), to.ToInt());
      result[from.ToInt()] = to_to;  // forward the block.
      forwarded = true;
    }
    if (pop) stack.pop();
  }
  RpoNumber unvisited() { return RpoNumber::FromInt(-1); }
  RpoNumber onstack() { return RpoNumber::FromInt(-2); }
};

bool IsBlockWithBranchPoisoning(InstructionSequence* code,
                                InstructionBlock* block) {
  if (block->PredecessorCount() != 1) return false;
  RpoNumber pred_rpo = (block->predecessors())[0];
  const InstructionBlock* pred = code->InstructionBlockAt(pred_rpo);
  if (pred->code_start() == pred->code_end()) return false;
  Instruction* instr = code->InstructionAt(pred->code_end() - 1);
  FlagsMode mode = FlagsModeField::decode(instr->opcode());
  return mode == kFlags_branch_and_poison;
}

}  // namespace

bool JumpThreading::ComputeForwarding(Zone* local_zone,
                                      ZoneVector<RpoNumber>* result,
                                      InstructionSequence* code,
                                      bool frame_at_start) {
  ZoneStack<RpoNumber> stack(local_zone);
  JumpThreadingState state = {false, *result, stack};
  state.Clear(code->InstructionBlockCount());
  RpoNumber empty_deconstruct_frame_return_block = RpoNumber::Invalid();
  int32_t empty_deconstruct_frame_return_size;
  RpoNumber empty_no_deconstruct_frame_return_block = RpoNumber::Invalid();
  int32_t empty_no_deconstruct_frame_return_size;

  // Iterate over the blocks forward, pushing the blocks onto the stack.
  for (auto const block : code->instruction_blocks()) {
    RpoNumber current = block->rpo_number();
    state.PushIfUnvisited(current);

    // Process the stack, which implements DFS through empty blocks.
    while (!state.stack.empty()) {
      InstructionBlock* block = code->InstructionBlockAt(state.stack.top());
      // Process the instructions in a block up to a non-empty instruction.
      TRACE("jt [%d] B%d\n", static_cast<int>(stack.size()),
            block->rpo_number().ToInt());
      RpoNumber fw = block->rpo_number();
      if (!IsBlockWithBranchPoisoning(code, block)) {
        bool fallthru = true;
        for (int i = block->code_start(); i < block->code_end(); ++i) {
          Instruction* instr = code->InstructionAt(i);
          if (!instr->AreMovesRedundant()) {
            // can't skip instructions with non redundant moves.
            TRACE("  parallel move\n");
            fallthru = false;
          } else if (FlagsModeField::decode(instr->opcode()) != kFlags_none) {
            // can't skip instructions with flags continuations.
            TRACE("  flags\n");
            fallthru = false;
          } else if (instr->IsNop()) {
            // skip nops.
            TRACE("  nop\n");
            continue;
          } else if (instr->arch_opcode() == kArchJmp) {
            // try to forward the jump instruction.
            TRACE("  jmp\n");
            // if this block deconstructs the frame, we can't forward it.
            // TODO(mtrofin): we can still forward if we end up building
            // the frame at start. So we should move the decision of whether
            // to build a frame or not in the register allocator, and trickle it
            // here and to the code generator.
            if (frame_at_start || !(block->must_deconstruct_frame() ||
                                    block->must_construct_frame())) {
              fw = code->InputRpo(instr, 0);
            }
            fallthru = false;
          } else if (instr->IsRet()) {
            TRACE("  ret\n");
            if (fallthru) {
              CHECK_IMPLIES(block->must_construct_frame(),
                            block->must_deconstruct_frame());
              // Only handle returns with immediate/constant operands, since
              // they must always be the same for all returns in a function.
              // Dynamic return values might use different registers at
              // different return sites and therefore cannot be shared.
              if (instr->InputAt(0)->IsImmediate()) {
                int32_t return_size = ImmediateOperand::cast(instr->InputAt(0))
                                          ->inline_int32_value();
                // Instructions can be shared only for blocks that share
                // the same |must_deconstruct_frame| attribute.
                if (block->must_deconstruct_frame()) {
                  if (empty_deconstruct_frame_return_block ==
                      RpoNumber::Invalid()) {
                    empty_deconstruct_frame_return_block = block->rpo_number();
                    empty_deconstruct_frame_return_size = return_size;
                  } else if (empty_deconstruct_frame_return_size ==
                             return_size) {
                    fw = empty_deconstruct_frame_return_block;
                    block->clear_must_deconstruct_frame();
                  }
                } else {
                  if (empty_no_deconstruct_frame_return_block ==
                      RpoNumber::Invalid()) {
                    empty_no_deconstruct_frame_return_block =
                        block->rpo_number();
                    empty_no_deconstruct_frame_return_size = return_size;
                  } else if (empty_no_deconstruct_frame_return_size ==
                             return_size) {
                    fw = empty_no_deconstruct_frame_return_block;
                  }
                }
              }
            }
            fallthru = false;
          } else {
            // can't skip other instructions.
            TRACE("  other\n");
            fallthru = false;
          }
          break;
        }
        if (fallthru) {
          int next = 1 + block->rpo_number().ToInt();
          if (next < code->InstructionBlockCount())
            fw = RpoNumber::FromInt(next);
        }
      }
      state.Forward(fw);
    }
  }

#ifdef DEBUG
  for (RpoNumber num : *result) {
    DCHECK(num.IsValid());
  }
#endif

  if (FLAG_trace_turbo_jt) {
    for (int i = 0; i < static_cast<int>(result->size()); i++) {
      TRACE("B%d ", i);
      int to = (*result)[i].ToInt();
      if (i != to) {
        TRACE("-> B%d\n", to);
      } else {
        TRACE("\n");
      }
    }
  }

  return state.forwarded;
}

void JumpThreading::ApplyForwarding(Zone* local_zone,
                                    ZoneVector<RpoNumber> const& result,
                                    InstructionSequence* code) {
  if (!FLAG_turbo_jt) return;

  ZoneVector<bool> skip(static_cast<int>(result.size()), false, local_zone);

  // Skip empty blocks when the previous block doesn't fall through.
  bool prev_fallthru = true;
  for (auto const block : code->instruction_blocks()) {
    RpoNumber block_rpo = block->rpo_number();
    int block_num = block_rpo.ToInt();
    RpoNumber result_rpo = result[block_num];
    skip[block_num] = !prev_fallthru && result_rpo != block_rpo;

    if (result_rpo != block_rpo) {
      // We need the handler information to be propagated, so that branch
      // targets are annotated as necessary for control flow integrity
      // checks (when enabled).
      if (code->InstructionBlockAt(block_rpo)->IsHandler()) {
        code->InstructionBlockAt(result_rpo)->MarkHandler();
      }
    }

    bool fallthru = true;
    for (int i = block->code_start(); i < block->code_end(); ++i) {
      Instruction* instr = code->InstructionAt(i);
      FlagsMode mode = FlagsModeField::decode(instr->opcode());
      if (mode == kFlags_branch || mode == kFlags_branch_and_poison) {
        fallthru = false;  // branches don't fall through to the next block.
      } else if (instr->arch_opcode() == kArchJmp ||
                 instr->arch_opcode() == kArchRet) {
        if (skip[block_num]) {
          // Overwrite a redundant jump with a nop.
          TRACE("jt-fw nop @%d\n", i);
          instr->OverwriteWithNop();
          // If this block was marked as a handler, it can be unmarked now.
          code->InstructionBlockAt(block_rpo)->UnmarkHandler();
        }
        fallthru = false;  // jumps don't fall through to the next block.
      }
    }
    prev_fallthru = fallthru;
  }

  // Patch RPO immediates.
  InstructionSequence::RpoImmediates& rpo_immediates = code->rpo_immediates();
  for (size_t i = 0; i < rpo_immediates.size(); i++) {
    RpoNumber rpo = rpo_immediates[i];
    if (rpo.IsValid()) {
      RpoNumber fw = result[rpo.ToInt()];
      if (fw != rpo) rpo_immediates[i] = fw;
    }
  }

  // Renumber the blocks so that IsNextInAssemblyOrder() will return true,
  // even if there are skipped blocks in-between.
  int ao = 0;
  for (auto const block : code->ao_blocks()) {
    block->set_ao_number(RpoNumber::FromInt(ao));
    if (!skip[block->rpo_number().ToInt()]) ao++;
  }
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
