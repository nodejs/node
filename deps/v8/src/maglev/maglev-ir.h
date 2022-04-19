// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_IR_H_
#define V8_MAGLEV_MAGLEV_IR_H_

#include "src/base/bit-field.h"
#include "src/base/macros.h"
#include "src/base/small-vector.h"
#include "src/base/threaded-list.h"
#include "src/codegen/label.h"
#include "src/codegen/reglist.h"
#include "src/common/globals.h"
#include "src/common/operation.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/heap-refs.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/objects/smi.h"
#include "src/roots/roots.h"
#include "src/utils/utils.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace maglev {

class BasicBlock;
class ProcessingState;
class MaglevCodeGenState;
class MaglevCompilationUnit;
class MaglevGraphLabeller;
class MaglevVregAllocationState;
class CompactInterpreterFrameState;

// Nodes are either
// 1. side-effecting or value-holding SSA nodes in the body of basic blocks, or
// 2. Control nodes that store the control flow at the end of basic blocks, and
// form a separate node hierarchy to non-control nodes.
//
// The macro lists below must match the node class hierarchy.

#define GENERIC_OPERATIONS_NODE_LIST(V) \
  V(GenericAdd)                         \
  V(GenericSubtract)                    \
  V(GenericMultiply)                    \
  V(GenericDivide)                      \
  V(GenericModulus)                     \
  V(GenericExponentiate)                \
  V(GenericBitwiseAnd)                  \
  V(GenericBitwiseOr)                   \
  V(GenericBitwiseXor)                  \
  V(GenericShiftLeft)                   \
  V(GenericShiftRight)                  \
  V(GenericShiftRightLogical)           \
  V(GenericBitwiseNot)                  \
  V(GenericNegate)                      \
  V(GenericIncrement)                   \
  V(GenericDecrement)                   \
  V(GenericEqual)                       \
  V(GenericStrictEqual)                 \
  V(GenericLessThan)                    \
  V(GenericLessThanOrEqual)             \
  V(GenericGreaterThan)                 \
  V(GenericGreaterThanOrEqual)

#define VALUE_NODE_LIST(V) \
  V(Call)                  \
  V(Constant)              \
  V(InitialValue)          \
  V(LoadField)             \
  V(LoadGlobal)            \
  V(LoadNamedGeneric)      \
  V(Phi)                   \
  V(RegisterInput)         \
  V(RootConstant)          \
  V(SmiConstant)           \
  V(CheckedSmiTag)         \
  V(CheckedSmiUntag)       \
  V(Int32AddWithOverflow)  \
  V(Int32Constant)         \
  GENERIC_OPERATIONS_NODE_LIST(V)

#define NODE_LIST(V) \
  V(CheckMaps)       \
  V(GapMove)         \
  V(StoreField)      \
  VALUE_NODE_LIST(V)

#define CONDITIONAL_CONTROL_NODE_LIST(V) \
  V(BranchIfTrue)                        \
  V(BranchIfToBooleanTrue)

#define UNCONDITIONAL_CONTROL_NODE_LIST(V) \
  V(Jump)                                  \
  V(JumpLoop)

#define CONTROL_NODE_LIST(V)       \
  V(Return)                        \
  V(Deopt)                         \
  CONDITIONAL_CONTROL_NODE_LIST(V) \
  UNCONDITIONAL_CONTROL_NODE_LIST(V)

#define NODE_BASE_LIST(V) \
  NODE_LIST(V)            \
  CONTROL_NODE_LIST(V)

// Define the opcode enum.
#define DEF_OPCODES(type) k##type,
enum class Opcode : uint8_t { NODE_BASE_LIST(DEF_OPCODES) };
#undef DEF_OPCODES
#define PLUS_ONE(type) +1
static constexpr int kOpcodeCount = NODE_BASE_LIST(PLUS_ONE);
static constexpr Opcode kFirstOpcode = static_cast<Opcode>(0);
static constexpr Opcode kLastOpcode = static_cast<Opcode>(kOpcodeCount - 1);
#undef PLUS_ONE

const char* ToString(Opcode opcode);
inline std::ostream& operator<<(std::ostream& os, Opcode opcode) {
  return os << ToString(opcode);
}

