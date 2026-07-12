// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-bytecode-analysis.h"

#include <utility>

#include "src/objects/fixed-array-inl.h"
#include "src/regexp/regexp-bytecode-iterator-inl.h"
#include "src/regexp/regexp-bytecodes-inl.h"
#include "src/regexp/regexp-bytecodes.h"

namespace v8 {
namespace internal {

RegExpBytecodeAnalysis::RegExpBytecodeAnalysis(
    Isolate* isolate, Zone* zone, DirectHandle<TrustedByteArray> bytecode)
    : zone_(zone),
      bytecode_(*bytecode, isolate),
      length_(bytecode->length()),
      offset_to_prev_bytecode_(bytecode->length() + kSlotAtLength, 0, zone),
      backtrack_targets_(zone),
      block_starts_(zone),
      offset_to_block_id_(length_, -1, zone),
      offset_to_ebb_id_(length_, -1, zone),
      predecessors_(zone),
      loops_(zone),
      is_loop_header_(0, zone),
      is_back_edge_(length_, zone),
      uses_current_char_(0, zone),
      loads_current_char_(0, zone) {}

void RegExpBytecodeAnalysis::Analyze() {
  // TODO(jgruber): Analysis passes need to be optimized before being used in
  // production. We are quite wasteful currently.
  FindBasicBlocks();
  AnalyzeControlFlow();
  AnalyzeDataFlow();
}

void RegExpBytecodeAnalysis::PrintBlock(int block_id) {
  const char* kGrey = "\e[0;32m";
  const char* kReset = "\033[0m";
  const char* prefix = "-- ";

  const int block_start = BlockStart(block_id);
  PrintF("%s%sb%02d, ebb%02d, [%x,%x), attrs {", kGrey, prefix, block_id,
         GetEbbId(block_start), block_start, BlockEnd(block_id));
  if (IsLoopHeader(block_id)) PrintF(" header");
  if (UsesCurrentChar(block_id)) PrintF(" use_cc");
  if (LoadsCurrentChar(block_id)) PrintF(" load_cc");

  {
    PrintF("}, pred {");
    bool printed_first = false;
    for (int pred : predecessors_[block_id]) {
      PrintF("%sb%02d", printed_first ? "," : "", pred);
      printed_first = true;
    }
  }
  {
    PrintF("}, succ {");
    bool printed_first = false;
    ForEachSuccessor(
        block_id,
        [&](int successor, int jump_offset, bool is_backtrack) {
          PrintF("%sb%02d", printed_first ? "," : "", successor);
          printed_first = true;
          if (IsBackEdge(jump_offset)) PrintF(" backedge");
        },
        true);
  }
  {
    // Loop membership.
    PrintF("}, loops {");
    bool printed_first = false;
    for (const auto& loop : loops_) {
      if (!loop.members.Contains(block_id)) continue;
      PrintF("%sb%02d", printed_first ? "," : "", loop.header_block_id);
      printed_first = true;
      for (const auto& exit : loop.exits) {
        if (exit.first == block_id) {
          PrintF(" exit");
          break;
        }
      }
    }
    PrintF("}");
  }
  PrintF("%s\n", kReset);

  // Loop headers have one addtl line with infos:
  if (IsLoopHeader(block_id)) {
    for (const auto& loop : loops_) {
      if (loop.header_block_id == block_id) {
        PrintF("%s%s  loop members {", kGrey, prefix);
        for (int member : loop.members) {
          PrintF(" b%02d", member);
        }
        PrintF("} exits {");
        for (const auto& exit : loop.exits) {
          PrintF(" b%02d->b%02d", exit.first, exit.second);
        }
        PrintF("}%s\n", kReset);
      }
    }
  }
}

int RegExpBytecodeAnalysis::GetBlockId(int bytecode_offset) const {
  DCHECK_GE(bytecode_offset, 0);
  DCHECK_LT(bytecode_offset, length_);
  int id = offset_to_block_id_[bytecode_offset];
  DCHECK_GE(id, 0);
  return id;
}

int RegExpBytecodeAnalysis::GetEbbId(int bytecode_offset) const {
  DCHECK_GE(bytecode_offset, 0);
  DCHECK_LT(bytecode_offset, length_);
  // Can be uninitialized for dead code so negative ids are valid.
  return offset_to_ebb_id_[bytecode_offset];
}

int RegExpBytecodeAnalysis::BlockStart(int block_id) const {
  DCHECK_GE(block_id, 0);
  DCHECK_LT(block_id, block_count());
  return block_starts_[block_id];
}

int RegExpBytecodeAnalysis::BlockEnd(int block_id) const {
  DCHECK_GE(block_id, 0);
  DCHECK_LT(block_id, block_count());
  return block_starts_[block_id + 1];
}

bool RegExpBytecodeAnalysis::IsLoopHeader(int block_id) const {
  return is_loop_header_.Contains(block_id);
}

bool RegExpBytecodeAnalysis::IsBackEdge(int bytecode_offset) const {
  return is_back_edge_.Contains(bytecode_offset);
}

void RegExpBytecodeAnalysis::FindBasicBlocks() {
  // Potential optimizations
  // * Simple xmm reg allocation, with constant hoisting.
  //   * Only do this if the code actually uses the xmm regs.
  //   * Careful around caller-saved xmms (for outgoing calls) and callee-saved
  //     for returns.
  // * Push callee-saved xmm regs in the prologue,epilogue, and when calling to
  //   C.
  // * Simdify much smaller chunks, e.g. AndCheck4Chars. But: reloading the
  //   current string into vector regs is expensive.
  //   The ..Masked optimization is hard to beat because it implicitly assumes
  //   that it's okay to be a bit fuzzy wrt fast simd candidate location - the
  //   scalar suffix will filter out bad matches.
  // * Load elimination. We could always load 8/4 byte chunks and eliminate
  //   following LoadCurrentCharacter with cp_offset inside this range. Loop
  //   unrolling could create more opportunities. Comparison ops would have to
  //   be updated to mask appropriately.
  // * Fold the repeated Load1-Check1 sequence.
  //    * EBB analysis.
  //    * Single use for each of the char loads.
  //    * cp_offset is (almost) consecutive.
  //    * The uses are all CheckFoo where Foo is the same in all uses (modulo
  //      Not), and the jump target is the same.
  // * Dead code elimination.
  // * Backtrack elimination (if only one backtrack target exists)
  //    * Remove all pushbacktracks.
  //    * Replace kBacktrack by Goto.
  // * Likewise, if the Backtrack bytecode is dead.
  // * Regexp stack check elimination (Extended Basic Blocks).
  // * Align loop headers.

  // Pass 1: Identify leaders.
  // A leader is:
  // 1. The first instruction (offset 0).
  // 2. The target of any jump.
  // 3. The instruction following a terminator.

  BitVector leaders(length_ + kSlotAtLength, zone_);
  leaders.Add(0);

  uint32_t prev_offset = 0;
  for (RegExpBytecodeIterator it(bytecode_); !it.done(); it.advance()) {
    const RegExpBytecode bytecode = it.current_bytecode();
    const RegExpBytecodeFlags flags = RegExpBytecodes::Flags(bytecode);
    const uint32_t current_offset = it.current_offset();
    const uint32_t next_offset =
        current_offset + RegExpBytecodes::Size(bytecode);

    const bool is_fallthrough = (flags & ReBcFlag::kNoFallthrough) == 0;
    bool treat_as_fallthrough = is_fallthrough;

    // Gather offsets for backward walks.
    DCHECK(current_offset > prev_offset || current_offset == 0);
    DCHECK(is_uint8(current_offset - prev_offset));
    offset_to_prev_bytecode_[current_offset] = current_offset - prev_offset;
    prev_offset = current_offset;

    // Gather jump targets.
    bool has_jumptarget_operand = false;
    bool has_nontrivial_jumptarget = false;
    RegExpBytecodes::DispatchOnBytecode(bytecode, [&]<RegExpBytecode bc>() {
      using Operands = RegExpBytecodeOperands<bc>;
      Operands::ForEachOperand([&]<auto op>() {
        if constexpr (Operands::Type(op) ==
                      RegExpBytecodeOperandType::kJumpTarget) {
          uint32_t target =
              Operands::template Get<op>(it.current_address(), no_gc_);
          DCHECK_LT(target, length_);
          if constexpr (bc == RegExpBytecode::kPushBacktrack) {
            // The target is pushed to the stack, not branched to.
            backtrack_targets_.push_back(target);
            leaders.Add(target);
            return;
          }
          has_jumptarget_operand = true;
          if (target == next_offset) {
            treat_as_fallthrough = true;
            return;
          }
          has_nontrivial_jumptarget = true;
          leaders.Add(target);
        }
      });
    });

    if (treat_as_fallthrough && has_nontrivial_jumptarget) {
      // Bytecodes that have multiple targets terminate the block.
      leaders.Add(next_offset);
    } else if (!treat_as_fallthrough) {
      // Bytecodes that don't fallthrough terminate the block.
      leaders.Add(next_offset);
    } else {
      // Execution always resumes at the next bytecode.
      DCHECK(treat_as_fallthrough && !has_nontrivial_jumptarget);
    }
  }

  // And for backwards walks from the end:
  DCHECK_GT(static_cast<uint32_t>(length_), prev_offset);
  DCHECK(is_uint8(static_cast<uint32_t>(length_) - prev_offset));
  offset_to_prev_bytecode_[length_] =
      static_cast<uint32_t>(length_) - prev_offset;

  // Pass 2: Create blocks.
  for (int leader : leaders) {
    block_starts_.push_back(leader);
  }

  // Pass 3: Map offsets to block IDs.
  int current_block_id = 0;
  int next_block_start = block_starts_[current_block_id + 1];
  for (int pc = 0; pc < length_; ++pc) {
    if (pc == next_block_start) {
      current_block_id++;
      next_block_start = block_starts_[current_block_id + 1];
      DCHECK_GT(next_block_start, pc);
    }
    offset_to_block_id_[pc] = current_block_id;
  }
}

template <typename Callback>
void RegExpBytecodeAnalysis::ForEachSuccessor(int block_id, Callback callback,
                                              bool include_backtrack) {
  int end = BlockEnd(block_id);
  DCHECK_GT(offset_to_prev_bytecode_[end], 0);
  int current_offset = end - offset_to_prev_bytecode_[end];

  // To find successors, we only need to look at the last instruction of the
  // block.

  RegExpBytecodeIterator iterator(bytecode_, current_offset);
  RegExpBytecode bytecode = iterator.current_bytecode();

  // Backtrack may jump to any backtrack target.
  if (bytecode == RegExpBytecode::kBacktrack) {
    if (include_backtrack) {
      for (uint32_t target : backtrack_targets_) {
        callback(GetBlockId(target), current_offset, true);
      }
    }
    return;
  }

  const RegExpBytecodeFlags flags = RegExpBytecodes::Flags(bytecode);
  const bool may_branch =
      (flags & ReBcFlag::kNoBranchDespiteJumpTargetOperand) == 0;
  const bool is_fallthrough = (flags & ReBcFlag::kNoFallthrough) == 0;

  if (may_branch) {
    RegExpBytecodes::DispatchOnBytecode(bytecode, [&]<RegExpBytecode bc>() {
      using Operands = RegExpBytecodeOperands<bc>;
      Operands::ForEachOperand([&]<auto op>() {
        if constexpr (Operands::Type(op) ==
                      RegExpBytecodeOperandType::kJumpTarget) {
          uint32_t target =
              Operands::template Get<op>(iterator.current_address(), no_gc_);
          CHECK_LT(target, length_);
          // TODO(jgruber): Maybe we also want to store the offset of the
          // current operand.
          callback(GetBlockId(target), current_offset, false);
        }
      });
    });
  }

  if (is_fallthrough && end < length_) {
    callback(GetBlockId(end), current_offset, false);
  }
}

void RegExpBytecodeAnalysis::AnalyzeControlFlow() {
  const int num_blocks = block_count();

  predecessors_.assign(num_blocks, ZoneSet<int>(zone_));
  is_loop_header_.Resize(num_blocks, zone_);

  BitVector visited(num_blocks, zone_);
  BitVector recursion_stack(num_blocks, zone_);
  ZoneVector<std::pair<int, int>> back_edges(zone_);

  struct Frame {
    int block_id;
    int ebb_id;
    bool successors_visited;
  };

  ZoneStack<Frame> dfs_stack(zone_);
  dfs_stack.push({0, 0, false});

  while (!dfs_stack.empty()) {
    Frame& frame = dfs_stack.top();
    const int u = frame.block_id;

    if (frame.successors_visited) {
      // Post-order.
      recursion_stack.Remove(u);
      dfs_stack.pop();
      continue;
    }

    if (visited.Contains(u)) {
      dfs_stack.pop();
      continue;
    }

    // Pre-order.
    visited.Add(u);
    recursion_stack.Add(u);
    frame.successors_visited = true;

    ForEachSuccessor(
        u,
        [&](int v, int jump_offset, bool is_backtrack) {
          predecessors_[v].emplace(u);
          if (!is_backtrack && recursion_stack.Contains(v)) {
            // Back-edge detected.
            is_loop_header_.Add(v);
            is_back_edge_.Add(jump_offset);
            back_edges.push_back({u, v});
          } else if (!visited.Contains(v)) {
            dfs_stack.push({v, 0, false});
          }
        },
        true);
  }

  // Now that we have information about predecessors, find extended basic
  // blocks.
  visited.Clear();
  int next_ebb_id = 0;
  offset_to_ebb_id_[0] = next_ebb_id;
  dfs_stack.push({0, next_ebb_id++, false});

  while (!dfs_stack.empty()) {
    Frame frame = dfs_stack.top();
    dfs_stack.pop();

    const int u = frame.block_id;
    if (visited.Contains(u)) {
      continue;
    }

    visited.Add(u);

    ForEachSuccessor(
        u,
        [&](int v, int jump_offset, bool is_backtrack) {
          if (visited.Contains(v)) return;
          int ebb_id = GetEbbId(BlockStart(v));
          if (ebb_id == -1) {
            ebb_id = frame.ebb_id;
            if (predecessors_[v].size() > 1 || is_backtrack) {
              ebb_id = next_ebb_id++;
            }

            // Only record at block start.
            offset_to_ebb_id_[BlockStart(v)] = ebb_id;
          }
          dfs_stack.push({v, ebb_id, false});
        },
        true);
  }

  ComputeLoops(back_edges);
}

void RegExpBytecodeAnalysis::ComputeLoops(
    const ZoneVector<std::pair<int, int>>& back_edges) {
  int num_blocks = block_count();

  for (const auto& edge : back_edges) {
    int header = edge.second;
    int latch = edge.first;

    // Find or create loop info for this header.
    // TODO(jgruber): not linear search. BlockInfo could point at LoopInfo.
    LoopInfo* loop = nullptr;
    for (auto& l : loops_) {
      if (l.header_block_id == header) {
        loop = &l;
        break;
      }
    }
    if (loop == nullptr) {
      loops_.emplace_back(header, num_blocks, zone_);
      loop = &loops_.back();
      loop->members.Add(header);
    }

    // Add blocks to loop body by walking backwards from latch to header.
    ZoneVector<int> worklist(zone_);
    worklist.push_back(latch);
    loop->members.Add(latch);

    while (!worklist.empty()) {
      int block = worklist.back();
      worklist.pop_back();

      if (block == header) continue;

      for (int pred : predecessors_[block]) {
        if (!loop->members.Contains(pred)) {
          loop->members.Add(pred);
          worklist.push_back(pred);
        }
      }
    }
  }

  // Compute loop exits now that loop infos are complete.
  for (auto& loop : loops_) {
    for (int block : loop.members) {
      ForEachSuccessor(
          block,
          [&](int successor, int jump_offset, bool is_backtrack) {
            if (!loop.members.Contains(successor)) {
              loop.exits.push_back({block, successor});
            }
          },
          true);
    }
  }
}

bool RegExpBytecodeAnalysis::UsesCurrentChar(int block_id) const {
  return uses_current_char_.Contains(block_id);
}

bool RegExpBytecodeAnalysis::LoadsCurrentChar(int block_id) const {
  return loads_current_char_.Contains(block_id);
}

void RegExpBytecodeAnalysis::AnalyzeDataFlow() {
  int num_blocks = block_count();
  uses_current_char_.Resize(num_blocks, zone_);
  loads_current_char_.Resize(num_blocks, zone_);

  // Annotate blocks for current_character usage. A block "uses"
  // current_character if any use exists before a load in the same block; any
  // uses after a local load are not counted.

  for (int block_id = 0; block_id < num_blocks; ++block_id) {
    int start = BlockStart(block_id);
    int end = BlockEnd(block_id);

    RegExpBytecodeIterator iterator(bytecode_, start);
    bool locally_loaded = false;

    while (iterator.current_offset() < end) {
      RegExpBytecode bytecode = iterator.current_bytecode();

      const RegExpBytecodeFlags flags = RegExpBytecodes::Flags(bytecode);
      const bool loads = (flags & ReBcFlag::kLoadsCC) != 0;
      const bool uses = (flags & ReBcFlag::kUsesCC) != 0;

      if (uses && !locally_loaded) {
        uses_current_char_.Add(block_id);
      }
      if (loads) {
        loads_current_char_.Add(block_id);
        locally_loaded = true;
      }

      iterator.advance();
    }
  }
}

}  // namespace internal
}  // namespace v8
