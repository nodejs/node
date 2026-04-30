// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-bytecode-analysis.h"

#include <algorithm>
#include <utility>

#include "src/objects/fixed-array-inl.h"
#include "src/regexp/regexp-bytecode-iterator-inl.h"
#include "src/regexp/regexp-bytecodes-inl.h"
#include "src/regexp/regexp-bytecodes.h"

namespace v8 {
namespace internal {
namespace regexp {

BytecodeAnalysis::BytecodeAnalysis(Isolate* isolate, Zone* zone,
                                   DirectHandle<TrustedByteArray> bytecode)
    : zone_(zone),
      bytecode_(*bytecode, isolate),
      length_(bytecode->ulength().value()),
      backtrack_targets_(zone),
      block_starts_(zone),
      block_to_ebb_id_(zone),
      successors_(zone),
      predecessors_(zone),
      loops_(zone),
      is_loop_header_(0, zone),
      uses_current_char_(0, zone),
      loads_current_char_(0, zone),
      terminates_with_backtrack_(0, zone),
      back_edges_(zone) {}

void BytecodeAnalysis::Analyze() {
  BuildBlocks();
  AnalyzeControlFlow();
}

void BytecodeAnalysis::PrintBlock(uint32_t block_id) {
  if (block_id == backtrack_dispatch_id()) return;

  const char* kGrey = "\e[0;32m";
  const char* kReset = "\033[0m";
  const char* prefix = "-- ";

  const uint32_t block_start = BlockStart(block_id);
  PrintF("%s%sb%02d, ebb%02d, [%x,%x), attrs {", kGrey, prefix, block_id,
         GetEbbId(block_id), block_start, BlockEnd(block_id));
  if (IsLoopHeader(block_id)) PrintF(" header");
  if (UsesCurrentChar(block_id)) PrintF(" use_cc");
  if (LoadsCurrentChar(block_id)) PrintF(" load_cc");

  {
    PrintF("}, pred {");
    bool printed_first = false;
    for (uint32_t pred : predecessors_[block_id]) {
      if (pred == backtrack_dispatch_id()) {
        PrintF("%sdispatch", printed_first ? "," : "");
      } else {
        PrintF("%sb%02d", printed_first ? "," : "", pred);
      }
      printed_first = true;
    }
  }
  {
    PrintF("}, succ {");
    bool printed_first = false;
    for (uint32_t successor : successors_[block_id]) {
      if (successor == backtrack_dispatch_id()) {
        PrintF("%sdispatch", printed_first ? "," : "");
      } else {
        PrintF("%sb%02d", printed_first ? "," : "", successor);
      }
      printed_first = true;
      if (std::find(back_edges_.begin(), back_edges_.end(),
                    std::pair<uint32_t, uint32_t>{block_id, successor}) !=
          back_edges_.end()) {
        PrintF(" backedge");
      }
    }
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
          if (member == static_cast<int>(backtrack_dispatch_id())) {
            PrintF(" dispatch");
          } else {
            PrintF(" b%02d", member);
          }
        }
        PrintF("} exits {");
        for (const auto& exit : loop.exits) {
          if (exit.first == backtrack_dispatch_id()) {
            PrintF(" dispatch->b%02d", exit.second);
          } else if (exit.second == backtrack_dispatch_id()) {
            PrintF(" b%02d->dispatch", exit.first);
          } else {
            PrintF(" b%02d->b%02d", exit.first, exit.second);
          }
        }
        PrintF("}%s\n", kReset);
      }
    }
  }
}

uint32_t BytecodeAnalysis::GetBlockId(uint32_t bytecode_offset) const {
  DCHECK_LT(bytecode_offset, length_);
  auto it = std::upper_bound(block_starts_.begin(), block_starts_.end(),
                             bytecode_offset);
  DCHECK(it != block_starts_.begin());
  return static_cast<uint32_t>(std::distance(block_starts_.begin(), it) - 1);
}

int BytecodeAnalysis::GetEbbId(uint32_t block_id) const {
  DCHECK_LT(block_id, total_block_count());
  return block_to_ebb_id_[block_id];
}

uint32_t BytecodeAnalysis::BlockStart(uint32_t block_id) const {
  DCHECK_LT(block_id, block_count());
  return block_starts_[block_id];
}

uint32_t BytecodeAnalysis::BlockEnd(uint32_t block_id) const {
  DCHECK_LT(block_id, block_count());
  return block_starts_[block_id + 1];
}

bool BytecodeAnalysis::IsLoopHeader(uint32_t block_id) const {
  return is_loop_header_.Contains(block_id);
}

bool BytecodeAnalysis::UsesCurrentChar(uint32_t block_id) const {
  return uses_current_char_.Contains(block_id);
}

bool BytecodeAnalysis::LoadsCurrentChar(uint32_t block_id) const {
  return loads_current_char_.Contains(block_id);
}

