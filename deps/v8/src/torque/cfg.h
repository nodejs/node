// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_CFG_H_
#define V8_TORQUE_CFG_H_

#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "src/torque/ast.h"
#include "src/torque/instructions.h"
#include "src/torque/source-positions.h"
#include "src/torque/types.h"

namespace v8 {
namespace internal {
namespace torque {

class ControlFlowGraph;

class Block {
 public:
  explicit Block(ControlFlowGraph* cfg, size_t id,
                 base::Optional<Stack<const Type*>> input_types,
                 bool is_deferred)
      : cfg_(cfg),
        input_types_(std::move(input_types)),
        id_(id),
        is_deferred_(is_deferred) {}
  void Add(Instruction instruction) {
    DCHECK(!IsComplete());
    instructions_.push_back(std::move(instruction));
  }

  bool HasInputTypes() const { return input_types_ != base::nullopt; }
  const Stack<const Type*>& InputTypes() const { return *input_types_; }
  void SetInputTypes(const Stack<const Type*>& input_types);
  void Retype() {
    Stack<const Type*> current_stack = InputTypes();
    for (const Instruction& instruction : instructions()) {
      instruction.TypeInstruction(&current_stack, cfg_);
    }
  }

  const std::vector<Instruction>& instructions() const { return instructions_; }
  bool IsComplete() const {
    return !instructions_.empty() && instructions_.back()->IsBlockTerminator();
  }
  size_t id() const { return id_; }
  bool IsDeferred() const { return is_deferred_; }

 private:
  ControlFlowGraph* cfg_;
  std::vector<Instruction> instructions_;
  base::Optional<Stack<const Type*>> input_types_;
  const size_t id_;
  bool is_deferred_;
};

class ControlFlowGraph {
 public:
  explicit ControlFlowGraph(Stack<const Type*> input_types) {
    start_ = NewBlock(std::move(input_types), false);
    PlaceBlock(start_);
  }

  Block* NewBlock(base::Optional<Stack<const Type*>> input_types,
                  bool is_deferred) {
    blocks_.emplace_back(this, next_block_id_++, std::move(input_types),
                         is_deferred);
    return &blocks_.back();
  }
  void PlaceBlock(Block* block) { placed_blocks_.push_back(block); }
  Block* start() const { return start_; }
  base::Optional<Block*> end() const { return end_; }
  void set_end(Block* end) { end_ = end; }
  void SetReturnType(const Type* t) {
    if (!return_type_) {
      return_type_ = t;
      return;
    }
    if (t != *return_type_) {
      ReportError("expected return type ", **return_type_, " instead of ", *t);
    }
  }
  const std::vector<Block*>& blocks() const { return placed_blocks_; }

 private:
  std::list<Block> blocks_;
  Block* start_;
  std::vector<Block*> placed_blocks_;
  base::Optional<Block*> end_;
  base::Optional<const Type*> return_type_;
  size_t next_block_id_ = 0;
};

class CfgAssembler {
 public:
  explicit CfgAssembler(Stack<const Type*> input_types)
      : current_stack_(std::move(input_types)), cfg_(current_stack_) {}

  const ControlFlowGraph& Result() {
    if (!CurrentBlockIsComplete()) {
      cfg_.set_end(current_block_);
    }
    return cfg_;
  }

  Block* NewBlock(
      base::Optional<Stack<const Type*>> input_types = base::nullopt,
      bool is_deferred = false) {
    return cfg_.NewBlock(std::move(input_types), is_deferred);
  }

  bool CurrentBlockIsComplete() const { return current_block_->IsComplete(); }

  void Emit(Instruction instruction) {
    instruction.TypeInstruction(&current_stack_, &cfg_);
    current_block_->Add(std::move(instruction));
  }

  const Stack<const Type*>& CurrentStack() const { return current_stack_; }

  StackRange TopRange(size_t slot_count) const {
    return CurrentStack().TopRange(slot_count);
  }

  void Bind(Block* block);
  void Goto(Block* block);
  // Goto block while keeping {preserved_slots} many slots on the top and
  // deleting additional the slots below these to match the input type of the
  // target block.
  // Returns the StackRange of the preserved slots in the target block.
  StackRange Goto(Block* block, size_t preserved_slots);
  // The condition must be of type bool and on the top of stack. It is removed
  // from the stack before branching.
  void Branch(Block* if_true, Block* if_false);
  // Delete the specified range of slots, moving upper slots to fill the gap.
  void DeleteRange(StackRange range);
  void DropTo(BottomOffset new_level);
  StackRange Peek(StackRange range, base::Optional<const Type*> type);
  void Poke(StackRange destination, StackRange origin,
            base::Optional<const Type*> type);
  void Print(std::string s);
  void AssertionFailure(std::string message);
  void Unreachable();
  void DebugBreak();

  void PrintCurrentStack(std::ostream& s) { s << "stack: " << current_stack_; }

 private:
  friend class CfgAssemblerScopedTemporaryBlock;
  Stack<const Type*> current_stack_;
  ControlFlowGraph cfg_;
  Block* current_block_ = cfg_.start();
};

class CfgAssemblerScopedTemporaryBlock {
 public:
  CfgAssemblerScopedTemporaryBlock(CfgAssembler* assembler, Block* block)
      : assembler_(assembler), saved_block_(block) {
    saved_stack_ = block->InputTypes();
    DCHECK(!assembler->CurrentBlockIsComplete());
    std::swap(saved_block_, assembler->current_block_);
    std::swap(saved_stack_, assembler->current_stack_);
    assembler->cfg_.PlaceBlock(block);
  }

  ~CfgAssemblerScopedTemporaryBlock() {
    DCHECK(assembler_->CurrentBlockIsComplete());
    std::swap(saved_block_, assembler_->current_block_);
    std::swap(saved_stack_, assembler_->current_stack_);
  }

 private:
  CfgAssembler* assembler_;
  Stack<const Type*> saved_stack_;
  Block* saved_block_;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_CFG_H_
