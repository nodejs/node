// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_OPERATIONS_H_
#define V8_COMPILER_TURBOSHAFT_OPERATIONS_H_

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/small-vector.h"
#include "src/base/template-utils.h"
#include "src/base/vector.h"
#include "src/codegen/external-reference.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/globals.h"
#include "src/compiler/turboshaft/fast-hash.h"
#include "src/compiler/turboshaft/utils.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/zone/zone.h"

namespace v8::internal {
class HeapObject;
}  // namespace v8::internal
namespace v8::internal::compiler {
class CallDescriptor;
class DeoptimizeParameters;
class FrameStateInfo;
class Node;
}  // namespace v8::internal::compiler
namespace v8::internal::compiler::turboshaft {
class Block;
struct FrameStateData;
class Variable;
class Graph;

// DEFINING NEW OPERATIONS
// =======================
// For each operation `Foo`, we define:
// - an entry V(Foo) in TURBOSHAFT_OPERATION_LIST, which defines `Opcode::kFoo`.
// - a `struct FooOp`
// The struct derives from `OperationT<Foo>` or `FixedArityOperationT<k, Foo>`
// Furthermore, it has to contain:
// - A bunch of options directly as public fields.
// - A getter `options()` returning a tuple of all these options. This is used
//   for default printing and hashing. Alternatively, `void
//   PrintOptions(std::ostream& os) const` and `size_t hash_value() const` can
//   also be defined manually.
// - Getters for named inputs.
// - A constructor that first takes all the inputs and then all the options. For
//   a variable arity operation where the constructor doesn't take the inputs as
//   a single base::Vector<OpIndex> argument, it's also necessary to overwrite
//   the static `New` function, see `CallOp` for an example.

#define TURBOSHAFT_OPERATION_LIST(V) \
  V(WordBinop)                       \
  V(FloatBinop)                      \
  V(OverflowCheckedBinop)            \
  V(WordUnary)                       \
  V(FloatUnary)                      \
  V(Shift)                           \
  V(Equal)                           \
  V(Comparison)                      \
  V(Change)                          \
  V(Float64InsertWord32)             \
  V(TaggedBitcast)                   \
  V(PendingLoopPhi)                  \
  V(Constant)                        \
  V(Load)                            \
  V(IndexedLoad)                     \
  V(Store)                           \
  V(IndexedStore)                    \
  V(Retain)                          \
  V(Parameter)                       \
  V(OsrValue)                        \
  V(Goto)                            \
  V(StackPointerGreaterThan)         \
  V(StackSlot)                       \
  V(FrameConstant)                   \
  V(CheckLazyDeopt)                  \
  V(Deoptimize)                      \
  V(DeoptimizeIf)                    \
  V(Phi)                             \
  V(FrameState)                      \
  V(Call)                            \
  V(Unreachable)                     \
  V(Return)                          \
  V(Branch)                          \
  V(CatchException)                  \
  V(Switch)                          \
  V(Tuple)                           \
  V(Projection)

enum class Opcode : uint8_t {
#define ENUM_CONSTANT(Name) k##Name,
  TURBOSHAFT_OPERATION_LIST(ENUM_CONSTANT)
#undef ENUM_CONSTANT
};

const char* OpcodeName(Opcode opcode);
constexpr std::underlying_type_t<Opcode> OpcodeIndex(Opcode x) {
  return static_cast<std::underlying_type_t<Opcode>>(x);
}

#define COUNT_OPCODES(Name) +1
constexpr uint16_t kNumberOfOpcodes =
    0 TURBOSHAFT_OPERATION_LIST(COUNT_OPCODES);
#undef COUNT_OPCODES

// Operations are stored in possibly muliple sequential storage slots.
using OperationStorageSlot = std::aligned_storage_t<8, 8>;
// Operations occupy at least 2 slots, therefore we assign one id per two slots.
constexpr size_t kSlotsPerId = 2;

// `OpIndex` is an offset from the beginning of the operations buffer.
// Compared to `Operation*`, it is more memory efficient (32bit) and stable when
// the operations buffer is re-allocated.
class OpIndex {
 public:
  explicit constexpr OpIndex(uint32_t offset) : offset_(offset) {
    DCHECK_EQ(offset % sizeof(OperationStorageSlot), 0);
  }
  constexpr OpIndex() : offset_(std::numeric_limits<uint32_t>::max()) {}

  uint32_t id() const {
    // Operations are stored at an offset that's a multiple of
    // `sizeof(OperationStorageSlot)`. In addition, an operation occupies at
    // least `kSlotsPerId` many `OperationSlot`s. Therefore, we can assign id's
    // by dividing by `kSlotsPerId`. A compact id space is important, because it
    // makes side-tables smaller.
    DCHECK_EQ(offset_ % sizeof(OperationStorageSlot), 0);
    return offset_ / sizeof(OperationStorageSlot) / kSlotsPerId;
  }
  uint32_t offset() const {
    DCHECK_EQ(offset_ % sizeof(OperationStorageSlot), 0);
    return offset_;
  }

  bool valid() const { return *this != Invalid(); }

  static constexpr OpIndex Invalid() { return OpIndex(); }

  // Encode a sea-of-nodes node id in the `OpIndex` type.
  // Only used for node origins that actually point to sea-of-nodes graph nodes.
  static OpIndex EncodeTurbofanNodeId(uint32_t id) {
    OpIndex result = OpIndex(id * sizeof(OperationStorageSlot));
    result.offset_ += kTurbofanNodeIdFlag;
    return result;
  }
  uint32_t DecodeTurbofanNodeId() const {
    DCHECK(IsTurbofanNodeId());
    return offset_ / sizeof(OperationStorageSlot);
  }
  bool IsTurbofanNodeId() const {
    return offset_ % sizeof(OperationStorageSlot) == kTurbofanNodeIdFlag;
  }

  bool operator==(OpIndex other) const { return offset_ == other.offset_; }
  bool operator!=(OpIndex other) const { return offset_ != other.offset_; }
  bool operator<(OpIndex other) const { return offset_ < other.offset_; }
  bool operator>(OpIndex other) const { return offset_ > other.offset_; }
  bool operator<=(OpIndex other) const { return offset_ <= other.offset_; }
  bool operator>=(OpIndex other) const { return offset_ >= other.offset_; }

 private:
  uint32_t offset_;

  static constexpr uint32_t kTurbofanNodeIdFlag = 1;
};

template <>
struct fast_hash<OpIndex> {
  V8_INLINE size_t operator()(OpIndex op) { return op.id(); }
};

// `BlockIndex` is the index of a bound block.
// A dominating block always has a smaller index.
// It corresponds to the ordering of basic blocks in the operations buffer.
class BlockIndex {
 public:
  explicit constexpr BlockIndex(uint32_t id) : id_(id) {}
  constexpr BlockIndex() : id_(std::numeric_limits<uint32_t>::max()) {}

  uint32_t id() const { return id_; }
  bool valid() const { return *this != Invalid(); }

  static constexpr BlockIndex Invalid() { return BlockIndex(); }

  bool operator==(BlockIndex other) const { return id_ == other.id_; }
  bool operator!=(BlockIndex other) const { return id_ != other.id_; }
  bool operator<(BlockIndex other) const { return id_ < other.id_; }
  bool operator>(BlockIndex other) const { return id_ > other.id_; }
  bool operator<=(BlockIndex other) const { return id_ <= other.id_; }
  bool operator>=(BlockIndex other) const { return id_ >= other.id_; }

