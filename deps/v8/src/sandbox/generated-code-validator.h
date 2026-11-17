// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_GENERATED_CODE_VALIDATOR_H_
#define V8_SANDBOX_GENERATED_CODE_VALIDATOR_H_

#ifdef V8_ENABLE_GENERATED_CODE_VALIDATOR

#include <algorithm>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/base/functional/function-ref.h"
#include "src/base/small-vector.h"
#include "src/codegen/jump-table-info.h"
#include "src/codegen/reloc-info.h"
#include "src/diagnostics/disasm.h"
#include "src/objects/code.h"

namespace v8::internal {

class InstructionHandler;

class GeneratedCodeValidator {
 public:
  static V8_EXPORT_PRIVATE void Validate(Isolate* isolate, Tagged<Code> code);
  static V8_EXPORT_PRIVATE bool IsValidated(Tagged<Code> code);

 private:
  // Helper class to iterate over the instructions of a Code object while
  // skipping inline data payload such as constant pools, jump tables, and
  // internal references.
  class InstructionIteratorSkippingData {
   public:
    explicit InstructionIteratorSkippingData(Tagged<Code> code);

    bool IsDone() const { return current_ >= end_; }
    const uint8_t* GetCurrent() const {
      DCHECK(!IsDone());
      return current_;
    }
    // Advances the iterator by `instruction_size` bytes, and then checks if
    // the new position points to data that needs to be skipped.
    void Advance(int instruction_size);

   private:
    // Checks if the current position points to data (jump table, reloc info,
    // constant pool) and if so, advances `current_` past it. Loops until
    // `current_` points to an instruction or `end_`.
    void SkipCheck();

    const uint8_t* const start_;
    const uint8_t* const end_;
    const uint8_t* current_;

    std::optional<JumpTableInfoIterator> jump_table_info_it_;
    std::optional<RelocIterator> reloc_it_;
    disasm::NameConverter converter_;
    disasm::Disassembler disasm_;
  };

  class ViolationsReporter {
   public:
    explicit ViolationsReporter(Tagged<Code> code);
    ~ViolationsReporter();

    void ReportDisassemblyFailed(const uint8_t* pc,
                                 size_t max_instruction_size);
    void ReportViolationWithInstruction(const uint8_t* pc, std::string instr,
                                        std::string error);
    void RecordDisassembledInstruction(const uint8_t* pc,
                                       size_t instruction_size,
                                       std::string instr);

   private:
    void ReportViolation(const uint8_t* pc, std::string error);
    void PrintPrologueIfNeeded();
    void PrintEpilogueIfNeeded();

    const Tagged<Code> code_;
    const uint8_t* const code_start_;
    std::stringstream disassembled_instructions_;
    bool violations_found_ = false;
  };

  class Utils {
   public:
    static bool IsEntryCode(const Tagged<Code> code);
    static bool IsDeoptCode(const Tagged<Code> code);
  };

  struct State {
    enum class BoolValue : uint8_t {
      kFalse,
      kTrue,
      kMixed,
    };

    static State CreateNewState(const Tagged<Code> code);
    static State Merge(const State& state1, const State& state2);
    static BoolValue MergeBoolValues(BoolValue v1, BoolValue v2) {
      return v1 == v2 ? v1 : BoolValue::kMixed;
    }

    bool operator==(const State& other) const = default;

    BoolValue is_root_reg_valid_;
    // TODO(523128533): Check this field when checking memory writes to sandbox.
    BoolValue is_cage_base_reg_valid_;
#if V8_TARGET_ARCH_ARM64
    struct RootRegisterInitState {
      static RootRegisterInitState Merge(const RootRegisterInitState& state1,
                                         const RootRegisterInitState& state2);

      bool operator==(const RootRegisterInitState& other) const = default;

      Address current_value = kNullAddress;
      BoolValue in_progress = BoolValue::kFalse;
    } root_reg_init_state_;
#endif  // V8_TARGET_ARCH_ARM64
  };

