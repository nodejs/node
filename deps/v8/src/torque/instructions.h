// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_INSTRUCTIONS_H_
#define V8_TORQUE_INSTRUCTIONS_H_

#include <memory>

#include "src/torque/ast.h"
#include "src/torque/source-positions.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class Block;
class Builtin;
class ControlFlowGraph;
class Macro;
class ModuleConstant;
class RuntimeFunction;

#define TORQUE_INSTRUCTION_LIST(V)    \
  V(PeekInstruction)                  \
  V(PokeInstruction)                  \
  V(DeleteRangeInstruction)           \
  V(PushUninitializedInstruction)     \
  V(PushCodePointerInstruction)       \
  V(CallCsaMacroInstruction)          \
  V(ModuleConstantInstruction)        \
  V(CallCsaMacroAndBranchInstruction) \
  V(CallBuiltinInstruction)           \
  V(CallRuntimeInstruction)           \
  V(CallBuiltinPointerInstruction)    \
  V(BranchInstruction)                \
  V(ConstexprBranchInstruction)       \
  V(GotoInstruction)                  \
  V(GotoExternalInstruction)          \
  V(ReturnInstruction)                \
  V(PrintConstantStringInstruction)   \
  V(DebugBreakInstruction)            \
  V(UnsafeCastInstruction)

#define TORQUE_INSTRUCTION_BOILERPLATE()                                 \
  static const InstructionKind kKind;                                    \
  std::unique_ptr<InstructionBase> Clone() const override;               \
  void Assign(const InstructionBase& other) override;                    \
  void TypeInstruction(Stack<const Type*>* stack, ControlFlowGraph* cfg) \
      const override;

enum class InstructionKind {
#define ENUM_ITEM(name) k##name,
  TORQUE_INSTRUCTION_LIST(ENUM_ITEM)
#undef ENUM_ITEM
};

struct InstructionBase {
  InstructionBase() : pos(CurrentSourcePosition::Get()) {}
  virtual std::unique_ptr<InstructionBase> Clone() const = 0;
  virtual void Assign(const InstructionBase& other) = 0;
  virtual ~InstructionBase() = default;

  virtual void TypeInstruction(Stack<const Type*>* stack,
                               ControlFlowGraph* cfg) const = 0;
  virtual bool IsBlockTerminator() const { return false; }
  virtual void AppendSuccessorBlocks(std::vector<Block*>* block_list) const {}

  SourcePosition pos;
};

class Instruction {
 public:
  template <class T>
  Instruction(T instr)  // NOLINT(runtime/explicit)
      : kind_(T::kKind), instruction_(new T(std::move(instr))) {}

  template <class T>
  T& Cast() {
    DCHECK(Is<T>());
    return static_cast<T&>(*instruction_);
  }

  template <class T>
  const T& Cast() const {
    DCHECK(Is<T>());
    return static_cast<const T&>(*instruction_);
  }

  template <class T>
  bool Is() const {
    return kind_ == T::kKind;
  }

  template <class T>
  T* DynamicCast() {
    if (Is<T>()) return &Cast<T>();
    return nullptr;
  }

  template <class T>
  const T* DynamicCast() const {
    if (Is<T>()) return &Cast<T>();
    return nullptr;
  }

  Instruction(const Instruction& other)
      : kind_(other.kind_), instruction_(other.instruction_->Clone()) {}
  Instruction& operator=(const Instruction& other) {
    if (kind_ == other.kind_) {
      instruction_->Assign(*other.instruction_);
    } else {
      kind_ = other.kind_;
      instruction_ = other.instruction_->Clone();
    }
    return *this;
  }

  InstructionKind kind() const { return kind_; }
  void TypeInstruction(Stack<const Type*>* stack, ControlFlowGraph* cfg) const {
    return instruction_->TypeInstruction(stack, cfg);
  }

  InstructionBase* operator->() { return instruction_.get(); }
  const InstructionBase* operator->() const { return instruction_.get(); }

 private:
  InstructionKind kind_;
  std::unique_ptr<InstructionBase> instruction_;
};

struct PeekInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()

  PeekInstruction(BottomOffset slot, base::Optional<const Type*> widened_type)
      : slot(slot), widened_type(widened_type) {}

  BottomOffset slot;
  base::Optional<const Type*> widened_type;
};

struct PokeInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()

  PokeInstruction(BottomOffset slot, base::Optional<const Type*> widened_type)
      : slot(slot), widened_type(widened_type) {}

  BottomOffset slot;
  base::Optional<const Type*> widened_type;
};

