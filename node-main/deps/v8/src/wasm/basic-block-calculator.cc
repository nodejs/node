// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/basic-block-calculator.h"

#include "src/wasm/function-body-decoder.h"

namespace v8::internal::wasm {

void BasicBlockCalculator::StartNewBlock(Decoder* decoder) {
  current_basic_block_++;
  current_basic_block_start_pos_ = decoder->position();
}

void BasicBlockCalculator::EndCurrentBlock(Decoder* decoder) {
  DCHECK_GE(current_basic_block_, 0);
  DCHECK_GE(decoder->position(), current_basic_block_start_pos_);

  int final_instruction_length = OpcodeLength(decoder->pc(), decoder->end());
  basic_block_ranges_.emplace_back(
      current_basic_block_start_pos_,
      decoder->position() + final_instruction_length - 1);
}

void BasicBlockCalculator::ComputeBasicBlocks() {
  DCHECK(basic_block_ranges_.empty());

  // Basic block rules.
  // ------------------
  // 1. The first basic block starts at function offset 0.
  // 2. The 'else' instructions and the 'end' instructions end the current
  // block. They are the last instruction in the basic block and the next
  // instruction will start a new block. Likewise, all branch, call, tail-call,
  // 'unreachable', 'return', 'if' and throw instructions end the current block.
  // 3. The 'loop' instructions end the current block and start a new block.
  // 4. The first instruction after the end of a block starts a new block.
  // There can be unreachable instructions in between, which are not part of
  // any block.
  // 5. The last basic block ends at the offset of the final function 'end'
  // instruction.

  BodyLocalDecls locals;
  BytecodeIterator iterator(start_, end_, &locals, zone_);
  DCHECK_LT(0, locals.encoded_size);

  // Start function.
  current_basic_block_ = 0;
  current_basic_block_start_pos_ = 0;
  bool at_block_end = false;

  for (; iterator.has_next(); iterator.next()) {
    WasmOpcode opcode = iterator.current();

    if (at_block_end) {
      StartNewBlock(&iterator);
      at_block_end = false;
    }

    switch (opcode) {
      case kExprLoop:
        DCHECK_GE(current_basic_block_, 0);
        if (iterator.position() > current_basic_block_start_pos_) {
          basic_block_ranges_.emplace_back(current_basic_block_start_pos_,
                                           iterator.position() - 1);
          StartNewBlock(&iterator);
        }
        break;

      case kExprEnd:
      case kExprIf:
      case kExprElse:
      case kExprUnreachable:
      case kExprBr:
      case kExprBrTable:
      case kExprReturn:
      case kExprDelegate:
      case kExprRethrow:
      case kExprThrow:
      case kExprThrowRef:
      case kExprCatch:
      case kExprCatchAll:
      case kExprBrIf:
      case kExprBrOnNull:
      case kExprBrOnNonNull:
      case kExprBrOnCast:
      case kExprBrOnCastFail:
      case kExprBrOnCastDesc:
      case kExprBrOnCastDescFail:
      case kExprReturnCall:
      case kExprReturnCallIndirect:
      case kExprReturnCallRef:
      case kExprCallFunction:
      case kExprCallIndirect:
      case kExprCallRef:
        EndCurrentBlock(&iterator);
        at_block_end = true;
        break;

      default:
        break;
    }
  }

  if (!at_block_end) {
    basic_block_ranges_.emplace_back(current_basic_block_start_pos_,
                                     iterator.position() - 1);
  }
}

}  // namespace v8::internal::wasm