 private:
  uint32_t id_;
};

template <>
struct fast_hash<BlockIndex> {
  V8_INLINE size_t operator()(BlockIndex op) { return op.id(); }
};

std::ostream& operator<<(std::ostream& os, BlockIndex b);
std::ostream& operator<<(std::ostream& os, const Block* b);

struct OpProperties {
  // The operation may read memory or depend on other information beyond its
  // inputs.
  const bool can_read;
  // The operation may write memory or have other observable side-effects.
  const bool can_write;
  // The operation can abort the current execution by throwing an exception or
  // deoptimizing.
  const bool can_abort;
  // The last operation in a basic block.
  const bool is_block_terminator;
  // By being const and not being set in the constructor, these properties are
  // guaranteed to be derived.
  const bool is_pure =
      !(can_read || can_write || can_abort || is_block_terminator);
  const bool is_required_when_unused =
      can_write || can_abort || is_block_terminator;
  // Nodes that don't read, write and aren't block terminators can be eliminated
  // via value numbering.
  const bool can_be_eliminated =
      !(can_read || can_write || is_block_terminator);

  constexpr OpProperties(bool can_read, bool can_write, bool can_abort,
                         bool is_block_terminator)
      : can_read(can_read),
        can_write(can_write),
        can_abort(can_abort),
        is_block_terminator(is_block_terminator) {}

  static constexpr OpProperties Pure() { return {false, false, false, false}; }
  static constexpr OpProperties Reading() {
    return {true, false, false, false};
  }
  static constexpr OpProperties Writing() {
    return {false, true, false, false};
  }
  static constexpr OpProperties CanDeopt() {
    return {false, false, true, false};
  }
  static constexpr OpProperties AnySideEffects() {
    return {true, true, true, false};
  }
  static constexpr OpProperties BlockTerminator() {
    return {false, false, false, true};
  }
  bool operator==(const OpProperties& other) const {
    return can_read == other.can_read && can_write == other.can_write &&
           can_abort == other.can_abort &&
           is_block_terminator == other.is_block_terminator;
  }
};
std::ostream& operator<<(std::ostream& os, OpProperties opProperties);

// Baseclass for all Turboshaft operations.
// The `alignas(OpIndex)` is necessary because it is followed by an array of
// `OpIndex` inputs.
struct alignas(OpIndex) Operation {
  const Opcode opcode;

  // The number of uses of this operation in the current graph.
  // Instead of overflowing, we saturate the value if it reaches the maximum. In
  // this case, the true number of uses is unknown.
  // We use such a small type to save memory and because nodes with a high
  // number of uses are rare. Additionally, we usually only care if the number
  // of uses is 0, 1 or bigger than 1.
  uint8_t saturated_use_count = 0;
  static constexpr uint8_t kUnknownUseCount =
      std::numeric_limits<uint8_t>::max();

  const uint16_t input_count;

  // The inputs are stored adjacent in memory, right behind the `Operation`
  // object.
  base::Vector<const OpIndex> inputs() const;
  V8_INLINE OpIndex input(size_t i) const { return inputs()[i]; }

  static size_t StorageSlotCount(Opcode opcode, size_t input_count);
  size_t StorageSlotCount() const {
    return StorageSlotCount(opcode, input_count);
  }

  template <class Op>
  bool Is() const {
    return opcode == Op::opcode;
  }
  template <class Op>
  Op& Cast() {
    DCHECK(Is<Op>());
    return *static_cast<Op*>(this);
  }
  template <class Op>
  const Op& Cast() const {
    DCHECK(Is<Op>());
    return *static_cast<const Op*>(this);
  }
  template <class Op>
  const Op* TryCast() const {
    if (!Is<Op>()) return nullptr;
    return static_cast<const Op*>(this);
  }
  template <class Op>
  Op* TryCast() {
    if (!Is<Op>()) return nullptr;
    return static_cast<Op*>(this);
  }
  const OpProperties& properties() const;

  std::string ToString() const;
  void PrintOptions(std::ostream& os) const;

 protected:
  // Operation objects store their inputs behind the object. Therefore, they can
  // only be constructed as part of a Graph.
  explicit Operation(Opcode opcode, size_t input_count)
      : opcode(opcode), input_count(input_count) {
    DCHECK_LE(input_count,
              std::numeric_limits<decltype(this->input_count)>::max());
  }

  Operation(const Operation&) = delete;
  Operation& operator=(const Operation&) = delete;
};

struct OperationPrintStyle {
  const Operation& op;
  const char* op_index_prefix = "#";
};

std::ostream& operator<<(std::ostream& os, OperationPrintStyle op);
inline std::ostream& operator<<(std::ostream& os, const Operation& op) {
  return os << OperationPrintStyle{op};
}
inline void Print(const Operation& op) { std::cout << op << "\n"; }

OperationStorageSlot* AllocateOpStorage(Graph* graph, size_t slot_count);

// This template knows the complete type of the operation and is plugged into
// the inheritance hierarchy. It removes boilerplate from the concrete
// `Operation` subclasses, defining everything that can be expressed
// generically. It overshadows many methods from `Operation` with ones that
// exploit additional static information.
template <class Derived>
struct OperationT : Operation {
  // Enable concise base-constructor call in derived struct.
  using Base = OperationT;

  static const Opcode opcode;

  static constexpr OpProperties properties() { return Derived::properties; }

  Derived& derived_this() { return *static_cast<Derived*>(this); }
  const Derived& derived_this() const {
    return *static_cast<const Derived*>(this);
  }

  // Shadow Operation::inputs to exploit static knowledge about object size.
  base::Vector<OpIndex> inputs() {
    return {reinterpret_cast<OpIndex*>(reinterpret_cast<char*>(this) +
                                       sizeof(Derived)),
            derived_this().input_count};
  }
  base::Vector<const OpIndex> inputs() const {
    return {reinterpret_cast<const OpIndex*>(
                reinterpret_cast<const char*>(this) + sizeof(Derived)),
            derived_this().input_count};
  }

  V8_INLINE OpIndex& input(size_t i) { return derived_this().inputs()[i]; }
  V8_INLINE OpIndex input(size_t i) const { return derived_this().inputs()[i]; }

  static size_t StorageSlotCount(size_t input_count) {
    // The operation size in bytes is:
    //   `sizeof(Derived) + input_count*sizeof(OpIndex)`.
    // This is an optimized computation of:
    //   round_up(size_in_bytes / sizeof(StorageSlot))
    constexpr size_t r = sizeof(OperationStorageSlot) / sizeof(OpIndex);
    static_assert(sizeof(OperationStorageSlot) % sizeof(OpIndex) == 0);
    static_assert(sizeof(Derived) % sizeof(OpIndex) == 0);
    size_t result = std::max<size_t>(
        2, (r - 1 + sizeof(Derived) / sizeof(OpIndex) + input_count) / r);
    DCHECK_EQ(result, Operation::StorageSlotCount(opcode, input_count));
    return result;
  }
  size_t StorageSlotCount() const { return StorageSlotCount(input_count); }