void BytecodeAnalysis::BuildBlocks() {
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
  //
  // This phase collects all bytecode offsets that start a new basic block.

  ZoneVector<uint32_t> leaders(zone_);
  leaders.push_back(0);

  for (BytecodeIterator it(bytecode_); !it.done(); it.advance()) {
    const Bytecode bytecode = it.current_bytecode();
    const BytecodeFlags flags = Bytecodes::Flags(bytecode);
    const uint32_t current_offset = it.current_offset();
    const uint32_t next_offset = current_offset + Bytecodes::Size(bytecode);

    const bool is_fallthrough = (flags & ReBcFlag::kNoFallthrough) == 0;
    bool treat_as_fallthrough = is_fallthrough;

    // Collect jump targets. Some bytecodes (like kPushBacktrack) don't
    // immediately branch, but their target will eventually be a leader.
    bool has_nontrivial_jumptarget = false;
    Bytecodes::DispatchOnBytecode(bytecode, [&]<Bytecode bc>() {
      using Operands = BytecodeOperands<bc>;
      Operands::ForEachOperand([&]<auto op>() {
        if constexpr (Operands::Type(op) == BytecodeOperandType::kJumpTarget) {
          uint32_t target =
              Operands::template Get<op>(it.current_address(), no_gc_);
          DCHECK_LT(target, length_);
          if constexpr (bc == Bytecode::kPushBacktrack) {
            backtrack_targets_.push_back(target);
            leaders.push_back(target);
            return;
          }
          if (target == next_offset) {
            treat_as_fallthrough = true;
            return;
          }
          has_nontrivial_jumptarget = true;
          leaders.push_back(target);
        }
      });
    });

    // If we have a jump target AND can fall through, the fall-through must
    // start a new block. Similarly, if we cannot fall through, the next
    // instruction starts a new block.
    if (treat_as_fallthrough && has_nontrivial_jumptarget) {
      leaders.push_back(next_offset);
    } else if (!treat_as_fallthrough) {
      leaders.push_back(next_offset);
    }
  }

  // Finalize block boundaries. We sort and unique the leaders to get a clean
  // list of block starts.
  leaders.push_back(length_);
  std::sort(leaders.begin(), leaders.end());
  leaders.erase(std::unique(leaders.begin(), leaders.end()), leaders.end());
  block_starts_ = std::move(leaders);

  const uint32_t num_blocks = block_count();
  const uint32_t total_blocks = total_block_count();

  // Initialize data structures for the graph.
  successors_.assign(total_blocks, ZoneVector<uint32_t>(zone_));
  predecessors_.assign(total_blocks, ZoneVector<uint32_t>(zone_));
  uses_current_char_.Resize(total_blocks, zone_);
  loads_current_char_.Resize(total_blocks, zone_);
  terminates_with_backtrack_.Resize(total_blocks, zone_);

  // Backtrack canonicalization: instead of edges from every 'Backtrack' site
  // to every 'PushBacktrack' target, we create a single backtrack dispatch
  // node. All 'Backtrack' sites point to it, and it points to all unique
  // target blocks.
  ZoneVector<uint32_t> backtrack_target_blocks(zone_);
  for (uint32_t target : backtrack_targets_) {
    backtrack_target_blocks.push_back(GetBlockId(target));
  }
  std::sort(backtrack_target_blocks.begin(), backtrack_target_blocks.end());
  backtrack_target_blocks.erase(std::unique(backtrack_target_blocks.begin(),
                                            backtrack_target_blocks.end()),
                                backtrack_target_blocks.end());

  successors_[backtrack_dispatch_id()] = backtrack_target_blocks;

  // Pass 2: Construct edges and annotate data-flow properties.
  for (uint32_t block_id = 0; block_id < num_blocks; ++block_id) {
    uint32_t start = BlockStart(block_id);
    uint32_t end = BlockEnd(block_id);

    BytecodeIterator it(bytecode_, start);
    bool locally_loaded = false;

    // A block "uses" current_character if any use exists before a load in the
    // same block; any uses after a local load are not counted because they
    // consume the locally loaded value.
    while (it.current_offset() < end) {
      Bytecode bytecode = it.current_bytecode();
      const BytecodeFlags flags = Bytecodes::Flags(bytecode);

      const bool loads = (flags & ReBcFlag::kLoadsCC) != 0;
      const bool uses = (flags & ReBcFlag::kUsesCC) != 0;

      if (uses && !locally_loaded) {
        uses_current_char_.Add(block_id);
      }
      if (loads) {
        loads_current_char_.Add(block_id);
        locally_loaded = true;
      }

      const uint8_t* current_address = it.current_address();
      it.advance();

      // Only the last instruction of a block can have non-fallthrough
      // successors.
      if (it.current_offset() == end) {
        if (bytecode == Bytecode::kBacktrack) {
          terminates_with_backtrack_.Add(block_id);
          successors_[block_id].push_back(backtrack_dispatch_id());
        } else {
          const bool may_branch =
              (flags & ReBcFlag::kNoBranchDespiteJumpTargetOperand) == 0;
          const bool is_fallthrough = (flags & ReBcFlag::kNoFallthrough) == 0;

          if (may_branch) {
            Bytecodes::DispatchOnBytecode(bytecode, [&]<Bytecode bc>() {
              using Operands = BytecodeOperands<bc>;
              Operands::template ForEachOperandOfType<
                  BytecodeOperandType::kJumpTarget>([&]<auto op>() {
                uint32_t target =
                    Operands::template Get<op>(current_address, no_gc_);
                successors_[block_id].push_back(GetBlockId(target));
              });
            });
          }

          if (is_fallthrough && end < length_) {
            successors_[block_id].push_back(GetBlockId(end));
          }
        }
      }
    }

    // Sort and unique successors to ensure clean adjacency lists.
    auto& succs = successors_[block_id];
    std::sort(succs.begin(), succs.end());
    succs.erase(std::unique(succs.begin(), succs.end()), succs.end());

    // Build predecessors from successors.
    for (uint32_t succ : succs) {
      predecessors_[succ].push_back(block_id);
    }
  }

  // Predecessors for the backtrack dispatch node's targets.
  auto& dispatch_succs = successors_[backtrack_dispatch_id()];
  for (uint32_t succ : dispatch_succs) {
    predecessors_[succ].push_back(backtrack_dispatch_id());
  }

  // Finalize predecessors by sorting and uniquing.
  for (uint32_t block_id = 0; block_id < total_blocks; ++block_id) {
    auto& preds = predecessors_[block_id];
    std::sort(preds.begin(), preds.end());
    preds.erase(std::unique(preds.begin(), preds.end()), preds.end());
  }
}