// Preserve the top {preserved_slots} number of slots, and delete
// {deleted_slots} number or slots below.
struct DeleteRangeInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit DeleteRangeInstruction(StackRange range) : range(range) {}

  StackRange range;
};

struct PushUninitializedInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit PushUninitializedInstruction(const Type* type) : type(type) {}

  const Type* type;
};

struct PushCodePointerInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  PushCodePointerInstruction(std::string external_name, const Type* type)
      : external_name(std::move(external_name)), type(type) {
    DCHECK(type->IsFunctionPointerType());
  }

  std::string external_name;
  const Type* type;
};

struct ModuleConstantInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit ModuleConstantInstruction(ModuleConstant* constant)
      : constant(constant) {}

  ModuleConstant* constant;
};

struct CallCsaMacroInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  CallCsaMacroInstruction(Macro* macro,
                          std::vector<std::string> constexpr_arguments)
      : macro(macro), constexpr_arguments(constexpr_arguments) {}

  Macro* macro;
  std::vector<std::string> constexpr_arguments;
};

struct CallCsaMacroAndBranchInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  CallCsaMacroAndBranchInstruction(Macro* macro,
                                   std::vector<std::string> constexpr_arguments,
                                   base::Optional<Block*> return_continuation,
                                   std::vector<Block*> label_blocks)
      : macro(macro),
        constexpr_arguments(constexpr_arguments),
        return_continuation(return_continuation),
        label_blocks(label_blocks) {}
  bool IsBlockTerminator() const override { return true; }
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    if (return_continuation) block_list->push_back(*return_continuation);
    for (Block* block : label_blocks) block_list->push_back(block);
  }

  Macro* macro;
  std::vector<std::string> constexpr_arguments;
  base::Optional<Block*> return_continuation;
  std::vector<Block*> label_blocks;
};

struct CallBuiltinInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return is_tailcall; }
  CallBuiltinInstruction(bool is_tailcall, Builtin* builtin, size_t argc)
      : is_tailcall(is_tailcall), builtin(builtin), argc(argc) {}

  bool is_tailcall;
  Builtin* builtin;
  size_t argc;
};

struct CallBuiltinPointerInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return is_tailcall; }
  CallBuiltinPointerInstruction(bool is_tailcall, Builtin* example_builtin,
                                size_t argc)
      : is_tailcall(is_tailcall),
        example_builtin(example_builtin),
        argc(argc) {}

  bool is_tailcall;
  Builtin* example_builtin;
  size_t argc;
};

struct CallRuntimeInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return is_tailcall; }

  CallRuntimeInstruction(bool is_tailcall, RuntimeFunction* runtime_function,
                         size_t argc)
      : is_tailcall(is_tailcall),
        runtime_function(runtime_function),
        argc(argc) {}

  bool is_tailcall;
  RuntimeFunction* runtime_function;
  size_t argc;
};

struct BranchInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return true; }
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    block_list->push_back(if_true);
    block_list->push_back(if_false);
  }

  BranchInstruction(Block* if_true, Block* if_false)
      : if_true(if_true), if_false(if_false) {}

  Block* if_true;
  Block* if_false;
};

struct ConstexprBranchInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return true; }
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    block_list->push_back(if_true);
    block_list->push_back(if_false);
  }

  ConstexprBranchInstruction(std::string condition, Block* if_true,
                             Block* if_false)
      : condition(condition), if_true(if_true), if_false(if_false) {}

  std::string condition;
  Block* if_true;
  Block* if_false;
};

struct GotoInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return true; }
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    block_list->push_back(destination);
  }

  explicit GotoInstruction(Block* destination) : destination(destination) {}

  Block* destination;
};

struct GotoExternalInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return true; }

  GotoExternalInstruction(std::string destination,
                          std::vector<std::string> variable_names)
      : destination(std::move(destination)),
        variable_names(std::move(variable_names)) {}

  std::string destination;
  std::vector<std::string> variable_names;
};

struct ReturnInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return true; }
};

struct PrintConstantStringInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit PrintConstantStringInstruction(std::string message) {
    // The normal way to write this triggers a bug in Clang on Windows.
    this->message = std::move(message);
  }

  std::string message;
};

struct DebugBreakInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return never_continues; }
  explicit DebugBreakInstruction(bool never_continues)
      : never_continues(never_continues) {}

  bool never_continues;
};

struct UnsafeCastInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit UnsafeCastInstruction(const Type* destination_type)
      : destination_type(destination_type) {}

  const Type* destination_type;
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_INSTRUCTIONS_H_