  class ControlFlowGraphBase {
   public:
    class NodeBase {
     public:
      NodeBase(const uint8_t* pc,
               base::SmallVector<const uint8_t*, 2> successors)
          : pc_(pc), successors_(std::move(successors)) {}

      const uint8_t* pc() const { return pc_; }
      const base::SmallVector<const uint8_t*, 2>& successors() const {
        return successors_;
      }

      bool has_input_state() const { return input_state_.has_value(); }
      const State& input_state() const {
        DCHECK(input_state_.has_value());
        return *input_state_;
      }
      bool MergeInputState(const State& state);
      bool UpdateOutputState(const State& state);

      bool is_pending() const { return is_pending_; }
      void set_is_pending(bool is_pending) { is_pending_ = is_pending; }

     private:
      const uint8_t* const pc_;
      base::SmallVector<const uint8_t*, 2> successors_;
      std::optional<State> input_state_;
      std::optional<State> output_state_;
      bool is_pending_ = false;
    };

    explicit ControlFlowGraphBase(Tagged<Code> code);
    virtual ~ControlFlowGraphBase() = default;

   protected:
    void RecordNodePc(const uint8_t* pc) {
      DCHECK(recorded_pcs_.empty() || pc > recorded_pcs_.back());
      recorded_pcs_.push_back(pc);
    }
    virtual NodeBase* GetNodeAt(const uint8_t* pc) = 0;
    void ValidateNodes(base::FunctionRef<State(const NodeBase&)> update_state,
                       base::FunctionRef<void(const NodeBase&)> check_state);
    bool IsPcInBounds(const uint8_t* pc) const {
      return pc >= code_start_ && pc < code_end_;
    }

   private:
    void PropagateStates(
        base::FunctionRef<State(const NodeBase&)> update_state);
    void CheckStates(base::FunctionRef<void(const NodeBase&)> check_state);

    const Tagged<Code> code_;
    const uint8_t* const code_start_;
    const uint8_t* const code_end_;
    std::vector<const uint8_t*> recorded_pcs_;
  };

  template <typename Instr>
  class ControlFlowGraph : public ControlFlowGraphBase {
   public:
    class Node : public NodeBase {
     public:
      Node(const uint8_t* pc, const Instr& instr,
           base::SmallVector<const uint8_t*, 2> successors)
          : NodeBase(pc, std::move(successors)), instr_(instr) {}

      const Instr& instr() const { return instr_; }

     private:
      const Instr instr_;
    };

    explicit ControlFlowGraph(Tagged<Code> code) : ControlFlowGraphBase(code) {
      nodes_.reserve(EstimateInstructionCount(code));
    }

    void AddNode(const uint8_t* pc, const Instr& instr,
                 base::SmallVector<const uint8_t*, 2> successors) {
      auto [it, inserted] =
          nodes_.try_emplace(pc, pc, instr, std::move(successors));
      DCHECK(inserted);
      USE(inserted);
      RecordNodePc(pc);
    }

    template <typename InstructionHandler>
    void Validate(InstructionHandler& instruction_handler) {
      ValidateNodes(
          [&](const NodeBase& node) {
            return instruction_handler.UpdateState(
                static_cast<const Node&>(node));
          },
          [&](const NodeBase& node) {
            instruction_handler.CheckState(static_cast<const Node&>(node));
          });
    }

   protected:
    NodeBase* GetNodeAt(const uint8_t* pc) override {
      auto it = nodes_.find(pc);
      return it != nodes_.end() ? &it->second : nullptr;
    }

   private:
    absl::flat_hash_map<const uint8_t*, Node> nodes_;
  };

  static size_t EstimateInstructionCount(Tagged<Code> code);
  static void ValidateImpl(Isolate* isolate, Tagged<Code> code);

  friend class InstructionHandler;
};

}  // namespace v8::internal

#endif  // V8_ENABLE_GENERATED_CODE_VALIDATOR

#endif  // V8_SANDBOX_GENERATED_CODE_VALIDATOR_H_