void BytecodeAnalysis::AnalyzeControlFlow() {
  const uint32_t total_blocks = total_block_count();

  is_loop_header_.Resize(total_blocks, zone_);
  block_to_ebb_id_.assign(total_blocks, -1);

  BitVector visited(total_blocks, zone_);
  BitVector recursion_stack(total_blocks, zone_);

  struct Frame {
    uint32_t block_id;
    int ebb_id;
    bool successors_visited;
  };

  ZoneStack<Frame> dfs_stack(zone_);
  dfs_stack.push({0, 0, false});

  // Iterative DFS for loop detection and reachability.
  while (!dfs_stack.empty()) {
    Frame& frame = dfs_stack.top();
    const uint32_t u = frame.block_id;

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

    for (uint32_t v : successors_[u]) {
      // Edges involving the backtrack dispatch node are not real
      // control flow — they model the runtime backtrack stack, not loops.
      bool is_backtrack_edge =
          u == backtrack_dispatch_id() || v == backtrack_dispatch_id();
      if (!is_backtrack_edge && recursion_stack.Contains(v)) {
        is_loop_header_.Add(v);
        back_edges_.push_back({u, v});
      } else if (!visited.Contains(v)) {
        dfs_stack.push({v, 0, false});
      }
    }
  }

  // Extended Basic Block (EBB) identification.
  // An EBB starts at:
  // 1. The entry block (0).
  // 2. Any block with multiple predecessors.
  // 3. Any block that is the target of a backtrack edge.
  visited.Clear();
  int next_ebb_id = 0;
  block_to_ebb_id_[0] = next_ebb_id;
  dfs_stack.push({0, next_ebb_id++, false});

  while (!dfs_stack.empty()) {
    Frame frame = dfs_stack.top();
    dfs_stack.pop();

    const uint32_t u = frame.block_id;
    if (visited.Contains(u)) {
      continue;
    }

    visited.Add(u);

    for (uint32_t v : successors_[u]) {
      if (visited.Contains(v)) continue;

      int ebb_id = block_to_ebb_id_[v];
      if (ebb_id == -1) {
        ebb_id = frame.ebb_id;
        bool is_backtrack_edge =
            u == backtrack_dispatch_id() || v == backtrack_dispatch_id();
        if (predecessors_[v].size() > 1 || is_backtrack_edge) {
          ebb_id = next_ebb_id++;
        }
        block_to_ebb_id_[v] = ebb_id;
      }
      dfs_stack.push({v, ebb_id, false});
    }
  }

  // Loop member identification.
  for (const auto& edge : back_edges_) {
    uint32_t header = edge.second;
    uint32_t latch = edge.first;

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
      loops_.emplace_back(header, total_blocks, zone_);
      loop = &loops_.back();
      loop->members.Add(header);
    }

    // Add blocks to loop body by walking backwards from latch to header
    // using the predecessor edges.
    if (!loop->members.Contains(latch)) {
      ZoneVector<uint32_t> worklist(zone_);
      worklist.push_back(latch);
      loop->members.Add(latch);

      while (!worklist.empty()) {
        uint32_t block = worklist.back();
        worklist.pop_back();

        if (block == header) continue;

        for (uint32_t pred : predecessors_[block]) {
          if (!loop->members.Contains(pred)) {
            loop->members.Add(pred);
            worklist.push_back(pred);
          }
        }
      }
    }
  }

  // Compute loop exits now that loop infos are complete.
  for (auto& loop : loops_) {
    for (uint32_t block : loop.members) {
      for (uint32_t successor : successors_[block]) {
        if (!loop.members.Contains(successor)) {
          loop.exits.push_back({block, successor});
        }
      }
    }
  }
}

}  // namespace regexp
}  // namespace internal
}  // namespace v8