#define V(Name) Opcode::k##Name,
static constexpr Opcode kFirstValueNodeOpcode =
    std::min({VALUE_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastValueNodeOpcode =
    std::max({VALUE_NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kFirstNodeOpcode = std::min({NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastNodeOpcode = std::max({NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kFirstConditionalControlNodeOpcode =
    std::min({CONDITIONAL_CONTROL_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastConditionalControlNodeOpcode =
    std::max({CONDITIONAL_CONTROL_NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kLastUnconditionalControlNodeOpcode =
    std::max({UNCONDITIONAL_CONTROL_NODE_LIST(V) kFirstOpcode});
static constexpr Opcode kFirstUnconditionalControlNodeOpcode =
    std::min({UNCONDITIONAL_CONTROL_NODE_LIST(V) kLastOpcode});

static constexpr Opcode kFirstControlNodeOpcode =
    std::min({CONTROL_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastControlNodeOpcode =
    std::max({CONTROL_NODE_LIST(V) kFirstOpcode});
#undef V

constexpr bool IsValueNode(Opcode opcode) {
  return kFirstValueNodeOpcode <= opcode && opcode <= kLastValueNodeOpcode;
}
constexpr bool IsConditionalControlNode(Opcode opcode) {
  return kFirstConditionalControlNodeOpcode <= opcode &&
         opcode <= kLastConditionalControlNodeOpcode;
}
constexpr bool IsUnconditionalControlNode(Opcode opcode) {
  return kFirstUnconditionalControlNodeOpcode <= opcode &&
         opcode <= kLastUnconditionalControlNodeOpcode;
}

// Forward-declare NodeBase sub-hierarchies.
class Node;
class ControlNode;
class ConditionalControlNode;
class UnconditionalControlNode;
class ValueNode;

enum class ValueRepresentation {
  kTagged,
  kUntagged,
};

#define DEF_FORWARD_DECLARATION(type, ...) class type;
NODE_BASE_LIST(DEF_FORWARD_DECLARATION)
#undef DEF_FORWARD_DECLARATION

using NodeIdT = uint32_t;
static constexpr uint32_t kInvalidNodeId = 0;

class OpProperties {
 public:
  constexpr bool is_call() const { return kIsCallBit::decode(bitfield_); }
  constexpr bool can_eager_deopt() const {
    return kCanEagerDeoptBit::decode(bitfield_);
  }
  constexpr bool can_lazy_deopt() const {
    return kCanLazyDeoptBit::decode(bitfield_);
  }
  constexpr bool can_read() const { return kCanReadBit::decode(bitfield_); }
  constexpr bool can_write() const { return kCanWriteBit::decode(bitfield_); }
  constexpr bool non_memory_side_effects() const {
    return kNonMemorySideEffectsBit::decode(bitfield_);
  }
  constexpr bool is_untagged_value() const {
    return kUntaggedValueBit::decode(bitfield_);
  }

  constexpr bool is_pure() const {
    return (bitfield_ | kPureMask) == kPureValue;
  }
  constexpr bool is_required_when_unused() const {
    return can_write() || non_memory_side_effects();
  }

  constexpr OpProperties operator|(const OpProperties& that) {
    return OpProperties(bitfield_ | that.bitfield_);
  }

  static constexpr OpProperties Pure() { return OpProperties(kPureValue); }
  static constexpr OpProperties Call() {
    return OpProperties(kIsCallBit::encode(true));
  }
  static constexpr OpProperties EagerDeopt() {
    return OpProperties(kCanEagerDeoptBit::encode(true));
  }
  static constexpr OpProperties LazyDeopt() {
    return OpProperties(kCanLazyDeoptBit::encode(true));
  }
  static constexpr OpProperties Reading() {
    return OpProperties(kCanReadBit::encode(true));
  }
  static constexpr OpProperties Writing() {
    return OpProperties(kCanWriteBit::encode(true));
  }
  static constexpr OpProperties NonMemorySideEffects() {
    return OpProperties(kNonMemorySideEffectsBit::encode(true));
  }
  static constexpr OpProperties UntaggedValue() {
    return OpProperties(kUntaggedValueBit::encode(true));
  }
  static constexpr OpProperties JSCall() {
    return Call() | NonMemorySideEffects() | LazyDeopt();
  }
  static constexpr OpProperties AnySideEffects() {
    return Reading() | Writing() | NonMemorySideEffects();
  }

  constexpr explicit OpProperties(uint32_t bitfield) : bitfield_(bitfield) {}
  operator uint32_t() const { return bitfield_; }

 private:
  using kIsCallBit = base::BitField<bool, 0, 1>;
  using kCanEagerDeoptBit = kIsCallBit::Next<bool, 1>;
  using kCanLazyDeoptBit = kCanEagerDeoptBit::Next<bool, 1>;
  using kCanReadBit = kCanLazyDeoptBit::Next<bool, 1>;
  using kCanWriteBit = kCanReadBit::Next<bool, 1>;
  using kNonMemorySideEffectsBit = kCanWriteBit::Next<bool, 1>;
  using kUntaggedValueBit = kNonMemorySideEffectsBit::Next<bool, 1>;

  static const uint32_t kPureMask = kCanReadBit::kMask | kCanWriteBit::kMask |
                                    kNonMemorySideEffectsBit::kMask;
  static const uint32_t kPureValue = kCanReadBit::encode(false) |
                                     kCanWriteBit::encode(false) |
                                     kNonMemorySideEffectsBit::encode(false);

  const uint32_t bitfield_;

 public:
  static const size_t kSize = kUntaggedValueBit::kLastUsedBit + 1;
};

class ValueLocation {
 public:
  ValueLocation() = default;

  template <typename... Args>
  void SetUnallocated(Args&&... args) {
    DCHECK(operand_.IsInvalid());
    operand_ = compiler::UnallocatedOperand(args...);
  }

  template <typename... Args>
  void SetAllocated(Args&&... args) {
    DCHECK(operand_.IsUnallocated());
    operand_ = compiler::AllocatedOperand(args...);
  }

  // Only to be used on inputs that inherit allocation.
  template <typename... Args>
  void InjectAllocated(Args&&... args) {
    operand_ = compiler::AllocatedOperand(args...);
  }

  template <typename... Args>
  void SetConstant(Args&&... args) {
    DCHECK(operand_.IsUnallocated());
    operand_ = compiler::ConstantOperand(args...);
  }

  Register AssignedRegister() const {
    return Register::from_code(
        compiler::AllocatedOperand::cast(operand_).register_code());
  }

  const compiler::InstructionOperand& operand() const { return operand_; }
  const compiler::InstructionOperand& operand() { return operand_; }

 private:
  compiler::InstructionOperand operand_;
};

class InputLocation : public ValueLocation {
 public:
  NodeIdT next_use_id() const { return next_use_id_; }
  // Used in ValueNode::mark_use
  NodeIdT* get_next_use_id_address() { return &next_use_id_; }

 private:
  NodeIdT next_use_id_ = kInvalidNodeId;
};

class Input : public InputLocation {
 public:
  explicit Input(ValueNode* node) : node_(node) {}
  ValueNode* node() const { return node_; }

 private:
  ValueNode* node_;
};

class CheckpointedInterpreterState {
 public:
  CheckpointedInterpreterState() = default;
  CheckpointedInterpreterState(BytecodeOffset bytecode_position,
                               const CompactInterpreterFrameState* state)
      : bytecode_position(bytecode_position), register_frame(state) {}

  BytecodeOffset bytecode_position = BytecodeOffset::None();
  const CompactInterpreterFrameState* register_frame = nullptr;
};

class DeoptInfo {
 protected:
  DeoptInfo(Zone* zone, const MaglevCompilationUnit& compilation_unit,
            CheckpointedInterpreterState checkpoint);

 public:
  CheckpointedInterpreterState state;
  InputLocation* input_locations = nullptr;
  Label deopt_entry_label;
  int deopt_index = -1;
};

class EagerDeoptInfo : public DeoptInfo {
 public:
  EagerDeoptInfo(Zone* zone, const MaglevCompilationUnit& compilation_unit,
                 CheckpointedInterpreterState checkpoint)
      : DeoptInfo(zone, compilation_unit, checkpoint) {}
};

class LazyDeoptInfo : public DeoptInfo {
 public:
  LazyDeoptInfo(Zone* zone, const MaglevCompilationUnit& compilation_unit,
                CheckpointedInterpreterState checkpoint)
      : DeoptInfo(zone, compilation_unit, checkpoint) {}

  int deopting_call_return_pc = -1;
  interpreter::Register result_location =
      interpreter::Register::invalid_value();
};

// Dummy type for the initial raw allocation.
struct NodeWithInlineInputs {};

namespace detail {
// Helper for getting the static opcode of a Node subclass. This is in a
// "detail" namespace rather than in NodeBase because we can't template
// specialize outside of namespace scopes before C++17.
template <class T>
struct opcode_of_helper;

#define DEF_OPCODE_OF(Name)                          \
  template <>                                        \
  struct opcode_of_helper<Name> {                    \
    static constexpr Opcode value = Opcode::k##Name; \
  };
NODE_BASE_LIST(DEF_OPCODE_OF)
#undef DEF_OPCODE_OF

}  // namespace detail

class NodeBase : public ZoneObject {
 private:
  // Bitfield specification.
  using OpcodeField = base::BitField<Opcode, 0, 6>;
  STATIC_ASSERT(OpcodeField::is_valid(kLastOpcode));
  using OpPropertiesField =
      OpcodeField::Next<OpProperties, OpProperties::kSize>;
  using InputCountField = OpPropertiesField::Next<uint16_t, 16>;

 protected:
  // Subclasses may use the remaining bitfield bits.
  template <class T, int size>
  using NextBitField = InputCountField::Next<T, size>;

  template <class T>
  static constexpr Opcode opcode_of = detail::opcode_of_helper<T>::value;

 public:
  template <class Derived, typename... Args>
  static Derived* New(Zone* zone, std::initializer_list<ValueNode*> inputs,
                      Args&&... args) {
    Derived* node =
        Allocate<Derived>(zone, inputs.size(), std::forward<Args>(args)...);

    int i = 0;
    for (ValueNode* input : inputs) {
      DCHECK_NOT_NULL(input);
      node->set_input(i++, input);
    }

    return node;
  }

  template <class Derived, typename... Args>
  static Derived* New(Zone* zone, const MaglevCompilationUnit& compilation_unit,
                      CheckpointedInterpreterState checkpoint, Args&&... args) {
    Derived* node = New<Derived>(zone, std::forward<Args>(args)...);
    if constexpr (Derived::kProperties.can_eager_deopt()) {
      new (node->eager_deopt_info_address())
          EagerDeoptInfo(zone, compilation_unit, checkpoint);
    } else {
      STATIC_ASSERT(Derived::kProperties.can_lazy_deopt());
      new (node->lazy_deopt_info_address())
          LazyDeoptInfo(zone, compilation_unit, checkpoint);
    }
    return node;
  }

  // Inputs must be initialized manually.
  template <class Derived, typename... Args>
  static Derived* New(Zone* zone, size_t input_count, Args&&... args) {
    Derived* node =
        Allocate<Derived>(zone, input_count, std::forward<Args>(args)...);
    return node;
  }

  // Overwritten by subclasses.
  static constexpr OpProperties kProperties = OpProperties::Pure();

  constexpr Opcode opcode() const { return OpcodeField::decode(bit_field_); }
  OpProperties properties() const {
    return OpPropertiesField::decode(bit_field_);
  }

  template <class T>
  constexpr bool Is() const;

  template <class T>
  constexpr T* Cast() {
    DCHECK(Is<T>());
    return static_cast<T*>(this);
  }
  template <class T>
  constexpr const T* Cast() const {
    DCHECK(Is<T>());
    return static_cast<const T*>(this);
  }
  template <class T>
  constexpr T* TryCast() {
    return Is<T>() ? static_cast<T*>(this) : nullptr;
  }

  constexpr bool has_inputs() const { return input_count() > 0; }
  constexpr uint16_t input_count() const {
    return InputCountField::decode(bit_field_);
  }

  Input& input(int index) { return *input_address(index); }
  const Input& input(int index) const { return *input_address(index); }

  // Input iterators, use like:
  //
  //  for (Input& input : *node) { ... }
  auto begin() { return std::make_reverse_iterator(input_address(-1)); }
  auto end() {
    return std::make_reverse_iterator(input_address(input_count() - 1));
  }

  constexpr NodeIdT id() const {
    DCHECK_NE(id_, kInvalidNodeId);
    return id_;
  }
  void set_id(NodeIdT id) {
    DCHECK_EQ(id_, kInvalidNodeId);
    DCHECK_NE(id, kInvalidNodeId);
    id_ = id;
  }

  int num_temporaries_needed() const {
#ifdef DEBUG
    if (kTemporariesState == kUnset) {
      DCHECK_EQ(num_temporaries_needed_, 0);
    } else {
      DCHECK_EQ(kTemporariesState, kNeedsTemporaries);
    }
#endif  // DEBUG
    return num_temporaries_needed_;
  }

  RegList temporaries() const {
    DCHECK_EQ(kTemporariesState, kHasTemporaries);
    return temporaries_;
  }

  void assign_temporaries(RegList list) {
#ifdef DEBUG
    if (kTemporariesState == kUnset) {
      DCHECK_EQ(num_temporaries_needed_, 0);
    } else {
      DCHECK_EQ(kTemporariesState, kNeedsTemporaries);
    }
    kTemporariesState = kHasTemporaries;
#endif  // DEBUG
    temporaries_ = list;
  }

  void Print(std::ostream& os, MaglevGraphLabeller*) const;

  EagerDeoptInfo* eager_deopt_info() {
    DCHECK(properties().can_eager_deopt());
    DCHECK(!properties().can_lazy_deopt());
    return (
        reinterpret_cast<EagerDeoptInfo*>(input_address(input_count() - 1)) -
        1);
  }

  const EagerDeoptInfo* eager_deopt_info() const {
    DCHECK(properties().can_eager_deopt());
    DCHECK(!properties().can_lazy_deopt());
    return (reinterpret_cast<const EagerDeoptInfo*>(
                input_address(input_count() - 1)) -
            1);
  }

  LazyDeoptInfo* lazy_deopt_info() {
    DCHECK(properties().can_lazy_deopt());
    DCHECK(!properties().can_eager_deopt());
    return (reinterpret_cast<LazyDeoptInfo*>(input_address(input_count() - 1)) -
            1);
  }

  const LazyDeoptInfo* lazy_deopt_info() const {
    DCHECK(properties().can_lazy_deopt());
    DCHECK(!properties().can_eager_deopt());
    return (reinterpret_cast<const LazyDeoptInfo*>(
                input_address(input_count() - 1)) -
            1);
  }

 protected:
  explicit NodeBase(uint32_t bitfield) : bit_field_(bitfield) {}

  Input* input_address(int index) {
    DCHECK_LT(index, input_count());
    return reinterpret_cast<Input*>(this) - (index + 1);
  }

  const Input* input_address(int index) const {
    DCHECK_LT(index, input_count());
    return reinterpret_cast<const Input*>(this) - (index + 1);
  }

  void set_input(int index, ValueNode* input) {
    new (input_address(index)) Input(input);
  }

  void set_temporaries_needed(int value) {
#ifdef DEBUG
    DCHECK_EQ(kTemporariesState, kUnset);
    kTemporariesState = kNeedsTemporaries;
#endif  // DEBUG
    num_temporaries_needed_ = value;
  }

  EagerDeoptInfo* eager_deopt_info_address() {
    DCHECK(properties().can_eager_deopt());
    DCHECK(!properties().can_lazy_deopt());
    return reinterpret_cast<EagerDeoptInfo*>(input_address(input_count() - 1)) -
           1;
  }

  LazyDeoptInfo* lazy_deopt_info_address() {
    DCHECK(!properties().can_eager_deopt());
    DCHECK(properties().can_lazy_deopt());
    return reinterpret_cast<LazyDeoptInfo*>(input_address(input_count() - 1)) -
           1;
  }

 private:
  template <class Derived, typename... Args>
  static Derived* Allocate(Zone* zone, size_t input_count, Args&&... args) {
    static_assert(
        !Derived::kProperties.can_eager_deopt() ||
            !Derived::kProperties.can_lazy_deopt(),
        "The current deopt info representation, at the end of inputs, requires "
        "that we cannot have both lazy and eager deopts on a node. If we ever "
        "need this, we have to update accessors to check node->properties() "
        "for which deopts are active.");
    const size_t size_before_node =
        input_count * sizeof(Input) +
        (Derived::kProperties.can_eager_deopt() ? sizeof(EagerDeoptInfo) : 0) +
        (Derived::kProperties.can_lazy_deopt() ? sizeof(LazyDeoptInfo) : 0);
    const size_t size = size_before_node + sizeof(Derived);
    intptr_t raw_buffer =
        reinterpret_cast<intptr_t>(zone->Allocate<NodeWithInlineInputs>(size));
    void* node_buffer = reinterpret_cast<void*>(raw_buffer + size_before_node);
    uint32_t bitfield = OpcodeField::encode(opcode_of<Derived>) |
                        OpPropertiesField::encode(Derived::kProperties) |
                        InputCountField::encode(input_count);
    Derived* node =
        new (node_buffer) Derived(bitfield, std::forward<Args>(args)...);
    return node;
  }

  uint32_t bit_field_;

 private:
  NodeIdT id_ = kInvalidNodeId;

  union {
    int num_temporaries_needed_ = 0;
    RegList temporaries_;
  };
#ifdef DEBUG
  enum {
    kUnset,
    kNeedsTemporaries,
    kHasTemporaries
  } kTemporariesState = kUnset;
#endif  // DEBUG

  NodeBase() = delete;
  NodeBase(const NodeBase&) = delete;
  NodeBase(NodeBase&&) = delete;
  NodeBase& operator=(const NodeBase&) = delete;
  NodeBase& operator=(NodeBase&&) = delete;
};

template <class T>
constexpr bool NodeBase::Is() const {
  return opcode() == opcode_of<T>;
}

// Specialized sub-hierarchy type checks.
template <>
constexpr bool NodeBase::Is<ValueNode>() const {
  return IsValueNode(opcode());
}
template <>
constexpr bool NodeBase::Is<ConditionalControlNode>() const {
  return IsConditionalControlNode(opcode());
}
template <>
constexpr bool NodeBase::Is<UnconditionalControlNode>() const {
  return IsUnconditionalControlNode(opcode());
}

// The Node class hierarchy contains all non-control nodes.
class Node : public NodeBase {
 public:
  using List = base::ThreadedList<Node>;

  inline ValueLocation& result();

  // This might break ThreadedList invariants.
  // Run ThreadedList::RevalidateTail afterwards.
  void AddNodeAfter(Node* node) {
    DCHECK_NOT_NULL(node);
    DCHECK_NULL(node->next_);
    node->next_ = next_;
    next_ = node;
  }

  Node* NextNode() const { return next_; }

 protected:
  using NodeBase::NodeBase;

 private:
  Node** next() { return &next_; }
  Node* next_ = nullptr;

  friend List;
  friend base::ThreadedListTraits<Node>;
};

// All non-control nodes with a result.
class ValueNode : public Node {
 public:
  ValueLocation& result() { return result_; }
  const ValueLocation& result() const { return result_; }

  const compiler::InstructionOperand& hint() const {
    DCHECK_EQ(state_, kSpillOrHint);
    DCHECK(spill_or_hint_.IsInvalid() || spill_or_hint_.IsUnallocated());
    return spill_or_hint_;
  }

  void SetNoSpillOrHint() {
    DCHECK_EQ(state_, kLastUse);
#ifdef DEBUG
    state_ = kSpillOrHint;
#endif  // DEBUG
    spill_or_hint_ = compiler::InstructionOperand();
  }

  bool is_spilled() const {
    DCHECK_EQ(state_, kSpillOrHint);
    return spill_or_hint_.IsStackSlot();
  }

  void Spill(compiler::AllocatedOperand operand) {
#ifdef DEBUG
    if (state_ == kLastUse) {
      state_ = kSpillOrHint;
    } else {
      DCHECK(!is_spilled());
    }
#endif  // DEBUG
    DCHECK(operand.IsAnyStackSlot());
    spill_or_hint_ = operand;
  }

  compiler::AllocatedOperand spill_slot() const {
    DCHECK_EQ(state_, kSpillOrHint);
    DCHECK(is_spilled());
    return compiler::AllocatedOperand::cast(spill_or_hint_);
  }

  void mark_use(NodeIdT id, InputLocation* input_location) {
    DCHECK_EQ(state_, kLastUse);
    DCHECK_NE(id, kInvalidNodeId);
    DCHECK_LT(start_id(), id);
    DCHECK_IMPLIES(has_valid_live_range(), id >= end_id_);
    end_id_ = id;
    *last_uses_next_use_id_ = id;
    last_uses_next_use_id_ = input_location->get_next_use_id_address();
  }

  struct LiveRange {
    NodeIdT start = kInvalidNodeId;
    NodeIdT end = kInvalidNodeId;
  };

  bool has_valid_live_range() const { return end_id_ != 0; }
  LiveRange live_range() const { return {start_id(), end_id_}; }
  NodeIdT next_use() const { return next_use_; }

  // The following metods should only be used during register allocation, to
  // mark the _current_ state of this Node according to the register allocator.
  void set_next_use(NodeIdT use) { next_use_ = use; }

  // A node is dead once it has no more upcoming uses.
  bool is_dead() const { return next_use_ == kInvalidNodeId; }

  void AddRegister(Register reg) { registers_with_result_.set(reg); }
  void RemoveRegister(Register reg) { registers_with_result_.clear(reg); }
  RegList ClearRegisters() {
    return std::exchange(registers_with_result_, kEmptyRegList);
  }

  int num_registers() const { return registers_with_result_.Count(); }
  bool has_register() const { return registers_with_result_ != kEmptyRegList; }

  compiler::AllocatedOperand allocation() const {
    if (has_register()) {
      return compiler::AllocatedOperand(compiler::LocationOperand::REGISTER,
                                        MachineRepresentation::kTagged,
                                        registers_with_result_.first().code());
    }
    DCHECK(is_spilled());
    return compiler::AllocatedOperand::cast(spill_or_hint_);
  }

  bool is_untagged_value() const { return properties().is_untagged_value(); }

  ValueRepresentation value_representation() const {
    return is_untagged_value() ? ValueRepresentation::kUntagged
                               : ValueRepresentation::kTagged;
  }

 protected:
  explicit ValueNode(uint32_t bitfield)
      : Node(bitfield),
        last_uses_next_use_id_(&next_use_)
#ifdef DEBUG
        ,
        state_(kLastUse)
#endif  // DEBUG
  {
  }

  // Rename for better pairing with `end_id`.
  NodeIdT start_id() const { return id(); }

  NodeIdT end_id_ = kInvalidNodeId;
  NodeIdT next_use_ = kInvalidNodeId;
  ValueLocation result_;
  RegList registers_with_result_ = kEmptyRegList;
  union {
    // Pointer to the current last use's next_use_id field. Most of the time
    // this will be a pointer to an Input's next_use_id_ field, but it's
    // initialized to this node's next_use_ to track the first use.
    NodeIdT* last_uses_next_use_id_;
    compiler::InstructionOperand spill_or_hint_;
  };
#ifdef DEBUG
  // TODO(leszeks): Consider spilling into kSpill and kHint.
  enum { kLastUse, kSpillOrHint } state_;
#endif  // DEBUG
};

ValueLocation& Node::result() {
  DCHECK(Is<ValueNode>());
  return Cast<ValueNode>()->result();
}

template <class Derived>
class NodeT : public Node {
  STATIC_ASSERT(!IsValueNode(opcode_of<Derived>));

 public:
  constexpr Opcode opcode() const { return opcode_of<Derived>; }
  const OpProperties& properties() const { return Derived::kProperties; }

 protected:
  explicit NodeT(uint32_t bitfield) : Node(bitfield) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Derived>);
  }
};

template <size_t InputCount, class Derived>
class FixedInputNodeT : public NodeT<Derived> {
  static constexpr size_t kInputCount = InputCount;

 public:
  // Shadowing for static knowledge.
  constexpr bool has_inputs() const { return input_count() > 0; }
  constexpr uint16_t input_count() const { return kInputCount; }
  auto end() {
    return std::make_reverse_iterator(this->input_address(input_count() - 1));
  }

 protected:
  explicit FixedInputNodeT(uint32_t bitfield) : NodeT<Derived>(bitfield) {
    DCHECK_EQ(NodeBase::input_count(), kInputCount);
  }
};

template <class Derived>
class ValueNodeT : public ValueNode {
  STATIC_ASSERT(IsValueNode(opcode_of<Derived>));

 public:
  constexpr Opcode opcode() const { return opcode_of<Derived>; }
  const OpProperties& properties() const { return Derived::kProperties; }

 protected:
  explicit ValueNodeT(uint32_t bitfield) : ValueNode(bitfield) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Derived>);
  }
};

template <size_t InputCount, class Derived>
class FixedInputValueNodeT : public ValueNodeT<Derived> {
  static constexpr size_t kInputCount = InputCount;

 public:
  // Shadowing for static knowledge.
  constexpr bool has_inputs() const { return input_count() > 0; }
  constexpr uint16_t input_count() const { return kInputCount; }
  auto end() {
    return std::make_reverse_iterator(this->input_address(input_count() - 1));
  }

 protected:
  explicit FixedInputValueNodeT(uint32_t bitfield)
      : ValueNodeT<Derived>(bitfield) {
    DCHECK_EQ(NodeBase::input_count(), kInputCount);
  }
};

template <class Derived, Operation kOperation>
class UnaryWithFeedbackNode : public FixedInputValueNodeT<1, Derived> {
  using Base = FixedInputValueNodeT<1, Derived>;

 public:
  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();

  static constexpr int kOperandIndex = 0;
  Input& operand_input() { return Node::input(kOperandIndex); }
  compiler::FeedbackSource feedback() const { return feedback_; }

 protected:
  explicit UnaryWithFeedbackNode(uint32_t bitfield,
                                 const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

  const compiler::FeedbackSource feedback_;
};

template <class Derived, Operation kOperation>
class BinaryWithFeedbackNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }
  compiler::FeedbackSource feedback() const { return feedback_; }

 protected:
  BinaryWithFeedbackNode(uint32_t bitfield,
                         const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

  const compiler::FeedbackSource feedback_;
};

#define DEF_OPERATION_NODE(Name, Super, OpName)                            \
  class Name : public Super<Name, Operation::k##OpName> {                  \
    using Base = Super<Name, Operation::k##OpName>;                        \
                                                                           \
   public:                                                                 \
    Name(uint32_t bitfield, const compiler::FeedbackSource& feedback)      \
        : Base(bitfield, feedback) {}                                      \
    void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&); \
    void GenerateCode(MaglevCodeGenState*, const ProcessingState&);        \
    void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}         \
  };

#define DEF_UNARY_WITH_FEEDBACK_NODE(Name) \
  DEF_OPERATION_NODE(Generic##Name, UnaryWithFeedbackNode, Name)
#define DEF_BINARY_WITH_FEEDBACK_NODE(Name) \
  DEF_OPERATION_NODE(Generic##Name, BinaryWithFeedbackNode, Name)
UNARY_OPERATION_LIST(DEF_UNARY_WITH_FEEDBACK_NODE)
ARITHMETIC_OPERATION_LIST(DEF_BINARY_WITH_FEEDBACK_NODE)
COMPARISON_OPERATION_LIST(DEF_BINARY_WITH_FEEDBACK_NODE)
#undef DEF_UNARY_WITH_FEEDBACK_NODE
#undef DEF_BINARY_WITH_FEEDBACK_NODE

class CheckedSmiTag : public FixedInputValueNodeT<1, CheckedSmiTag> {
  using Base = FixedInputValueNodeT<1, CheckedSmiTag>;

 public:
  explicit CheckedSmiTag(uint32_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();

  Input& input() { return Node::input(0); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class CheckedSmiUntag : public FixedInputValueNodeT<1, CheckedSmiUntag> {
  using Base = FixedInputValueNodeT<1, CheckedSmiUntag>;

 public:
  explicit CheckedSmiUntag(uint32_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::UntaggedValue();

  Input& input() { return Node::input(0); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Int32Constant : public FixedInputValueNodeT<0, Int32Constant> {
  using Base = FixedInputValueNodeT<0, Int32Constant>;

 public:
  explicit Int32Constant(uint32_t bitfield, int32_t value)
      : Base(bitfield), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::UntaggedValue();

  int32_t value() const { return value_; }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int32_t value_;
};

class Int32AddWithOverflow
    : public FixedInputValueNodeT<2, Int32AddWithOverflow> {
  using Base = FixedInputValueNodeT<2, Int32AddWithOverflow>;

 public:
  explicit Int32AddWithOverflow(uint32_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::UntaggedValue();

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return Node::input(kLeftIndex); }
  Input& right_input() { return Node::input(kRightIndex); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class InitialValue : public FixedInputValueNodeT<0, InitialValue> {
  using Base = FixedInputValueNodeT<0, InitialValue>;

 public:
  explicit InitialValue(uint32_t bitfield, interpreter::Register source)
      : Base(bitfield), source_(source) {}

  interpreter::Register source() const { return source_; }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const interpreter::Register source_;
};

class RegisterInput : public FixedInputValueNodeT<0, RegisterInput> {
  using Base = FixedInputValueNodeT<0, RegisterInput>;

 public:
  explicit RegisterInput(uint32_t bitfield, Register input)
      : Base(bitfield), input_(input) {}

  Register input() const { return input_; }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const Register input_;
};

class SmiConstant : public FixedInputValueNodeT<0, SmiConstant> {
  using Base = FixedInputValueNodeT<0, SmiConstant>;

 public:
  explicit SmiConstant(uint32_t bitfield, Smi value)
      : Base(bitfield), value_(value) {}

  Smi value() const { return value_; }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const Smi value_;
};

class Constant : public FixedInputValueNodeT<0, Constant> {
  using Base = FixedInputValueNodeT<0, Constant>;

 public:
  explicit Constant(uint32_t bitfield, const compiler::HeapObjectRef& object)
      : Base(bitfield), object_(object) {}

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::HeapObjectRef object_;
};

class RootConstant : public FixedInputValueNodeT<0, RootConstant> {
  using Base = FixedInputValueNodeT<0, RootConstant>;

 public:
  explicit RootConstant(uint32_t bitfield, RootIndex index)
      : Base(bitfield), index_(index) {}

  RootIndex index() const { return index_; }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const RootIndex index_;
};

class CheckMaps : public FixedInputNodeT<1, CheckMaps> {
  using Base = FixedInputNodeT<1, CheckMaps>;

 public:
  explicit CheckMaps(uint32_t bitfield, const compiler::MapRef& map)
      : Base(bitfield), map_(map) {}

  // TODO(verwaest): This just calls in deferred code, so probably we'll need to
  // mark that to generate stack maps. Mark as call so we at least clear the
  // registers since we currently don't properly spill either.
  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Call();

  compiler::MapRef map() const { return map_; }

  static constexpr int kActualMapIndex = 0;
  Input& actual_map_input() { return input(kActualMapIndex); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::MapRef map_;
};

class LoadField : public FixedInputValueNodeT<1, LoadField> {
  using Base = FixedInputValueNodeT<1, LoadField>;

 public:
  explicit LoadField(uint32_t bitfield, int handler)
      : Base(bitfield), handler_(handler) {}

  static constexpr OpProperties kProperties = OpProperties::Reading();

  int handler() const { return handler_; }

  static constexpr int kObjectIndex = 0;
  Input& object_input() { return input(kObjectIndex); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int handler_;
};

class StoreField : public FixedInputNodeT<2, StoreField> {
  using Base = FixedInputNodeT<2, StoreField>;

 public:
  explicit StoreField(uint32_t bitfield, int handler)
      : Base(bitfield), handler_(handler) {}

  static constexpr OpProperties kProperties = OpProperties::Writing();

  int handler() const { return handler_; }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input& object_input() { return input(kObjectIndex); }
  Input& value_input() { return input(kValueIndex); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const int handler_;
};

class LoadGlobal : public FixedInputValueNodeT<1, LoadGlobal> {
  using Base = FixedInputValueNodeT<1, LoadGlobal>;

 public:
  explicit LoadGlobal(uint32_t bitfield, const compiler::NameRef& name)
      : Base(bitfield), name_(name) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input& context() { return input(0); }
  const compiler::NameRef& name() const { return name_; }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::NameRef name_;
};

class LoadNamedGeneric : public FixedInputValueNodeT<2, LoadNamedGeneric> {
  using Base = FixedInputValueNodeT<2, LoadNamedGeneric>;

 public:
  explicit LoadNamedGeneric(uint32_t bitfield, const compiler::NameRef& name,
                            const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  Input& context() { return input(kContextIndex); }
  Input& object_input() { return input(kObjectIndex); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class GapMove : public FixedInputNodeT<0, GapMove> {
  using Base = FixedInputNodeT<0, GapMove>;

 public:
  GapMove(uint32_t bitfield, compiler::AllocatedOperand source,
          compiler::AllocatedOperand target)
      : Base(bitfield), source_(source), target_(target) {}

  compiler::AllocatedOperand source() const { return source_; }
  compiler::AllocatedOperand target() const { return target_; }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  compiler::AllocatedOperand source_;
  compiler::AllocatedOperand target_;
};

// TODO(verwaest): It may make more sense to buffer phis in merged_states until
// we set up the interpreter frame state for code generation. At that point we
// can generate correctly-sized phis.
class Phi : public ValueNodeT<Phi> {
  using Base = ValueNodeT<Phi>;

 public:
  using List = base::ThreadedList<Phi>;

  // TODO(jgruber): More intuitive constructors, if possible.
  Phi(uint32_t bitfield, interpreter::Register owner, int merge_offset)
      : Base(bitfield), owner_(owner), merge_offset_(merge_offset) {}

  interpreter::Register owner() const { return owner_; }
  int merge_offset() const { return merge_offset_; }

  using Node::set_input;

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void AllocateVregInPostProcess(MaglevVregAllocationState*);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const;

 private:
  Phi** next() { return &next_; }

  const interpreter::Register owner_;
  Phi* next_ = nullptr;
  const int merge_offset_;
  friend List;
  friend base::ThreadedListTraits<Phi>;
};

class Call : public ValueNodeT<Call> {
  using Base = ValueNodeT<Call>;

 public:
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kFixedInputCount = 2;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  Call(uint32_t bitfield, ConvertReceiverMode mode, ValueNode* function,
       ValueNode* context)
      : Base(bitfield), receiver_mode_(mode) {
    set_input(kFunctionIndex, function);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input& function() { return input(kFunctionIndex); }
  const Input& function() const { return input(kFunctionIndex); }
  Input& context() { return input(kContextIndex); }
  const Input& context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input& arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  ConvertReceiverMode receiver_mode_;
};

// Represents either a direct BasicBlock pointer, or an entry in a list of
// unresolved BasicBlockRefs which will be mutated (in place) at some point into
// direct BasicBlock pointers.
class BasicBlockRef {
  struct BasicBlockRefBuilder;

 public:
  BasicBlockRef() : next_ref_(nullptr) {
#ifdef DEBUG
    state_ = kRefList;
#endif
  }
  explicit BasicBlockRef(BasicBlock* block) : block_ptr_(block) {
#ifdef DEBUG
    state_ = kBlockPointer;
#endif
  }

  // Refs can't be copied or moved, since they are referenced by `this` pointer
  // in the ref list.
  BasicBlockRef(const BasicBlockRef&) = delete;
  BasicBlockRef(BasicBlockRef&&) = delete;
  BasicBlockRef& operator=(const BasicBlockRef&) = delete;
  BasicBlockRef& operator=(BasicBlockRef&&) = delete;

  // Construct a new ref-list mode BasicBlockRef and add it to the given ref
  // list.
  explicit BasicBlockRef(BasicBlockRef* ref_list_head) : BasicBlockRef() {
    BasicBlockRef* old_next_ptr = MoveToRefList(ref_list_head);
    USE(old_next_ptr);
    DCHECK_NULL(old_next_ptr);
  }

  // Change this ref to a direct basic block pointer, returning the old "next"
  // pointer of the current ref.
  BasicBlockRef* SetToBlockAndReturnNext(BasicBlock* block) {
    DCHECK_EQ(state_, kRefList);

    BasicBlockRef* old_next_ptr = next_ref_;
    block_ptr_ = block;
#ifdef DEBUG
    state_ = kBlockPointer;
#endif
    return old_next_ptr;
  }

  // Reset this ref list to null, returning the old ref list (i.e. the old
  // "next" pointer).
  BasicBlockRef* Reset() {
    DCHECK_EQ(state_, kRefList);

    BasicBlockRef* old_next_ptr = next_ref_;
    next_ref_ = nullptr;
    return old_next_ptr;
  }

  // Move this ref to the given ref list, returning the old "next" pointer of
  // the current ref.
  BasicBlockRef* MoveToRefList(BasicBlockRef* ref_list_head) {
    DCHECK_EQ(state_, kRefList);
    DCHECK_EQ(ref_list_head->state_, kRefList);

    BasicBlockRef* old_next_ptr = next_ref_;
    next_ref_ = ref_list_head->next_ref_;
    ref_list_head->next_ref_ = this;
    return old_next_ptr;
  }

  BasicBlock* block_ptr() const {
    DCHECK_EQ(state_, kBlockPointer);
    return block_ptr_;
  }

  BasicBlockRef* next_ref() const {
    DCHECK_EQ(state_, kRefList);
    return next_ref_;
  }

  bool has_ref() const {
    DCHECK_EQ(state_, kRefList);
    return next_ref_ != nullptr;
  }

 private:
  union {
    BasicBlock* block_ptr_;
    BasicBlockRef* next_ref_;
  };
#ifdef DEBUG
  enum { kBlockPointer, kRefList } state_;
#endif  // DEBUG
};

class ControlNode : public NodeBase {
 public:
  // A "hole" in control flow is a control node that unconditionally interrupts
  // linear control flow (either by jumping or by exiting).
  //
  // A "post-dominating" hole is a hole that is guaranteed to be be reached in
  // control flow after this node (i.e. it is a hole that is a post-dominator
  // of this node).
  ControlNode* next_post_dominating_hole() const {
    return next_post_dominating_hole_;
  }
  void set_next_post_dominating_hole(ControlNode* node) {
    DCHECK_IMPLIES(node != nullptr, node->Is<Jump>() || node->Is<Return>() ||
                                        node->Is<Deopt>() ||
                                        node->Is<JumpLoop>());
    next_post_dominating_hole_ = node;
  }

 protected:
  using NodeBase::NodeBase;

 private:
  ControlNode* next_post_dominating_hole_ = nullptr;
};

class UnconditionalControlNode : public ControlNode {
 public:
  BasicBlock* target() const { return target_.block_ptr(); }
  int predecessor_id() const { return predecessor_id_; }
  void set_predecessor_id(int id) { predecessor_id_ = id; }

 protected:
  explicit UnconditionalControlNode(uint32_t bitfield,
                                    BasicBlockRef* target_refs)
      : ControlNode(bitfield), target_(target_refs) {}
  explicit UnconditionalControlNode(uint32_t bitfield, BasicBlock* target)
      : ControlNode(bitfield), target_(target) {}

 private:
  const BasicBlockRef target_;
  int predecessor_id_ = 0;
};

template <class Derived>
class UnconditionalControlNodeT : public UnconditionalControlNode {
  STATIC_ASSERT(IsUnconditionalControlNode(opcode_of<Derived>));
  static constexpr size_t kInputCount = 0;

 public:
  // Shadowing for static knowledge.
  constexpr Opcode opcode() const { return NodeBase::opcode_of<Derived>; }
  constexpr bool has_inputs() const { return input_count() > 0; }
  constexpr uint16_t input_count() const { return kInputCount; }
  auto end() {
    return std::make_reverse_iterator(input_address(input_count() - 1));
  }

 protected:
  explicit UnconditionalControlNodeT(uint32_t bitfield,
                                     BasicBlockRef* target_refs)
      : UnconditionalControlNode(bitfield, target_refs) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Derived>);
    DCHECK_EQ(NodeBase::input_count(), kInputCount);
  }
  explicit UnconditionalControlNodeT(uint32_t bitfield, BasicBlock* target)
      : UnconditionalControlNode(bitfield, target) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Derived>);
    DCHECK_EQ(NodeBase::input_count(), kInputCount);
  }
};

class ConditionalControlNode : public ControlNode {
 public:
  ConditionalControlNode(uint32_t bitfield, BasicBlockRef* if_true_refs,
                         BasicBlockRef* if_false_refs)
      : ControlNode(bitfield),
        if_true_(if_true_refs),
        if_false_(if_false_refs) {}

  BasicBlock* if_true() const { return if_true_.block_ptr(); }
  BasicBlock* if_false() const { return if_false_.block_ptr(); }

 private:
  BasicBlockRef if_true_;
  BasicBlockRef if_false_;
};

template <size_t InputCount, class Derived>
class ConditionalControlNodeT : public ConditionalControlNode {
  STATIC_ASSERT(IsConditionalControlNode(opcode_of<Derived>));
  static constexpr size_t kInputCount = InputCount;

 public:
  // Shadowing for static knowledge.
  constexpr Opcode opcode() const { return NodeBase::opcode_of<Derived>; }
  constexpr bool has_inputs() const { return input_count() > 0; }
  constexpr uint16_t input_count() const { return kInputCount; }
  auto end() {
    return std::make_reverse_iterator(input_address(input_count() - 1));
  }

 protected:
  explicit ConditionalControlNodeT(uint32_t bitfield,
                                   BasicBlockRef* if_true_refs,
                                   BasicBlockRef* if_false_refs)
      : ConditionalControlNode(bitfield, if_true_refs, if_false_refs) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Derived>);
    DCHECK_EQ(NodeBase::input_count(), kInputCount);
  }
};

class Jump : public UnconditionalControlNodeT<Jump> {
  using Base = UnconditionalControlNodeT<Jump>;

 public:
  explicit Jump(uint32_t bitfield, BasicBlockRef* target_refs)
      : Base(bitfield, target_refs) {}

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class JumpLoop : public UnconditionalControlNodeT<JumpLoop> {
  using Base = UnconditionalControlNodeT<JumpLoop>;

 public:
  explicit JumpLoop(uint32_t bitfield, BasicBlock* target)
      : Base(bitfield, target) {}

  explicit JumpLoop(uint32_t bitfield, BasicBlockRef* ref)
      : Base(bitfield, ref) {}

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Return : public ControlNode {
 public:
  explicit Return(uint32_t bitfield) : ControlNode(bitfield) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Return>);
  }

  Input& value_input() { return input(0); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class Deopt : public ControlNode {
 public:
  explicit Deopt(uint32_t bitfield) : ControlNode(bitfield) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Deopt>);
  }

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class BranchIfTrue : public ConditionalControlNodeT<1, BranchIfTrue> {
  using Base = ConditionalControlNodeT<1, BranchIfTrue>;

 public:
  explicit BranchIfTrue(uint32_t bitfield, BasicBlockRef* if_true_refs,
                        BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  Input& condition_input() { return input(0); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class BranchIfToBooleanTrue
    : public ConditionalControlNodeT<1, BranchIfToBooleanTrue> {
  using Base = ConditionalControlNodeT<1, BranchIfToBooleanTrue>;

 public:
  explicit BranchIfToBooleanTrue(uint32_t bitfield, BasicBlockRef* if_true_refs,
                                 BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr OpProperties kProperties = OpProperties::Call();

  Input& condition_input() { return input(0); }

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}
};

class BranchIfCompare
    : public ConditionalControlNodeT<2, BranchIfToBooleanTrue> {
  using Base = ConditionalControlNodeT<2, BranchIfToBooleanTrue>;

 public:
  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input& left_input() { return NodeBase::input(kLeftIndex); }
  Input& right_input() { return NodeBase::input(kRightIndex); }

  explicit BranchIfCompare(uint32_t bitfield, Operation operation,
                           BasicBlockRef* if_true_refs,
                           BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), operation_(operation) {}

  void AllocateVreg(MaglevVregAllocationState*, const ProcessingState&);
  void GenerateCode(MaglevCodeGenState*, const ProcessingState&);
  void PrintParams(std::ostream&, MaglevGraphLabeller*) const {}

 private:
  Operation operation_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_IR_H_
