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
class Intrinsic;
class Macro;
class NamespaceConstant;
class RuntimeFunction;

#define TORQUE_INSTRUCTION_LIST(V)    \
  V(PeekInstruction)                  \
  V(PokeInstruction)                  \
  V(DeleteRangeInstruction)           \
  V(PushUninitializedInstruction)     \
  V(PushBuiltinPointerInstruction)    \
  V(CreateFieldReferenceInstruction)  \
  V(LoadReferenceInstruction)         \
  V(StoreReferenceInstruction)        \
  V(CallCsaMacroInstruction)          \
  V(CallIntrinsicInstruction)         \
  V(NamespaceConstantInstruction)     \
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
  V(AbortInstruction)                 \
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
  void InvalidateTransientTypes(Stack<const Type*>* stack) const;
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

  Instruction(const Instruction& other) V8_NOEXCEPT
      : kind_(other.kind_),
        instruction_(other.instruction_->Clone()) {}
  Instruction& operator=(const Instruction& other) V8_NOEXCEPT {
    if (kind_ == other.kind_) {
      instruction_->Assign(*other.instruction_);
    } else {
      kind_ = other.kind_;
      instruction_ = other.instruction_->Clone();
    }
    return *this;
  }

  InstructionKind kind() const { return kind_; }
  const char* Mnemonic() const {
    switch (kind()) {
#define ENUM_ITEM(name)          \
  case InstructionKind::k##name: \
    return #name;
      TORQUE_INSTRUCTION_LIST(ENUM_ITEM)
#undef ENUM_ITEM
      default:
        UNREACHABLE();
    }
  }
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

struct PushBuiltinPointerInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  PushBuiltinPointerInstruction(std::string external_name, const Type* type)
      : external_name(std::move(external_name)), type(type) {
    DCHECK(type->IsBuiltinPointerType());
  }

  std::string external_name;
  const Type* type;
};

struct NamespaceConstantInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit NamespaceConstantInstruction(NamespaceConstant* constant)
      : constant(constant) {}

  NamespaceConstant* constant;
};

struct CreateFieldReferenceInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  CreateFieldReferenceInstruction(const ClassType* class_type,
                                  std::string field_name)
      : class_type(class_type), field_name(std::move(field_name)) {}
  const ClassType* class_type;
  std::string field_name;
};

struct LoadReferenceInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit LoadReferenceInstruction(const Type* type) : type(type) {}
  const Type* type;
};

struct StoreReferenceInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  explicit StoreReferenceInstruction(const Type* type) : type(type) {}
  const Type* type;
};

struct CallIntrinsicInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  CallIntrinsicInstruction(Intrinsic* intrinsic,
                           TypeVector specialization_types,
                           std::vector<std::string> constexpr_arguments)
      : intrinsic(intrinsic),
        specialization_types(std::move(specialization_types)),
        constexpr_arguments(constexpr_arguments) {}

  Intrinsic* intrinsic;
  TypeVector specialization_types;
  std::vector<std::string> constexpr_arguments;
};

struct CallCsaMacroInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  CallCsaMacroInstruction(Macro* macro,
                          std::vector<std::string> constexpr_arguments,
                          base::Optional<Block*> catch_block)
      : macro(macro),
        constexpr_arguments(constexpr_arguments),
        catch_block(catch_block) {}
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    if (catch_block) block_list->push_back(*catch_block);
  }

  Macro* macro;
  std::vector<std::string> constexpr_arguments;
  base::Optional<Block*> catch_block;
};

struct CallCsaMacroAndBranchInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  CallCsaMacroAndBranchInstruction(Macro* macro,
                                   std::vector<std::string> constexpr_arguments,
                                   base::Optional<Block*> return_continuation,
                                   std::vector<Block*> label_blocks,
                                   base::Optional<Block*> catch_block)
      : macro(macro),
        constexpr_arguments(constexpr_arguments),
        return_continuation(return_continuation),
        label_blocks(label_blocks),
        catch_block(catch_block) {}
  bool IsBlockTerminator() const override { return true; }
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    if (catch_block) block_list->push_back(*catch_block);
    if (return_continuation) block_list->push_back(*return_continuation);
    for (Block* block : label_blocks) block_list->push_back(block);
  }

  Macro* macro;
  std::vector<std::string> constexpr_arguments;
  base::Optional<Block*> return_continuation;
  std::vector<Block*> label_blocks;
  base::Optional<Block*> catch_block;
};

struct CallBuiltinInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return is_tailcall; }
  CallBuiltinInstruction(bool is_tailcall, Builtin* builtin, size_t argc,
                         base::Optional<Block*> catch_block)
      : is_tailcall(is_tailcall),
        builtin(builtin),
        argc(argc),
        catch_block(catch_block) {}
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    if (catch_block) block_list->push_back(*catch_block);
  }

  bool is_tailcall;
  Builtin* builtin;
  size_t argc;
  base::Optional<Block*> catch_block;
};

struct CallBuiltinPointerInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override { return is_tailcall; }
  CallBuiltinPointerInstruction(bool is_tailcall,
                                const BuiltinPointerType* type, size_t argc)
      : is_tailcall(is_tailcall), type(type), argc(argc) {}

  bool is_tailcall;
  const BuiltinPointerType* type;
  size_t argc;
};

struct CallRuntimeInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  bool IsBlockTerminator() const override;

  CallRuntimeInstruction(bool is_tailcall, RuntimeFunction* runtime_function,
                         size_t argc, base::Optional<Block*> catch_block)
      : is_tailcall(is_tailcall),
        runtime_function(runtime_function),
        argc(argc),
        catch_block(catch_block) {}
  void AppendSuccessorBlocks(std::vector<Block*>* block_list) const override {
    if (catch_block) block_list->push_back(*catch_block);
  }

  bool is_tailcall;
  RuntimeFunction* runtime_function;
  size_t argc;
  base::Optional<Block*> catch_block;
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
  explicit PrintConstantStringInstruction(std::string message)
      : message(std::move(message)) {}

  std::string message;
};

struct AbortInstruction : InstructionBase {
  TORQUE_INSTRUCTION_BOILERPLATE()
  enum class Kind { kDebugBreak, kUnreachable, kAssertionFailure };
  bool IsBlockTerminator() const override { return kind != Kind::kDebugBreak; }
  explicit AbortInstruction(Kind kind, std::string message = "")
      : kind(kind), message(std::move(message)) {}

  Kind kind;
  std::string message;
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