  template <class... Args>
  static Derived& New(Graph* graph, size_t input_count, Args... args) {
    OperationStorageSlot* ptr =
        AllocateOpStorage(graph, StorageSlotCount(input_count));
    Derived* result = new (ptr) Derived(std::move(args)...);
    // If this DCHECK fails, then the number of inputs specified in the
    // operation constructor and in the static New function disagree.
    DCHECK_EQ(input_count, result->Operation::input_count);
    return *result;
  }

  template <class... Args>
  static Derived& New(Graph* graph, base::Vector<const OpIndex> inputs,
                      Args... args) {
    return New(graph, inputs.size(), inputs, args...);
  }

  explicit OperationT(size_t input_count) : Operation(opcode, input_count) {
    static_assert((std::is_base_of<OperationT, Derived>::value));
#if !V8_CC_MSVC
    static_assert(std::is_trivially_copyable<Derived>::value);
#endif  // !V8_CC_MSVC
    static_assert(std::is_trivially_destructible<Derived>::value);
  }
  explicit OperationT(base::Vector<const OpIndex> inputs)
      : OperationT(inputs.size()) {
    this->inputs().OverwriteWith(inputs);
  }

  bool operator==(const Base& other) const {
    return derived_this().inputs() == other.derived_this().inputs() &&
           derived_this().options() == other.derived_this().options();
  }
  size_t hash_value() const {
    return fast_hash_combine(opcode, derived_this().inputs(),
                             derived_this().options());
  }

  void PrintOptions(std::ostream& os) const {
    const auto& options = derived_this().options();
    constexpr size_t options_count =
        std::tuple_size<std::remove_reference_t<decltype(options)>>::value;
    if (options_count == 0) {
      return;
    }
    PrintOptionsHelper(os, options, std::make_index_sequence<options_count>());
  }

 private:
  template <class... T, size_t... I>
  static void PrintOptionsHelper(std::ostream& os,
                                 const std::tuple<T...>& options,
                                 std::index_sequence<I...>) {
    os << "[";
    bool first = true;
    USE(first);
    ((first ? (first = false, os << std::get<I>(options))
            : os << ", " << std::get<I>(options)),
     ...);
    os << "]";
  }
};

template <size_t InputCount, class Derived>
struct FixedArityOperationT : OperationT<Derived> {
  // Enable concise base access in derived struct.
  using Base = FixedArityOperationT;

  // Shadow Operation::input_count to exploit static knowledge.
  static constexpr uint16_t input_count = InputCount;

  template <class... Args>
  explicit FixedArityOperationT(Args... args)
      : OperationT<Derived>(InputCount) {
    static_assert(sizeof...(Args) == InputCount, "wrong number of inputs");
    size_t i = 0;
    OpIndex* inputs = this->inputs().begin();
    ((inputs[i++] = args), ...);
  }

  // Redefine the input initialization to tell C++ about the static input size.
  template <class... Args>
  static Derived& New(Graph* graph, Args... args) {
    Derived& result =
        OperationT<Derived>::New(graph, InputCount, std::move(args)...);
    return result;
  }
};

#define SUPPORTED_OPERATIONS_LIST(V)               \
  V(float32_round_down, Float32RoundDown)          \
  V(float64_round_down, Float64RoundDown)          \
  V(float32_round_up, Float32RoundUp)              \
  V(float64_round_up, Float64RoundUp)              \
  V(float32_round_to_zero, Float32RoundTruncate)   \
  V(float64_round_to_zero, Float64RoundTruncate)   \
  V(float32_round_ties_even, Float32RoundTiesEven) \
  V(float64_round_ties_even, Float64RoundTiesEven) \
  V(float64_round_ties_away, Float64RoundTiesAway) \
  V(int32_div_is_safe, Int32DivIsSafe)             \
  V(uint32_div_is_safe, Uint32DivIsSafe)           \
  V(word32_shift_is_safe, Word32ShiftIsSafe)       \
  V(word32_ctz, Word32Ctz)                         \
  V(word64_ctz, Word64Ctz)                         \
  V(word64_ctz_lowerable, Word64CtzLowerable)      \
  V(word32_popcnt, Word32Popcnt)                   \
  V(word64_popcnt, Word64Popcnt)                   \
  V(word32_reverse_bits, Word32ReverseBits)        \
  V(word64_reverse_bits, Word64ReverseBits)        \
  V(float32_select, Float32Select)                 \
  V(float64_select, Float64Select)                 \
  V(int32_abs_with_overflow, Int32AbsWithOverflow) \
  V(int64_abs_with_overflow, Int64AbsWithOverflow) \
  V(word32_rol, Word32Rol)                         \
  V(word64_rol, Word64Rol)                         \
  V(word64_rol_lowerable, Word64RolLowerable)      \
  V(sat_conversion_is_safe, SatConversionIsSafe)   \
  V(word32_select, Word32Select)                   \
  V(word64_select, Word64Select)

class SupportedOperations {
#define DECLARE_FIELD(name, machine_name) bool name##_;
#define DECLARE_GETTER(name, machine_name)     \
  static bool name() {                         \
    if constexpr (DEBUG_BOOL) {                \
      base::MutexGuard lock(mutex_.Pointer()); \
      DCHECK(initialized_);                    \
    }                                          \
    return instance_.name##_;                  \
  }

 public:
  static void Initialize();
  SUPPORTED_OPERATIONS_LIST(DECLARE_GETTER)

 private:
  SUPPORTED_OPERATIONS_LIST(DECLARE_FIELD)

  static bool initialized_;
  static base::LazyMutex mutex_;
  static SupportedOperations instance_;

#undef DECLARE_FIELD
#undef DECLARE_GETTER
};

struct WordBinopOp : FixedArityOperationT<2, WordBinopOp> {
  enum class Kind : uint8_t {
    kAdd,
    kMul,
    kSignedMulOverflownBits,
    kUnsignedMulOverflownBits,
    kBitwiseAnd,
    kBitwiseOr,
    kBitwiseXor,
    kSub,
    kSignedDiv,
    kUnsignedDiv,
    kSignedMod,
    kUnsignedMod,
  };
  Kind kind;
  MachineRepresentation rep;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex left() const { return input(0); }
  OpIndex right() const { return input(1); }

  static bool IsCommutative(Kind kind) {
    switch (kind) {
      case Kind::kAdd:
      case Kind::kMul:
      case Kind::kSignedMulOverflownBits:
      case Kind::kUnsignedMulOverflownBits:
      case Kind::kBitwiseAnd:
      case Kind::kBitwiseOr:
      case Kind::kBitwiseXor:
        return true;
      case Kind::kSub:
      case Kind::kSignedDiv:
      case Kind::kUnsignedDiv:
      case Kind::kSignedMod:
      case Kind::kUnsignedMod:
        return false;
    }
  }

  static bool IsAssociative(Kind kind, MachineRepresentation rep) {
    if (IsFloatingPoint(rep)) {
      return false;
    }
    switch (kind) {
      case Kind::kAdd:
      case Kind::kMul:
      case Kind::kBitwiseAnd:
      case Kind::kBitwiseOr:
      case Kind::kBitwiseXor:
        return true;
      case Kind::kSignedMulOverflownBits:
      case Kind::kUnsignedMulOverflownBits:
      case Kind::kSub:
      case Kind::kSignedDiv:
      case Kind::kUnsignedDiv:
      case Kind::kSignedMod:
      case Kind::kUnsignedMod:
        return false;
    }
  }
  // The Word32 and Word64 versions of the operator compute the same result when
  // truncated to 32 bit.
  static bool AllowsWord64ToWord32Truncation(Kind kind) {
    switch (kind) {
      case Kind::kAdd:
      case Kind::kMul:
      case Kind::kBitwiseAnd:
      case Kind::kBitwiseOr:
      case Kind::kBitwiseXor:
      case Kind::kSub:
        return true;
      case Kind::kSignedMulOverflownBits:
      case Kind::kUnsignedMulOverflownBits:
      case Kind::kSignedDiv:
      case Kind::kUnsignedDiv:
      case Kind::kSignedMod:
      case Kind::kUnsignedMod:
        return false;
    }
  }

  WordBinopOp(OpIndex left, OpIndex right, Kind kind, MachineRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {
    DCHECK_EQ(rep, any_of(MachineRepresentation::kWord32,
                          MachineRepresentation::kWord64));
    DCHECK_IMPLIES(kind == any_of(Kind::kSignedMulOverflownBits,
                                  Kind::kUnsignedMulOverflownBits),
                   rep == MachineRepresentation::kWord32);
  }
  auto options() const { return std::tuple{kind, rep}; }
  void PrintOptions(std::ostream& os) const;
};

struct FloatBinopOp : FixedArityOperationT<2, FloatBinopOp> {
  enum class Kind : uint8_t {
    kAdd,
    kMul,
    kMin,
    kMax,
    kSub,
    kDiv,
    kMod,
    kPower,
    kAtan2,
  };
  Kind kind;
  MachineRepresentation rep;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex left() const { return input(0); }
  OpIndex right() const { return input(1); }

  static bool IsCommutative(Kind kind) {
    switch (kind) {
      case Kind::kAdd:
      case Kind::kMul:
      case Kind::kMin:
      case Kind::kMax:
        return true;
      case Kind::kSub:
      case Kind::kDiv:
      case Kind::kMod:
      case Kind::kPower:
      case Kind::kAtan2:
        return false;
    }
  }

  FloatBinopOp(OpIndex left, OpIndex right, Kind kind,
               MachineRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {
    DCHECK_EQ(rep, any_of(MachineRepresentation::kFloat32,
                          MachineRepresentation::kFloat64));
    DCHECK_IMPLIES(kind == any_of(Kind::kPower, Kind::kAtan2, Kind::kMod),
                   rep == MachineRepresentation::kFloat64);
  }
  auto options() const { return std::tuple{kind, rep}; }
  void PrintOptions(std::ostream& os) const;
};

struct OverflowCheckedBinopOp
    : FixedArityOperationT<2, OverflowCheckedBinopOp> {
  enum class Kind : uint8_t {
    kSignedAdd,
    kSignedMul,
    kSignedSub,
  };
  Kind kind;
  MachineRepresentation rep;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex left() const { return input(0); }
  OpIndex right() const { return input(1); }

  static bool IsCommutative(Kind kind) {
    switch (kind) {
      case Kind::kSignedAdd:
      case Kind::kSignedMul:
        return true;
      case Kind::kSignedSub:
        return false;
    }
  }

  OverflowCheckedBinopOp(OpIndex left, OpIndex right, Kind kind,
                         MachineRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {
    DCHECK_EQ(rep, MachineRepresentation::kWord32);
  }
  auto options() const { return std::tuple{kind, rep}; }
  void PrintOptions(std::ostream& os) const;
};

struct WordUnaryOp : FixedArityOperationT<1, WordUnaryOp> {
  enum class Kind : uint8_t {
    kReverseBytes,
    kCountLeadingZeros,
  };
  Kind kind;
  MachineRepresentation rep;
  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex input() const { return Base::input(0); }

  explicit WordUnaryOp(OpIndex input, Kind kind, MachineRepresentation rep)
      : Base(input), kind(kind), rep(rep) {
    DCHECK_EQ(rep, any_of(MachineRepresentation::kWord32,
                          MachineRepresentation::kWord64));
  }
  auto options() const { return std::tuple{kind, rep}; }
};
std::ostream& operator<<(std::ostream& os, WordUnaryOp::Kind kind);

struct FloatUnaryOp : FixedArityOperationT<1, FloatUnaryOp> {
  enum class Kind : uint8_t {
    kAbs,
    kNegate,
    kSilenceNaN,
    kRoundDown,      // round towards -infinity
    kRoundUp,        // round towards +infinity
    kRoundToZero,    // round towards 0
    kRoundTiesEven,  // break ties by rounding towards the next even number
    kLog,
    kSqrt,
    kExp,
    kExpm1,
    kSin,
    kCos,
    kSinh,
    kCosh,
    kAcos,
    kAsin,
    kAsinh,
    kAcosh,
    kTan,
    kTanh,
  };
  Kind kind;
  MachineRepresentation rep;
  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex input() const { return Base::input(0); }

  static bool IsSupported(Kind kind, MachineRepresentation rep);

  explicit FloatUnaryOp(OpIndex input, Kind kind, MachineRepresentation rep)
      : Base(input), kind(kind), rep(rep) {
    DCHECK_EQ(rep, any_of(MachineRepresentation::kFloat32,
                          MachineRepresentation::kFloat64));
    DCHECK(IsSupported(kind, rep));
  }
  auto options() const { return std::tuple{kind, rep}; }
};
std::ostream& operator<<(std::ostream& os, FloatUnaryOp::Kind kind);

struct ShiftOp : FixedArityOperationT<2, ShiftOp> {
  enum class Kind : uint8_t {
    kShiftRightArithmeticShiftOutZeros,
    kShiftRightArithmetic,
    kShiftRightLogical,
    kShiftLeft,
    kRotateRight,
    kRotateLeft
  };
  Kind kind;
  MachineRepresentation rep;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex left() const { return input(0); }
  OpIndex right() const { return input(1); }

  static bool IsRightShift(Kind kind) {
    switch (kind) {
      case Kind::kShiftRightArithmeticShiftOutZeros:
      case Kind::kShiftRightArithmetic:
      case Kind::kShiftRightLogical:
        return true;
      case Kind::kShiftLeft:
      case Kind::kRotateRight:
      case Kind::kRotateLeft:
        return false;
    }
  }
  // The Word32 and Word64 versions of the operator compute the same result when
  // truncated to 32 bit.
  static bool AllowsWord64ToWord32Truncation(Kind kind) {
    switch (kind) {
      case Kind::kShiftLeft:
        return true;
      case Kind::kShiftRightArithmeticShiftOutZeros:
      case Kind::kShiftRightArithmetic:
      case Kind::kShiftRightLogical:
      case Kind::kRotateRight:
      case Kind::kRotateLeft:
        return false;
    }
  }

  ShiftOp(OpIndex left, OpIndex right, Kind kind, MachineRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {
    DCHECK_EQ(rep, any_of(MachineRepresentation::kWord32,
                          MachineRepresentation::kWord64));
  }
  auto options() const { return std::tuple{kind, rep}; }
};
std::ostream& operator<<(std::ostream& os, ShiftOp::Kind kind);

struct EqualOp : FixedArityOperationT<2, EqualOp> {
  MachineRepresentation rep;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex left() const { return input(0); }
  OpIndex right() const { return input(1); }

  EqualOp(OpIndex left, OpIndex right, MachineRepresentation rep)
      : Base(left, right), rep(rep) {
    DCHECK(rep == MachineRepresentation::kWord32 ||
           rep == MachineRepresentation::kWord64 ||
           rep == MachineRepresentation::kFloat32 ||
           rep == MachineRepresentation::kFloat64);
  }
  auto options() const { return std::tuple{rep}; }
};

struct ComparisonOp : FixedArityOperationT<2, ComparisonOp> {
  enum class Kind : uint8_t {
    kSignedLessThan,
    kSignedLessThanOrEqual,
    kUnsignedLessThan,
    kUnsignedLessThanOrEqual
  };
  Kind kind;
  MachineRepresentation rep;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex left() const { return input(0); }
  OpIndex right() const { return input(1); }

  ComparisonOp(OpIndex left, OpIndex right, Kind kind,
               MachineRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {
    DCHECK_EQ(rep, any_of(MachineRepresentation::kWord32,
                          MachineRepresentation::kWord64,
                          MachineRepresentation::kFloat32,
                          MachineRepresentation::kFloat64));
  }
  auto options() const { return std::tuple{kind, rep}; }
};
std::ostream& operator<<(std::ostream& os, ComparisonOp::Kind kind);

struct ChangeOp : FixedArityOperationT<1, ChangeOp> {
  enum class Kind : uint8_t {
    // narrowing means undefined behavior if value cannot be represented
    // precisely
    kSignedNarrowing,
    kUnsignedNarrowing,
    // convert between different floating-point types
    kFloatConversion,
    // conversion to signed integer, rounding towards zero,
    // overflow behavior system-specific
    kSignedFloatTruncate,
    // like kSignedFloatTruncate, but overflow guaranteed to result in the
    // minimal integer
    kSignedFloatTruncateOverflowToMin,
    // JS semantics float64 to word32 truncation
    // https://tc39.es/ecma262/#sec-touint32
    kJSFloatTruncate,
    // convert (un)signed integer to floating-point value
    kSignedToFloat,
    kUnsignedToFloat,
    // extract half of a float64 value
    kExtractHighHalf,
    kExtractLowHalf,
    // increase bit-width for unsigned integer values
    kZeroExtend,
    // increase bid-width for signed integer values
    kSignExtend,
    // preserve bits, change meaning
    kBitcast
  };
  Kind kind;
  MachineRepresentation from;
  MachineRepresentation to;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex input() const { return Base::input(0); }

  ChangeOp(OpIndex input, Kind kind, MachineRepresentation from,
           MachineRepresentation to)
      : Base(input), kind(kind), from(from), to(to) {}
  auto options() const { return std::tuple{kind, from, to}; }
};
std::ostream& operator<<(std::ostream& os, ChangeOp::Kind kind);

// TODO(tebbi): Unify with other operations.
struct Float64InsertWord32Op : FixedArityOperationT<2, Float64InsertWord32Op> {
  enum class Kind { kLowHalf, kHighHalf };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex float64() const { return input(0); }
  OpIndex word32() const { return input(1); }

  Float64InsertWord32Op(OpIndex float64, OpIndex word32, Kind kind)
      : Base(float64, word32), kind(kind) {}
  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, Float64InsertWord32Op::Kind kind);

struct TaggedBitcastOp : FixedArityOperationT<1, TaggedBitcastOp> {
  MachineRepresentation from;
  MachineRepresentation to;

  // Due to moving GC, converting from or to pointers doesn't commute with GC.
  static constexpr OpProperties properties = OpProperties::Reading();

  OpIndex input() const { return Base::input(0); }

  TaggedBitcastOp(OpIndex input, MachineRepresentation from,
                  MachineRepresentation to)
      : Base(input), from(from), to(to) {}
  auto options() const { return std::tuple{from, to}; }
};

struct PhiOp : OperationT<PhiOp> {
  MachineRepresentation rep;

  static constexpr OpProperties properties = OpProperties::Pure();

  static constexpr size_t kLoopPhiBackEdgeIndex = 1;

  explicit PhiOp(base::Vector<const OpIndex> inputs, MachineRepresentation rep)
      : Base(inputs), rep(rep) {}
  auto options() const { return std::tuple{rep}; }
};

// Only used when moving a loop phi to a new graph while the loop backedge has
// not been emitted yet.
struct PendingLoopPhiOp : FixedArityOperationT<1, PendingLoopPhiOp> {
  MachineRepresentation rep;
  union {
    // Used when transforming a Turboshaft graph.
    // This is not an input because it refers to the old graph.
    OpIndex old_backedge_index = OpIndex::Invalid();
    // Used when translating from sea-of-nodes.
    Node* old_backedge_node;
  };

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex first() const { return input(0); }

  PendingLoopPhiOp(OpIndex first, MachineRepresentation rep,
                   OpIndex old_backedge_index)
      : Base(first), rep(rep), old_backedge_index(old_backedge_index) {
    DCHECK(old_backedge_index.valid());
  }
  PendingLoopPhiOp(OpIndex first, MachineRepresentation rep,
                   Node* old_backedge_node)
      : Base(first), rep(rep), old_backedge_node(old_backedge_node) {}
  std::tuple<> options() const { UNREACHABLE(); }
  void PrintOptions(std::ostream& os) const;
};

struct ConstantOp : FixedArityOperationT<0, ConstantOp> {
  enum class Kind : uint8_t {
    kWord32,
    kWord64,
    kFloat32,
    kFloat64,
    kNumber,  // TODO(tebbi): See if we can avoid number constants.
    kTaggedIndex,
    kExternal,
    kHeapObject,
    kCompressedHeapObject
  };

  Kind kind;
  union Storage {
    uint64_t integral;
    float float32;
    double float64;
    ExternalReference external;
    Handle<HeapObject> handle;

    Storage(uint64_t integral = 0) : integral(integral) {}
    Storage(double constant) : float64(constant) {}
    Storage(float constant) : float32(constant) {}
    Storage(ExternalReference constant) : external(constant) {}
    Storage(Handle<HeapObject> constant) : handle(constant) {}
  } storage;

  static constexpr OpProperties properties = OpProperties::Pure();

  MachineRepresentation Representation() const {
    switch (kind) {
      case Kind::kWord32:
        return MachineRepresentation::kWord32;
      case Kind::kWord64:
        return MachineRepresentation::kWord64;
      case Kind::kFloat32:
        return MachineRepresentation::kFloat32;
      case Kind::kFloat64:
        return MachineRepresentation::kFloat64;
      case Kind::kExternal:
      case Kind::kTaggedIndex:
        return MachineType::PointerRepresentation();
      case Kind::kHeapObject:
      case Kind::kNumber:
        return MachineRepresentation::kTagged;
      case Kind::kCompressedHeapObject:
        return MachineRepresentation::kCompressed;
    }
  }

  ConstantOp(Kind kind, Storage storage)
      : Base(), kind(kind), storage(storage) {
    DCHECK_IMPLIES(
        kind == Kind::kWord32,
        storage.integral <= MaxUnsignedValue(MachineRepresentation::kWord32));
  }

  uint64_t integral() const {
    DCHECK(kind == Kind::kWord32 || kind == Kind::kWord64);
    return storage.integral;
  }

  int64_t signed_integral() const {
    switch (kind) {
      case Kind::kWord32:
        return static_cast<int32_t>(storage.integral);
      case Kind::kWord64:
        return static_cast<int64_t>(storage.integral);
      default:
        UNREACHABLE();
    }
  }

  uint32_t word32() const {
    DCHECK(kind == Kind::kWord32 || kind == Kind::kWord64);
    return static_cast<uint32_t>(storage.integral);
  }

  uint64_t word64() const {
    DCHECK_EQ(kind, Kind::kWord64);
    return static_cast<uint64_t>(storage.integral);
  }

  double number() const {
    DCHECK_EQ(kind, Kind::kNumber);
    return storage.float64;
  }

  float float32() const {
    DCHECK_EQ(kind, Kind::kFloat32);
    return storage.float32;
  }

  double float64() const {
    DCHECK_EQ(kind, Kind::kFloat64);
    return storage.float64;
  }

  int32_t tagged_index() const {
    DCHECK_EQ(kind, Kind::kTaggedIndex);
    return static_cast<int32_t>(static_cast<uint32_t>(storage.integral));
  }

  ExternalReference external_reference() const {
    DCHECK_EQ(kind, Kind::kExternal);
    return storage.external;
  }

  Handle<i::HeapObject> handle() const {
    DCHECK(kind == Kind::kHeapObject || kind == Kind::kCompressedHeapObject);
    return storage.handle;
  }

  bool IsZero() const {
    switch (kind) {
      case Kind::kWord32:
      case Kind::kWord64:
      case Kind::kTaggedIndex:
        return storage.integral == 0;
      case Kind::kFloat32:
        return storage.float32 == 0;
      case Kind::kFloat64:
      case Kind::kNumber:
        return storage.float64 == 0;
      case Kind::kExternal:
      case Kind::kHeapObject:
      case Kind::kCompressedHeapObject:
        UNREACHABLE();
    }
  }

  bool IsOne() const {
    switch (kind) {
      case Kind::kWord32:
      case Kind::kWord64:
      case Kind::kTaggedIndex:
        return storage.integral == 1;
      case Kind::kFloat32:
        return storage.float32 == 1;
      case Kind::kFloat64:
      case Kind::kNumber:
        return storage.float64 == 1;
      case Kind::kExternal:
      case Kind::kHeapObject:
      case Kind::kCompressedHeapObject:
        UNREACHABLE();
    }
  }

  bool IsWord(uint64_t value) const {
    switch (kind) {
      case Kind::kWord32:
        return static_cast<uint32_t>(value) == word32();
      case Kind::kWord64:
        return value == word64();
      default:
        UNREACHABLE();
    }
  }

  auto options() const { return std::tuple{kind, storage}; }

  void PrintOptions(std::ostream& os) const;
  size_t hash_value() const {
    switch (kind) {
      case Kind::kWord32:
      case Kind::kWord64:
      case Kind::kTaggedIndex:
        return fast_hash_combine(opcode, kind, storage.integral);
      case Kind::kFloat32:
        return fast_hash_combine(opcode, kind, storage.float32);
      case Kind::kFloat64:
      case Kind::kNumber:
        return fast_hash_combine(opcode, kind, storage.float64);
      case Kind::kExternal:
        return fast_hash_combine(opcode, kind, storage.external.address());
      case Kind::kHeapObject:
      case Kind::kCompressedHeapObject:
        return fast_hash_combine(opcode, kind, storage.handle.address());
    }
  }
  bool operator==(const ConstantOp& other) const {
    if (kind != other.kind) return false;
    switch (kind) {
      case Kind::kWord32:
      case Kind::kWord64:
      case Kind::kTaggedIndex:
        return storage.integral == other.storage.integral;
      case Kind::kFloat32:
        // Using a bit_cast to uint32_t in order to return false when comparing
        // +0 and -0.
        return base::bit_cast<uint32_t>(storage.float32) ==
                   base::bit_cast<uint32_t>(other.storage.float32) ||
               (std::isnan(storage.float32) &&
                std::isnan(other.storage.float32));
      case Kind::kFloat64:
      case Kind::kNumber:
        // Using a bit_cast to uint64_t in order to return false when comparing
        // +0 and -0.
        return base::bit_cast<uint64_t>(storage.float64) ==
                   base::bit_cast<uint64_t>(other.storage.float64) ||
               (std::isnan(storage.float64) &&
                std::isnan(other.storage.float64));
      case Kind::kExternal:
        return storage.external.address() == other.storage.external.address();
      case Kind::kHeapObject:
      case Kind::kCompressedHeapObject:
        return storage.handle.address() == other.storage.handle.address();
    }
  }
};

// Load loaded_rep from: base + offset.
// For Kind::tagged_base: subtract kHeapObjectTag,
//                        `base` has to be the object start.
// For (u)int8/16, the value will be sign- or zero-extended to Word32.
struct LoadOp : FixedArityOperationT<1, LoadOp> {
  enum class Kind { kTaggedBase, kRawAligned, kRawUnaligned };
  Kind kind;
  MachineType loaded_rep;
  int32_t offset;

  static constexpr OpProperties properties = OpProperties::Reading();

  OpIndex base() const { return input(0); }

  LoadOp(OpIndex base, Kind kind, MachineType loaded_rep, int32_t offset)
      : Base(base), kind(kind), loaded_rep(loaded_rep), offset(offset) {}
  void PrintOptions(std::ostream& os) const;
  auto options() const { return std::tuple{kind, loaded_rep, offset}; }
};

inline bool IsAlignedAccess(LoadOp::Kind kind) {
  switch (kind) {
    case LoadOp::Kind::kTaggedBase:
    case LoadOp::Kind::kRawAligned:
      return true;
    case LoadOp::Kind::kRawUnaligned:
      return false;
  }
}

// Load `loaded_rep` from: base + offset + index * 2^element_size_log2.
// For Kind::tagged_base: subtract kHeapObjectTag,
//                        `base` has to be the object start.
// For (u)int8/16, the value will be sign- or zero-extended to Word32.
struct IndexedLoadOp : FixedArityOperationT<2, IndexedLoadOp> {
  using Kind = LoadOp::Kind;
  Kind kind;
  MachineType loaded_rep;
  uint8_t element_size_log2;  // multiply index with 2^element_size_log2
  int32_t offset;             // add offset to scaled index

  static constexpr OpProperties properties = OpProperties::Reading();

  OpIndex base() const { return input(0); }
  OpIndex index() const { return input(1); }

  IndexedLoadOp(OpIndex base, OpIndex index, Kind kind, MachineType loaded_rep,
                int32_t offset, uint8_t element_size_log2)
      : Base(base, index),
        kind(kind),
        loaded_rep(loaded_rep),
        element_size_log2(element_size_log2),
        offset(offset) {}
  void PrintOptions(std::ostream& os) const;
  auto options() const {
    return std::tuple{kind, loaded_rep, offset, element_size_log2};
  }
};

// Store `value` to: base + offset.
// For Kind::tagged_base: subtract kHeapObjectTag,
//                        `base` has to be the object start.
struct StoreOp : FixedArityOperationT<2, StoreOp> {
  using Kind = LoadOp::Kind;
  Kind kind;
  MachineRepresentation stored_rep;
  WriteBarrierKind write_barrier;
  int32_t offset;

  static constexpr OpProperties properties = OpProperties::Writing();

  OpIndex base() const { return input(0); }
  OpIndex value() const { return input(1); }

  StoreOp(OpIndex base, OpIndex value, Kind kind,
          MachineRepresentation stored_rep, WriteBarrierKind write_barrier,
          int32_t offset)
      : Base(base, value),
        kind(kind),
        stored_rep(stored_rep),
        write_barrier(write_barrier),
        offset(offset) {}
  void PrintOptions(std::ostream& os) const;
  auto options() const {
    return std::tuple{kind, stored_rep, write_barrier, offset};
  }
};

// Store `value` to: base + offset + index * 2^element_size_log2.
// For Kind::tagged_base: subtract kHeapObjectTag,
//                        `base` has to be the object start.
struct IndexedStoreOp : FixedArityOperationT<3, IndexedStoreOp> {
  using Kind = StoreOp::Kind;
  Kind kind;
  MachineRepresentation stored_rep;
  WriteBarrierKind write_barrier;
  uint8_t element_size_log2;  // multiply index with 2^element_size_log2
  int32_t offset;             // add offset to scaled index

  static constexpr OpProperties properties = OpProperties::Writing();

  OpIndex base() const { return input(0); }
  OpIndex index() const { return input(1); }
  OpIndex value() const { return input(2); }

  IndexedStoreOp(OpIndex base, OpIndex index, OpIndex value, Kind kind,
                 MachineRepresentation stored_rep,
                 WriteBarrierKind write_barrier, int32_t offset,
                 uint8_t element_size_log2)
      : Base(base, index, value),
        kind(kind),
        stored_rep(stored_rep),
        write_barrier(write_barrier),
        element_size_log2(element_size_log2),
        offset(offset) {}
  void PrintOptions(std::ostream& os) const;
  auto options() const {
    return std::tuple{kind, stored_rep, write_barrier, offset,
                      element_size_log2};
  }
};

// Retain a HeapObject to prevent it from being garbage collected too early.
struct RetainOp : FixedArityOperationT<1, RetainOp> {
  OpIndex retained() const { return input(0); }

  // Retain doesn't actually write, it just keeps a value alive. However, since
  // this must not be reordered with operations reading from the heap, we mark
  // it as writing to prevent such reorderings.
  static constexpr OpProperties properties = OpProperties::Writing();

  explicit RetainOp(OpIndex retained) : Base(retained) {}
  auto options() const { return std::tuple{}; }
};

struct StackPointerGreaterThanOp
    : FixedArityOperationT<1, StackPointerGreaterThanOp> {
  StackCheckKind kind;

  static constexpr OpProperties properties = OpProperties::Reading();

  OpIndex stack_limit() const { return input(0); }

  StackPointerGreaterThanOp(OpIndex stack_limit, StackCheckKind kind)
      : Base(stack_limit), kind(kind) {}
  auto options() const { return std::tuple{kind}; }
};

// Allocate a piece of memory in the current stack frame. Every operation
// in the IR is a separate stack slot, but repeated execution in a loop
// produces the same stack slot.
struct StackSlotOp : FixedArityOperationT<0, StackSlotOp> {
  int size;
  int alignment;

  static constexpr OpProperties properties = OpProperties::Writing();

  StackSlotOp(int size, int alignment) : size(size), alignment(alignment) {}
  auto options() const { return std::tuple{size, alignment}; }
};

// Values that are constant for the current stack frame/invocation.
// Therefore, they behaves like a constant, even though they are different for
// every invocation.
struct FrameConstantOp : FixedArityOperationT<0, FrameConstantOp> {
  enum class Kind { kStackCheckOffset, kFramePointer, kParentFramePointer };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::Pure();

  explicit FrameConstantOp(Kind kind) : Base(), kind(kind) {}
  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, FrameConstantOp::Kind kind);

struct FrameStateOp : OperationT<FrameStateOp> {
  bool inlined;
  const FrameStateData* data;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex parent_frame_state() const {
    DCHECK(inlined);
    return input(0);
  }
  base::Vector<const OpIndex> state_values() const {
    base::Vector<const OpIndex> result = inputs();
    if (inlined) result += 1;
    return result;
  }

  FrameStateOp(base::Vector<const OpIndex> inputs, bool inlined,
               const FrameStateData* data)
      : Base(inputs), inlined(inlined), data(data) {}
  void PrintOptions(std::ostream& os) const;
  auto options() const { return std::tuple{inlined, data}; }
};

// CheckLazyDeoptOp should always immediately follow a call.
// Semantically, it deopts if the current code object has been
// deoptimized. But this might also be implemented differently.
struct CheckLazyDeoptOp : FixedArityOperationT<2, CheckLazyDeoptOp> {
  static constexpr OpProperties properties = OpProperties::CanDeopt();

  OpIndex call() const { return input(0); }
  OpIndex frame_state() const { return input(1); }

  CheckLazyDeoptOp(OpIndex call, OpIndex frame_state)
      : Base(call, frame_state) {}
  auto options() const { return std::tuple{}; }
};

struct DeoptimizeOp : FixedArityOperationT<1, DeoptimizeOp> {
  const DeoptimizeParameters* parameters;

  static constexpr OpProperties properties = OpProperties::BlockTerminator();

  OpIndex frame_state() const { return input(0); }

  DeoptimizeOp(OpIndex frame_state, const DeoptimizeParameters* parameters)
      : Base(frame_state), parameters(parameters) {}
  auto options() const { return std::tuple{parameters}; }
};

struct DeoptimizeIfOp : FixedArityOperationT<2, DeoptimizeIfOp> {
  bool negated;
  const DeoptimizeParameters* parameters;

  static constexpr OpProperties properties = OpProperties::CanDeopt();

  OpIndex condition() const { return input(0); }
  OpIndex frame_state() const { return input(1); }

  DeoptimizeIfOp(OpIndex condition, OpIndex frame_state, bool negated,
                 const DeoptimizeParameters* parameters)
      : Base(condition, frame_state),
        negated(negated),
        parameters(parameters) {}
  auto options() const { return std::tuple{negated, parameters}; }
};

struct ParameterOp : FixedArityOperationT<0, ParameterOp> {
  int32_t parameter_index;
  const char* debug_name;

  static constexpr OpProperties properties = OpProperties::Pure();

  explicit ParameterOp(int32_t parameter_index, const char* debug_name = "")
      : Base(), parameter_index(parameter_index), debug_name(debug_name) {}
  auto options() const { return std::tuple{parameter_index, debug_name}; }
  void PrintOptions(std::ostream& os) const;
};

struct OsrValueOp : FixedArityOperationT<0, OsrValueOp> {
  int32_t index;

  static constexpr OpProperties properties = OpProperties::Pure();

  explicit OsrValueOp(int32_t index) : Base(), index(index) {}
  auto options() const { return std::tuple{index}; }
};

struct CallOp : OperationT<CallOp> {
  const CallDescriptor* descriptor;

  static constexpr OpProperties properties = OpProperties::AnySideEffects();

  OpIndex callee() const { return input(0); }
  base::Vector<const OpIndex> arguments() const {
    return inputs().SubVector(1, input_count);
  }

  CallOp(OpIndex callee, base::Vector<const OpIndex> arguments,
         const CallDescriptor* descriptor)
      : Base(1 + arguments.size()), descriptor(descriptor) {
    base::Vector<OpIndex> inputs = this->inputs();
    inputs[0] = callee;
    inputs.SubVector(1, inputs.size()).OverwriteWith(arguments);
  }
  static CallOp& New(Graph* graph, OpIndex callee,
                     base::Vector<const OpIndex> arguments,
                     const CallDescriptor* descriptor) {
    return Base::New(graph, 1 + arguments.size(), callee, arguments,
                     descriptor);
  }
  auto options() const { return std::tuple{descriptor}; }
};

// Control-flow should never reach here.
struct UnreachableOp : FixedArityOperationT<0, UnreachableOp> {
  static constexpr OpProperties properties = OpProperties::BlockTerminator();

  UnreachableOp() : Base() {}
  auto options() const { return std::tuple{}; }
};

struct ReturnOp : OperationT<ReturnOp> {
  static constexpr OpProperties properties = OpProperties::BlockTerminator();

  // Number of additional stack slots to be removed.
  OpIndex pop_count() const { return input(0); }

  base::Vector<const OpIndex> return_values() const {
    return inputs().SubVector(1, input_count);
  }

  ReturnOp(OpIndex pop_count, base::Vector<const OpIndex> return_values)
      : Base(1 + return_values.size()) {
    base::Vector<OpIndex> inputs = this->inputs();
    inputs[0] = pop_count;
    inputs.SubVector(1, inputs.size()).OverwriteWith(return_values);
  }
  static ReturnOp& New(Graph* graph, OpIndex pop_count,
                       base::Vector<const OpIndex> return_values) {
    return Base::New(graph, 1 + return_values.size(), pop_count, return_values);
  }
  auto options() const { return std::tuple{}; }
};

struct GotoOp : FixedArityOperationT<0, GotoOp> {
  Block* destination;

  static constexpr OpProperties properties = OpProperties::BlockTerminator();

  explicit GotoOp(Block* destination) : Base(), destination(destination) {}
  auto options() const { return std::tuple{destination}; }
};

struct BranchOp : FixedArityOperationT<1, BranchOp> {
  Block* if_true;
  Block* if_false;

  static constexpr OpProperties properties = OpProperties::BlockTerminator();

  OpIndex condition() const { return input(0); }

  BranchOp(OpIndex condition, Block* if_true, Block* if_false)
      : Base(condition), if_true(if_true), if_false(if_false) {}
  auto options() const { return std::tuple{if_true, if_false}; }
};

// `CatchExceptionOp` has to follow a `CallOp` with a subsequent
// `CheckLazyDeoptOp`. It provides the exception value, which might only be used
// from the `if_exception` successor.
struct CatchExceptionOp : FixedArityOperationT<1, CatchExceptionOp> {
  Block* if_success;
  Block* if_exception;

  static constexpr OpProperties properties = OpProperties::BlockTerminator();

  OpIndex call() const { return input(0); }

  explicit CatchExceptionOp(OpIndex call, Block* if_success,
                            Block* if_exception)
      : Base(call), if_success(if_success), if_exception(if_exception) {}
  auto options() const { return std::tuple{if_success, if_exception}; }
};

struct SwitchOp : FixedArityOperationT<1, SwitchOp> {
  struct Case {
    int32_t value;
    Block* destination;

    Case(int32_t value, Block* destination)
        : value(value), destination(destination) {}

    bool operator==(const Case& other) const {
      return value == other.value && destination == other.destination;
    }
  };
  base::Vector<const Case> cases;
  Block* default_case;

  static constexpr OpProperties properties = OpProperties::BlockTerminator();

  OpIndex input() const { return Base::input(0); }

  SwitchOp(OpIndex input, base::Vector<const Case> cases, Block* default_case)
      : Base(input), cases(cases), default_case(default_case) {}
  void PrintOptions(std::ostream& os) const;
  auto options() const { return std::tuple{cases, default_case}; }
};

template <>
struct fast_hash<SwitchOp::Case> {
  size_t operator()(SwitchOp::Case v) {
    return fast_hash_combine(v.value, v.destination);
  }
};

// Tuples are only used to lower operations with multiple outputs.
// `TupleOp` should be folded away by subsequent `ProjectionOp`s.
struct TupleOp : OperationT<TupleOp> {
  static constexpr OpProperties properties = OpProperties::Pure();

  explicit TupleOp(base::Vector<const OpIndex> inputs) : Base(inputs) {}
  auto options() const { return std::tuple{}; }
};

// For operations that produce multiple results, we use `ProjectionOp` to
// distinguish them.
struct ProjectionOp : FixedArityOperationT<1, ProjectionOp> {
  uint16_t index;

  static constexpr OpProperties properties = OpProperties::Pure();

  OpIndex input() const { return Base::input(0); }

  ProjectionOp(OpIndex input, uint16_t index) : Base(input), index(index) {}
  auto options() const { return std::tuple{index}; }
};

#define OPERATION_PROPERTIES_CASE(Name) Name##Op::properties,
static constexpr OpProperties kOperationPropertiesTable[kNumberOfOpcodes] = {
    TURBOSHAFT_OPERATION_LIST(OPERATION_PROPERTIES_CASE)};
#undef OPERATION_PROPERTIES_CASE

template <class Op>
struct operation_to_opcode_map {};

#define OPERATION_OPCODE_MAP_CASE(Name)    \
  template <>                              \
  struct operation_to_opcode_map<Name##Op> \
      : std::integral_constant<Opcode, Opcode::k##Name> {};
TURBOSHAFT_OPERATION_LIST(OPERATION_OPCODE_MAP_CASE)
#undef OPERATION_OPCODE_MAP_CASE

template <class Op>
const Opcode OperationT<Op>::opcode = operation_to_opcode_map<Op>::value;

template <class Op, class = void>
struct static_operation_input_count : std::integral_constant<uint32_t, 0> {};
template <class Op>
struct static_operation_input_count<Op, std::void_t<decltype(Op::inputs)>>
    : std::integral_constant<uint32_t, sizeof(Op::inputs) / sizeof(OpIndex)> {};
constexpr size_t kOperationSizeTable[kNumberOfOpcodes] = {
#define OPERATION_SIZE(Name) sizeof(Name##Op),
    TURBOSHAFT_OPERATION_LIST(OPERATION_SIZE)
#undef OPERATION_SIZE
};
constexpr size_t kOperationSizeDividedBySizeofOpIndexTable[kNumberOfOpcodes] = {
#define OPERATION_SIZE(Name) (sizeof(Name##Op) / sizeof(OpIndex)),
    TURBOSHAFT_OPERATION_LIST(OPERATION_SIZE)
#undef OPERATION_SIZE
};

inline base::Vector<const OpIndex> Operation::inputs() const {
  // This is actually undefined behavior, since we use the `this` pointer to
  // access an adjacent object.
  const OpIndex* ptr = reinterpret_cast<const OpIndex*>(
      reinterpret_cast<const char*>(this) +
      kOperationSizeTable[OpcodeIndex(opcode)]);
  return {ptr, input_count};
}

inline const OpProperties& Operation::properties() const {
  return kOperationPropertiesTable[OpcodeIndex(opcode)];
}

// static
inline size_t Operation::StorageSlotCount(Opcode opcode, size_t input_count) {
  size_t size = kOperationSizeDividedBySizeofOpIndexTable[OpcodeIndex(opcode)];
  constexpr size_t r = sizeof(OperationStorageSlot) / sizeof(OpIndex);
  static_assert(sizeof(OperationStorageSlot) % sizeof(OpIndex) == 0);
  return std::max<size_t>(2, (r - 1 + size + input_count) / r);
}

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_OPERATIONS_H_
