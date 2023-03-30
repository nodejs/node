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

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/optional.h"
#include "src/base/platform/mutex.h"
#include "src/base/small-vector.h"
#include "src/base/template-utils.h"
#include "src/base/vector.h"
#include "src/codegen/external-reference.h"
#include "src/common/globals.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/globals.h"
#include "src/compiler/turboshaft/deopt-data.h"
#include "src/compiler/turboshaft/fast-hash.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/types.h"
#include "src/compiler/turboshaft/utils.h"
#include "src/compiler/write-barrier-kind.h"

namespace v8::internal {
class HeapObject;
}  // namespace v8::internal
namespace v8::internal::compiler {
class CallDescriptor;
class DeoptimizeParameters;
class FrameStateInfo;
class Node;
enum class TrapId : uint32_t;
}  // namespace v8::internal::compiler
namespace v8::internal::compiler::turboshaft {
class Block;
struct FrameStateData;
class Graph;
struct FrameStateOp;

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
// - `OperatorProperties` as either a static constexpr member `properties` or a
//   non-static method `Properties()` if the properties depend on the particular
//   operation and not just the opcode.

#ifdef V8_INTL_SUPPORT
#define TURBOSHAFT_INTL_OPERATION_LIST(V) V(StringToCaseIntl)
#else
#define TURBOSHAFT_INTL_OPERATION_LIST(V)
#endif  // V8_INTL_SUPPORT

#define TURBOSHAFT_OPERATION_LIST(V) \
  TURBOSHAFT_INTL_OPERATION_LIST(V)  \
  V(WordBinop)                       \
  V(FloatBinop)                      \
  V(OverflowCheckedBinop)            \
  V(WordUnary)                       \
  V(FloatUnary)                      \
  V(Shift)                           \
  V(Equal)                           \
  V(Comparison)                      \
  V(Change)                          \
  V(ChangeOrDeopt)                   \
  V(TryChange)                       \
  V(Float64InsertWord32)             \
  V(TaggedBitcast)                   \
  V(Select)                          \
  V(PendingLoopPhi)                  \
  V(Constant)                        \
  V(Load)                            \
  V(Store)                           \
  V(Allocate)                        \
  V(DecodeExternalPointer)           \
  V(Retain)                          \
  V(Parameter)                       \
  V(OsrValue)                        \
  V(Goto)                            \
  V(StackPointerGreaterThan)         \
  V(StackSlot)                       \
  V(FrameConstant)                   \
  V(Deoptimize)                      \
  V(DeoptimizeIf)                    \
  V(TrapIf)                          \
  V(Phi)                             \
  V(FrameState)                      \
  V(Call)                            \
  V(CallAndCatchException)           \
  V(LoadException)                   \
  V(TailCall)                        \
  V(Unreachable)                     \
  V(Return)                          \
  V(Branch)                          \
  V(Switch)                          \
  V(Tuple)                           \
  V(Projection)                      \
  V(StaticAssert)                    \
  V(CheckTurboshaftTypeOf)           \
  V(ObjectIs)                        \
  V(FloatIs)                         \
  V(ConvertToObject)                 \
  V(ConvertToObjectOrDeopt)          \
  V(ConvertObjectToPrimitive)        \
  V(ConvertObjectToPrimitiveOrDeopt) \
  V(TruncateObjectToPrimitive)       \
  V(Tag)                             \
  V(Untag)                           \
  V(NewConsString)                   \
  V(NewArray)                        \
  V(DoubleArrayMinMax)               \
  V(LoadFieldByIndex)                \
  V(DebugBreak)                      \
  V(BigIntBinop)                     \
  V(BigIntEqual)                     \
  V(BigIntComparison)                \
  V(BigIntUnary)                     \
  V(LoadRootRegister)                \
  V(StringAt)                        \
  V(StringLength)                    \
  V(StringIndexOf)                   \
  V(StringFromCodePointAt)           \
  V(StringSubstring)                 \
  V(StringEqual)                     \
  V(StringComparison)

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

struct OpProperties {
  // The operation may read memory or depend on other information beyond its
  // inputs. Generating random numbers or nondeterministic behavior counts as
  // reading.
  const bool can_read;
  // The operation may write memory or have other observable side-effects.
  // Writing to memory allocated as part of the operation does not count, since
  // it is not observable.
  const bool can_write;
  // The operation can allocate memory on the heap, which might also trigger GC.
  const bool can_allocate;
  // The operation can abort the current execution by throwing an exception or
  // deoptimizing.
  const bool can_abort;
  // The last operation in a basic block.
  const bool is_block_terminator;
  // By being const and not being set in the constructor, these properties are
  // guaranteed to be derived.
  const bool is_pure_no_allocation = !(can_read || can_write || can_allocate ||
                                       can_abort || is_block_terminator);
  const bool is_required_when_unused =
      can_write || can_abort || is_block_terminator;
  // Operations that don't read, write, allocate and aren't block terminators
  // can be eliminated via value numbering, which means that if there are two
  // identical operations where one dominates the other, then the second can be
  // replaced with the first one. This is safe for deopting or throwing
  // operations, because the first instance would have aborted the execution
  // already as
  // `!can_read` guarantees deterministic behavior.
  const bool can_be_eliminated =
      !(can_read || can_write || can_allocate || is_block_terminator);

  constexpr OpProperties(bool can_read, bool can_write, bool can_allocate,
                         bool can_abort, bool is_block_terminator)
      : can_read(can_read),
        can_write(can_write),
        can_allocate(can_allocate),
        can_abort(can_abort),
        is_block_terminator(is_block_terminator) {}

#define ALL_OP_PROPERTIES(V)                                        \
  V(PureNoAllocation, false, false, false, false, false)            \
  V(PureMayAllocate, false, false, true, false, false)              \
  V(Reading, true, false, false, false, false)                      \
  V(Writing, false, true, false, false, false)                      \
  V(CanAbort, false, false, false, true, false)                     \
  V(AnySideEffects, true, true, true, true, false)                  \
  V(BlockTerminator, false, false, false, false, true)              \
  V(BlockTerminatorWithAnySideEffect, true, true, true, true, true) \
  V(ReadingAndCanAbort, true, false, false, true, false)            \
  V(WritingAndCanAbort, false, true, false, true, false)

#define DEFINE_OP_PROPERTY(Name, can_read, can_write, can_allocate, can_abort, \
                           is_block_terminator)                                \
  static constexpr OpProperties Name() {                                       \
    return {can_read, can_write, can_allocate, can_abort,                      \
            is_block_terminator};                                              \
  }

  ALL_OP_PROPERTIES(DEFINE_OP_PROPERTY)
#undef DEFINE_OP_PROPERTY

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

  base::Vector<const RegisterRepresentation> outputs_rep() const;

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
  OpProperties Properties() const;

  std::string ToString() const;
  void PrintInputs(std::ostream& os, const std::string& op_index_prefix) const;
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
void Print(const Operation& op);

OperationStorageSlot* AllocateOpStorage(Graph* graph, size_t slot_count);
const Operation& Get(const Graph& graph, OpIndex index);

// Determine if an operation declares `properties`, which means that its
// properties are static and don't depend on inputs or options.
template <class Op, class = void>
struct HasStaticProperties : std::bool_constant<false> {};
template <class Op>
struct HasStaticProperties<Op, std::void_t<decltype(Op::properties)>>
    : std::bool_constant<true> {};

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

  static constexpr OpProperties Properties() { return Derived::properties; }

  static constexpr base::Optional<OpProperties> PropertiesIfStatic() {
    if constexpr (HasStaticProperties<Derived>::value) {
      return Derived::Properties();
    }
    return base::nullopt;
  }

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
#ifdef DEBUG
    result->Validate(*graph);
#endif
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

  void PrintInputs(std::ostream& os, const std::string& op_index_prefix) const {
    os << "(";
    bool first = true;
    for (OpIndex input : inputs()) {
      if (!first) os << ", ";
      first = false;
      os << op_index_prefix << input.id();
    }
    os << ")";
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

  // Check graph invariants for this operation. Will be invoked in debug mode
  // immediately upon construction.
  // Concrete Operator classes are expected to re-define it.
  void Validate(const Graph& graph) const = delete;

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

  // All Operations have to define the outputs_rep function, to which
  // Operation::outputs_rep() will forward, based on their opcode. If you forget
  // to define it, then Operation::outputs_rep() would forward to itself,
  // resulting in an infinite loop. To avoid this, we define here in OperationT
  // a private version outputs_rep (with no implementation): if an operation
  // forgets to define outputs_rep, then Operation::outputs_rep() tries to call
  // this private version, which fails at compile time.
  base::Vector<const RegisterRepresentation> outputs_rep() const;
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

template <RegisterRepresentation::Enum... reps>
base::Vector<const RegisterRepresentation> RepVector() {
  static const std::array<RegisterRepresentation, sizeof...(reps)> rep_array{
      RegisterRepresentation{reps}...};
  return base::VectorOf(rep_array);
}

bool ValidOpInputRep(const Graph& graph, OpIndex input,
                     std::initializer_list<RegisterRepresentation> expected_rep,
                     base::Optional<size_t> projection_index = {});
bool ValidOpInputRep(const Graph& graph, OpIndex input,
                     RegisterRepresentation expected_rep,
                     base::Optional<size_t> projection_index = {});

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
  WordRepresentation rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(static_cast<const RegisterRepresentation*>(&rep), 1);
  }

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

  static bool IsAssociative(Kind kind) {
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

  WordBinopOp(OpIndex left, OpIndex right, Kind kind, WordRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, left(), rep));
    DCHECK(ValidOpInputRep(graph, right(), rep));
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
  FloatRepresentation rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(static_cast<const RegisterRepresentation*>(&rep), 1);
  }

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

  FloatBinopOp(OpIndex left, OpIndex right, Kind kind, FloatRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {}

  void Validate(const Graph& graph) const {
    DCHECK_IMPLIES(kind == any_of(Kind::kPower, Kind::kAtan2, Kind::kMod),
                   rep == FloatRepresentation::Float64());
    DCHECK(ValidOpInputRep(graph, left(), rep));
    DCHECK(ValidOpInputRep(graph, right(), rep));
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
  WordRepresentation rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    switch (rep.value()) {
      case WordRepresentation::Word32():
        return RepVector<RegisterRepresentation::Word32(),
                         RegisterRepresentation::Word32()>();
      case WordRepresentation::Word64():
        return RepVector<RegisterRepresentation::Word64(),
                         RegisterRepresentation::Word32()>();
    }
  }

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
                         WordRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, left(), rep));
    DCHECK(ValidOpInputRep(graph, right(), rep));
  }
  auto options() const { return std::tuple{kind, rep}; }
  void PrintOptions(std::ostream& os) const;
};

struct WordUnaryOp : FixedArityOperationT<1, WordUnaryOp> {
  enum class Kind : uint8_t {
    kReverseBytes,
    kCountLeadingZeros,
    kCountTrailingZeros,
    kPopCount,
    kSignExtend8,
    kSignExtend16,
  };
  Kind kind;
  WordRepresentation rep;
  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(static_cast<const RegisterRepresentation*>(&rep), 1);
  }

  OpIndex input() const { return Base::input(0); }

  static bool IsSupported(Kind kind, WordRepresentation rep);

  explicit WordUnaryOp(OpIndex input, Kind kind, WordRepresentation rep)
      : Base(input), kind(kind), rep(rep) {}

  void Validate(const Graph& graph) const {
    DCHECK(IsSupported(kind, rep));
    DCHECK(ValidOpInputRep(graph, input(), rep));
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
    kLog2,
    kLog10,
    kLog1p,
    kSqrt,
    kCbrt,
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
    kAtan,
    kAtanh,
  };
  Kind kind;
  FloatRepresentation rep;
  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(static_cast<const RegisterRepresentation*>(&rep), 1);
  }

  OpIndex input() const { return Base::input(0); }

  static bool IsSupported(Kind kind, FloatRepresentation rep);

  explicit FloatUnaryOp(OpIndex input, Kind kind, FloatRepresentation rep)
      : Base(input), kind(kind), rep(rep) {}

  void Validate(const Graph& graph) const {
    DCHECK(IsSupported(kind, rep));
    DCHECK(ValidOpInputRep(graph, input(), rep));
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
  WordRepresentation rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(static_cast<const RegisterRepresentation*>(&rep), 1);
  }

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

  ShiftOp(OpIndex left, OpIndex right, Kind kind, WordRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, left(), rep));
    DCHECK(ValidOpInputRep(graph, right(), WordRepresentation::Word32()));
  }
  auto options() const { return std::tuple{kind, rep}; }
};
std::ostream& operator<<(std::ostream& os, ShiftOp::Kind kind);

struct EqualOp : FixedArityOperationT<2, EqualOp> {
  RegisterRepresentation rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Word32()>();
  }

  OpIndex left() const { return input(0); }
  OpIndex right() const { return input(1); }

  bool ValidInputRep(
      base::Vector<const RegisterRepresentation> input_reps) const;

  EqualOp(OpIndex left, OpIndex right, RegisterRepresentation rep)
      : Base(left, right), rep(rep) {}

  void Validate(const Graph& graph) const {
#ifdef DEBUG
    DCHECK(rep == any_of(RegisterRepresentation::Word32(),
                         RegisterRepresentation::Word64(),
                         RegisterRepresentation::Float32(),
                         RegisterRepresentation::Float64(),
                         RegisterRepresentation::Tagged()));
    RegisterRepresentation input_rep = rep;
#ifdef V8_COMPRESS_POINTERS
    // In the presence of pointer compression, we only compare the lower 32bit.
    if (input_rep == RegisterRepresentation::Tagged()) {
      input_rep = RegisterRepresentation::Compressed();
    }
#endif  // V8_COMPRESS_POINTERS
    DCHECK(ValidOpInputRep(graph, left(), input_rep));
    DCHECK(ValidOpInputRep(graph, right(), input_rep));
#endif  // DEBUG
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
  RegisterRepresentation rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Word32()>();
  }

  OpIndex left() const { return input(0); }
  OpIndex right() const { return input(1); }

  ComparisonOp(OpIndex left, OpIndex right, Kind kind,
               RegisterRepresentation rep)
      : Base(left, right), kind(kind), rep(rep) {}

  void Validate(const Graph& graph) const {
    DCHECK_EQ(rep, any_of(RegisterRepresentation::Word32(),
                          RegisterRepresentation::Word64(),
                          RegisterRepresentation::Float32(),
                          RegisterRepresentation::Float64()));
    DCHECK_IMPLIES(
        rep == any_of(RegisterRepresentation::Float32(),
                      RegisterRepresentation::Float64()),
        kind == any_of(Kind::kSignedLessThan, Kind::kSignedLessThanOrEqual));
    DCHECK(ValidOpInputRep(graph, left(), rep));
    DCHECK(ValidOpInputRep(graph, right(), rep));
  }
  auto options() const { return std::tuple{kind, rep}; }

  static bool IsLessThan(Kind kind) {
    switch (kind) {
      case Kind::kSignedLessThan:
      case Kind::kUnsignedLessThan:
        return true;
      case Kind::kSignedLessThanOrEqual:
      case Kind::kUnsignedLessThanOrEqual:
        return false;
    }
  }
  static bool IsSigned(Kind kind) {
    switch (kind) {
      case Kind::kSignedLessThan:
      case Kind::kSignedLessThanOrEqual:
        return true;
      case Kind::kUnsignedLessThan:
      case Kind::kUnsignedLessThanOrEqual:
        return false;
    }
  }
  static Kind SetSigned(Kind kind, bool is_signed) {
    switch (kind) {
      case Kind::kSignedLessThan:
      case Kind::kUnsignedLessThan:
        return is_signed ? Kind::kSignedLessThan : Kind::kUnsignedLessThan;
      case Kind::kSignedLessThanOrEqual:
      case Kind::kUnsignedLessThanOrEqual:
        return is_signed ? Kind::kSignedLessThanOrEqual
                         : Kind::kUnsignedLessThanOrEqual;
    }
  }
};
std::ostream& operator<<(std::ostream& os, ComparisonOp::Kind kind);

struct ChangeOp : FixedArityOperationT<1, ChangeOp> {
  enum class Kind : uint8_t {
    // convert between different floating-point types
    kFloatConversion,
    // overflow guaranteed to result in the minimal integer
    kSignedFloatTruncateOverflowToMin,
    kUnsignedFloatTruncateOverflowToMin,
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
  // Violated assumptions result in undefined behavior.
  enum class Assumption : uint8_t {
    kNoAssumption,
    // Used for conversions from floating-point to integer, assumes that the
    // value doesn't exceed the integer range.
    kNoOverflow,
    // Assume that the original value can be recovered by a corresponding
    // reverse transformation.
    kReversible,
  };
  Kind kind;
  // Reversible means undefined behavior if value cannot be represented
  // precisely.
  Assumption assumption;
  RegisterRepresentation from;
  RegisterRepresentation to;

  static bool IsReversible(Kind kind, Assumption assumption,
                           RegisterRepresentation from,
                           RegisterRepresentation to, Kind reverse_kind,
                           bool signalling_nan_possible) {
    switch (kind) {
      case Kind::kFloatConversion:
        return from == RegisterRepresentation::Float32() &&
               to == RegisterRepresentation::Float64() &&
               reverse_kind == Kind::kFloatConversion &&
               !signalling_nan_possible;
      case Kind::kSignedFloatTruncateOverflowToMin:
        return assumption == Assumption::kReversible &&
               reverse_kind == Kind::kSignedToFloat;
      case Kind::kUnsignedFloatTruncateOverflowToMin:
        return assumption == Assumption::kReversible &&
               reverse_kind == Kind::kUnsignedToFloat;
      case Kind::kJSFloatTruncate:
        return false;
      case Kind::kSignedToFloat:
        if (from == RegisterRepresentation::Word32() &&
            to == RegisterRepresentation::Float64()) {
          return reverse_kind == any_of(Kind::kSignedFloatTruncateOverflowToMin,
                                        Kind::kJSFloatTruncate);
        } else {
          return assumption == Assumption::kReversible &&
                 reverse_kind ==
                     any_of(Kind::kSignedFloatTruncateOverflowToMin);
        }
      case Kind::kUnsignedToFloat:
        if (from == RegisterRepresentation::Word32() &&
            to == RegisterRepresentation::Float64()) {
          return reverse_kind ==
                 any_of(Kind::kUnsignedFloatTruncateOverflowToMin,
                        Kind::kJSFloatTruncate);
        } else {
          return assumption == Assumption::kReversible &&
                 reverse_kind == Kind::kUnsignedFloatTruncateOverflowToMin;
        }
      case Kind::kExtractHighHalf:
      case Kind::kExtractLowHalf:
      case Kind::kZeroExtend:
      case Kind::kSignExtend:
        return false;
      case Kind::kBitcast:
        return reverse_kind == Kind::kBitcast;
    }
  }

  bool IsReversibleBy(Kind reverse_kind, bool signalling_nan_possible) const {
    return IsReversible(kind, assumption, from, to, reverse_kind,
                        signalling_nan_possible);
  }

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(&to, 1);
  }

  OpIndex input() const { return Base::input(0); }

  ChangeOp(OpIndex input, Kind kind, Assumption assumption,
           RegisterRepresentation from, RegisterRepresentation to)
      : Base(input), kind(kind), assumption(assumption), from(from), to(to) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), from));
  }
  auto options() const { return std::tuple{kind, assumption, from, to}; }
};
std::ostream& operator<<(std::ostream& os, ChangeOp::Kind kind);
std::ostream& operator<<(std::ostream& os, ChangeOp::Assumption assumption);

struct ChangeOrDeoptOp : FixedArityOperationT<2, ChangeOrDeoptOp> {
  enum class Kind : uint8_t {
    kUint32ToInt32,
    kInt64ToInt32,
    kUint64ToInt32,
    kUint64ToInt64,
    kFloat64ToInt32,
    kFloat64ToInt64,
  };
  Kind kind;
  CheckForMinusZeroMode minus_zero_mode;
  FeedbackSource feedback;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    switch (kind) {
      case Kind::kUint32ToInt32:
      case Kind::kInt64ToInt32:
      case Kind::kUint64ToInt32:
      case Kind::kFloat64ToInt32:
        return RepVector<RegisterRepresentation::Word32()>();
      case Kind::kUint64ToInt64:
      case Kind::kFloat64ToInt64:
        return RepVector<RegisterRepresentation::Word64()>();
    }
  }

  OpIndex input() const { return Base::input(0); }
  OpIndex frame_state() const { return Base::input(1); }

  ChangeOrDeoptOp(OpIndex input, OpIndex frame_state, Kind kind,
                  CheckForMinusZeroMode minus_zero_mode,
                  const FeedbackSource& feedback)
      : Base(input, frame_state),
        kind(kind),
        minus_zero_mode(minus_zero_mode),
        feedback(feedback) {}

  void Validate(const Graph& graph) const {
    switch (kind) {
      case Kind::kUint32ToInt32:
        DCHECK(
            ValidOpInputRep(graph, input(), RegisterRepresentation::Word32()));
        break;
      case Kind::kInt64ToInt32:
      case Kind::kUint64ToInt32:
      case Kind::kUint64ToInt64:
        DCHECK(
            ValidOpInputRep(graph, input(), RegisterRepresentation::Word64()));
        break;
      case Kind::kFloat64ToInt32:
      case Kind::kFloat64ToInt64:
        DCHECK(
            ValidOpInputRep(graph, input(), RegisterRepresentation::Float64()));
        break;
    }
    DCHECK(Get(graph, frame_state()).Is<FrameStateOp>());
  }
  auto options() const { return std::tuple{kind, minus_zero_mode, feedback}; }
};
std::ostream& operator<<(std::ostream& os, ChangeOrDeoptOp::Kind kind);

// Perform a conversion and return a pair of the result and a bit if it was
// successful.
struct TryChangeOp : FixedArityOperationT<1, TryChangeOp> {
  static constexpr uint32_t kSuccessValue = 1;
  static constexpr uint32_t kFailureValue = 0;
  enum class Kind : uint8_t {
    // The result of the truncation is undefined if the result is out of range.
    kSignedFloatTruncateOverflowUndefined,
    kUnsignedFloatTruncateOverflowUndefined,
  };
  Kind kind;
  FloatRepresentation from;
  WordRepresentation to;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    switch (to.value()) {
      case WordRepresentation::Word32():
        return RepVector<RegisterRepresentation::Word32(),
                         RegisterRepresentation::Word32()>();
      case WordRepresentation::Word64():
        return RepVector<RegisterRepresentation::Word64(),
                         RegisterRepresentation::Word32()>();
    }
  }

  OpIndex input() const { return Base::input(0); }

  TryChangeOp(OpIndex input, Kind kind, FloatRepresentation from,
              WordRepresentation to)
      : Base(input), kind(kind), from(from), to(to) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), from));
  }
  auto options() const { return std::tuple{kind, from, to}; }
};
std::ostream& operator<<(std::ostream& os, TryChangeOp::Kind kind);

// TODO(tebbi): Unify with other operations.
struct Float64InsertWord32Op : FixedArityOperationT<2, Float64InsertWord32Op> {
  enum class Kind { kLowHalf, kHighHalf };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Float64()>();
  }

  OpIndex float64() const { return input(0); }
  OpIndex word32() const { return input(1); }

  Float64InsertWord32Op(OpIndex float64, OpIndex word32, Kind kind)
      : Base(float64, word32), kind(kind) {}

  void Validate(const Graph& graph) const {
    DCHECK(
        ValidOpInputRep(graph, float64(), RegisterRepresentation::Float64()));
    DCHECK(ValidOpInputRep(graph, word32(), RegisterRepresentation::Word32()));
  }
  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, Float64InsertWord32Op::Kind kind);

struct TaggedBitcastOp : FixedArityOperationT<1, TaggedBitcastOp> {
  RegisterRepresentation from;
  RegisterRepresentation to;

  // Due to moving GC, converting from or to pointers doesn't commute with GC.
  static constexpr OpProperties properties = OpProperties::Reading();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(&to, 1);
  }

  OpIndex input() const { return Base::input(0); }

  TaggedBitcastOp(OpIndex input, RegisterRepresentation from,
                  RegisterRepresentation to)
      : Base(input), from(from), to(to) {}

  void Validate(const Graph& graph) const {
    DCHECK((from == RegisterRepresentation::PointerSized() &&
            to == RegisterRepresentation::Tagged()) ||
           (from == RegisterRepresentation::Tagged() &&
            to == RegisterRepresentation::PointerSized()) ||
           (from == RegisterRepresentation::Compressed() &&
            to == RegisterRepresentation::Word32()));
    DCHECK(ValidOpInputRep(graph, input(), from));
  }
  auto options() const { return std::tuple{from, to}; }
};

struct SelectOp : FixedArityOperationT<3, SelectOp> {
  enum class Implementation : uint8_t { kBranch, kCMove };

  RegisterRepresentation rep;
  BranchHint hint;
  Implementation implem;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(&rep, 1);
  }

  SelectOp(OpIndex cond, OpIndex vtrue, OpIndex vfalse,
           RegisterRepresentation rep, BranchHint hint, Implementation implem)
      : Base(cond, vtrue, vfalse), rep(rep), hint(hint), implem(implem) {}

  void Validate(const Graph& graph) const {
    DCHECK_IMPLIES(implem == Implementation::kCMove,
                   (rep == RegisterRepresentation::Word32() &&
                    SupportedOperations::word32_select()) ||
                       (rep == RegisterRepresentation::Word64() &&
                        SupportedOperations::word64_select()));
    DCHECK(ValidOpInputRep(graph, cond(), RegisterRepresentation::Word32()));
    DCHECK(ValidOpInputRep(graph, vtrue(), rep));
    DCHECK(ValidOpInputRep(graph, vfalse(), rep));
  }

  OpIndex cond() const { return input(0); }
  OpIndex vtrue() const { return input(1); }
  OpIndex vfalse() const { return input(2); }

  auto options() const { return std::tuple{rep, hint, implem}; }
};
std::ostream& operator<<(std::ostream& os, SelectOp::Implementation kind);

struct PhiOp : OperationT<PhiOp> {
  RegisterRepresentation rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(&rep, 1);
  }

  static constexpr size_t kLoopPhiBackEdgeIndex = 1;

  explicit PhiOp(base::Vector<const OpIndex> inputs, RegisterRepresentation rep)
      : Base(inputs), rep(rep) {}

  void Validate(const Graph& graph) const {
#ifdef DEBUG
    for (OpIndex input : inputs()) {
      DCHECK(ValidOpInputRep(graph, input, rep));
    }
#endif
  }
  auto options() const { return std::tuple{rep}; }
};

// Only used when moving a loop phi to a new graph while the loop backedge has
// not been emitted yet.
struct PendingLoopPhiOp : FixedArityOperationT<1, PendingLoopPhiOp> {
  struct PhiIndex {
    int index;
  };
  struct Data {
    union {
      // Used when transforming a Turboshaft graph.
      // This is not an input because it refers to the old graph.
      OpIndex old_backedge_index = OpIndex::Invalid();
      // Used when translating from sea-of-nodes.
      Node* old_backedge_node;
      // Used when building loops with the assembler macros.
      PhiIndex phi_index;
    };
    explicit Data(OpIndex old_backedge_index)
        : old_backedge_index(old_backedge_index) {}
    explicit Data(Node* old_backedge_node)
        : old_backedge_node(old_backedge_node) {}
    explicit Data(PhiIndex phi_index) : phi_index(phi_index) {}
  };
  RegisterRepresentation rep;
  Data data;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(&rep, 1);
  }

  OpIndex first() const { return input(0); }

  PendingLoopPhiOp(OpIndex first, RegisterRepresentation rep, Data data)
      : Base(first), rep(rep), data(data) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, first(), rep));
  }
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
    kCompressedHeapObject,
    kRelocatableWasmCall,
    kRelocatableWasmStubCall
  };

  Kind kind;
  RegisterRepresentation rep = Representation(kind);
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

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(&rep, 1);
  }

  static RegisterRepresentation Representation(Kind kind) {
    switch (kind) {
      case Kind::kWord32:
        return RegisterRepresentation::Word32();
      case Kind::kWord64:
        return RegisterRepresentation::Word64();
      case Kind::kFloat32:
        return RegisterRepresentation::Float32();
      case Kind::kFloat64:
        return RegisterRepresentation::Float64();
      case Kind::kExternal:
      case Kind::kTaggedIndex:
      case Kind::kRelocatableWasmCall:
      case Kind::kRelocatableWasmStubCall:
        return RegisterRepresentation::PointerSized();
      case Kind::kHeapObject:
      case Kind::kNumber:
        return RegisterRepresentation::Tagged();
      case Kind::kCompressedHeapObject:
        return RegisterRepresentation::Compressed();
    }
  }

  ConstantOp(Kind kind, Storage storage)
      : Base(), kind(kind), storage(storage) {}

  void Validate(const Graph& graph) const {
    DCHECK_IMPLIES(
        kind == Kind::kWord32,
        storage.integral <= WordRepresentation::Word32().MaxUnsignedValue());
  }

  uint64_t integral() const {
    DCHECK(kind == Kind::kWord32 || kind == Kind::kWord64 ||
           kind == Kind::kRelocatableWasmCall ||
           kind == Kind::kRelocatableWasmStubCall);
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
      case Kind::kRelocatableWasmCall:
      case Kind::kRelocatableWasmStubCall:
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
      case Kind::kRelocatableWasmCall:
      case Kind::kRelocatableWasmStubCall:
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
      case Kind::kRelocatableWasmCall:
      case Kind::kRelocatableWasmStubCall:
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
      case Kind::kRelocatableWasmCall:
      case Kind::kRelocatableWasmStubCall:
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

// Load `loaded_rep` from: base + offset + index * 2^element_size_log2.
// For Kind::tagged_base: subtract kHeapObjectTag,
//                        `base` has to be the object start.
// For (u)int8/16, the value will be sign- or zero-extended to Word32.
// When result_rep is RegisterRepresentation::Compressed(), then the load does
// not decompress the value.
struct LoadOp : OperationT<LoadOp> {
  struct Kind {
    // The `base` input is a tagged pointer to a HeapObject.
    bool tagged_base : 1;
    // The effective address might be unaligned.
    bool maybe_unaligned : 1;
    // There is a Wasm trap handler for out-of-bounds accesses.
    bool with_trap_handler : 1;

    static constexpr Kind Aligned(BaseTaggedness base_is_tagged) {
      switch (base_is_tagged) {
        case BaseTaggedness::kTaggedBase:
          return TaggedBase();
        case BaseTaggedness::kUntaggedBase:
          return RawAligned();
      }
    }
    static constexpr Kind TaggedBase() { return Kind{true, false, false}; }
    static constexpr Kind RawAligned() { return Kind{false, false, false}; }
    static constexpr Kind RawUnaligned() { return Kind{false, true, false}; }
    static constexpr Kind Protected() { return Kind{false, false, true}; }

    bool operator==(const Kind& other) const {
      return tagged_base == other.tagged_base &&
             maybe_unaligned == other.maybe_unaligned &&
             with_trap_handler == other.with_trap_handler;
    }
  };
  Kind kind;
  MemoryRepresentation loaded_rep;
  RegisterRepresentation result_rep;
  uint8_t element_size_log2;  // multiply index with 2^element_size_log2
  int32_t offset;             // add offset to scaled index

  OpProperties Properties() const {
    return kind.with_trap_handler ? OpProperties::ReadingAndCanAbort()
                                  : OpProperties::Reading();
  }
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(&result_rep, 1);
  }

  OpIndex base() const { return input(0); }
  OpIndex index() const {
    return input_count == 2 ? input(1) : OpIndex::Invalid();
  }

  LoadOp(OpIndex base, OpIndex index, Kind kind,
         MemoryRepresentation loaded_rep, RegisterRepresentation result_rep,
         int32_t offset, uint8_t element_size_log2)
      : Base(1 + index.valid()),
        kind(kind),
        loaded_rep(loaded_rep),
        result_rep(result_rep),
        element_size_log2(element_size_log2),
        offset(offset) {
    input(0) = base;
    if (index.valid()) {
      input(1) = index;
    }
  }

  void Validate(const Graph& graph) const {
    DCHECK(loaded_rep.ToRegisterRepresentation() == result_rep ||
           (loaded_rep.IsTagged() &&
            result_rep == RegisterRepresentation::Compressed()));
    DCHECK_IMPLIES(element_size_log2 > 0, index().valid());
    DCHECK(
        kind.tagged_base
            ? ValidOpInputRep(graph, base(), RegisterRepresentation::Tagged())
            : ValidOpInputRep(graph, base(),
                              {RegisterRepresentation::PointerSized(),
                               RegisterRepresentation::Tagged()}));
    if (index().valid()) {
      DCHECK(ValidOpInputRep(graph, index(),
                             RegisterRepresentation::PointerSized()));
    }
  }
  static LoadOp& New(Graph* graph, OpIndex base, OpIndex index, Kind kind,
                     MemoryRepresentation loaded_rep,
                     RegisterRepresentation result_rep, int32_t offset,
                     uint8_t element_size_log2) {
    return Base::New(graph, 1 + index.valid(), base, index, kind, loaded_rep,
                     result_rep, offset, element_size_log2);
  }
  void PrintInputs(std::ostream& os, const std::string& op_index_prefix) const;
  void PrintOptions(std::ostream& os) const;
  auto options() const {
    return std::tuple{kind, loaded_rep, result_rep, offset, element_size_log2};
  }
};

V8_INLINE size_t hash_value(LoadOp::Kind kind) {
  return base::hash_value(static_cast<int>(kind.tagged_base) |
                          (kind.maybe_unaligned << 1) |
                          (kind.with_trap_handler << 2));
}

// Store `value` to: base + offset + index * 2^element_size_log2.
// For Kind::tagged_base: subtract kHeapObjectTag,
//                        `base` has to be the object start.
struct StoreOp : OperationT<StoreOp> {
  using Kind = LoadOp::Kind;
  Kind kind;
  MemoryRepresentation stored_rep;
  WriteBarrierKind write_barrier;
  uint8_t element_size_log2;  // multiply index with 2^element_size_log2
  int32_t offset;             // add offset to scaled index

  OpProperties Properties() const {
    return kind.with_trap_handler ? OpProperties::WritingAndCanAbort()
                                  : OpProperties::Writing();
  }
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  OpIndex base() const { return input(0); }
  OpIndex value() const { return input(1); }
  OpIndex index() const {
    return input_count == 3 ? input(2) : OpIndex::Invalid();
  }

  StoreOp(OpIndex base, OpIndex index, OpIndex value, Kind kind,
          MemoryRepresentation stored_rep, WriteBarrierKind write_barrier,
          int32_t offset, uint8_t element_size_log2)
      : Base(2 + index.valid()),
        kind(kind),
        stored_rep(stored_rep),
        write_barrier(write_barrier),
        element_size_log2(element_size_log2),
        offset(offset) {
    input(0) = base;
    input(1) = value;
    if (index.valid()) {
      input(2) = index;
    }
  }

  void Validate(const Graph& graph) const {
    DCHECK_IMPLIES(element_size_log2 > 0, index().valid());
    DCHECK(
        kind.tagged_base
            ? ValidOpInputRep(graph, base(), RegisterRepresentation::Tagged())
            : ValidOpInputRep(graph, base(),
                              {RegisterRepresentation::PointerSized(),
                               RegisterRepresentation::Tagged()}));
    DCHECK(ValidOpInputRep(graph, value(),
                           stored_rep.ToRegisterRepresentationForStore()));
    if (index().valid()) {
      DCHECK(ValidOpInputRep(graph, index(),
                             RegisterRepresentation::PointerSized()));
    }
  }
  static StoreOp& New(Graph* graph, OpIndex base, OpIndex index, OpIndex value,
                      Kind kind, MemoryRepresentation stored_rep,
                      WriteBarrierKind write_barrier, int32_t offset,
                      uint8_t element_size_log2) {
    return Base::New(graph, 2 + index.valid(), base, index, value, kind,
                     stored_rep, write_barrier, offset, element_size_log2);
  }

  void PrintInputs(std::ostream& os, const std::string& op_index_prefix) const;
  void PrintOptions(std::ostream& os) const;
  auto options() const {
    return std::tuple{kind, stored_rep, write_barrier, offset,
                      element_size_log2};
  }
};

struct AllocateOp : FixedArityOperationT<1, AllocateOp> {
  AllocationType type;
  AllowLargeObjects allow_large_objects;

  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex size() const { return input(0); }

  AllocateOp(OpIndex size, AllocationType type,
             AllowLargeObjects allow_large_objects)
      : Base(size), type(type), allow_large_objects(allow_large_objects) {}

  void Validate(const Graph& graph) const {
    DCHECK(
        ValidOpInputRep(graph, size(), RegisterRepresentation::PointerSized()));
  }
  void PrintOptions(std::ostream& os) const;
  auto options() const { return std::tuple{type, allow_large_objects}; }
};

struct DecodeExternalPointerOp
    : FixedArityOperationT<1, DecodeExternalPointerOp> {
  ExternalPointerTag tag;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::PointerSized()>();
  }

  OpIndex handle() const { return input(0); }

  DecodeExternalPointerOp(OpIndex handle, ExternalPointerTag tag)
      : Base(handle), tag(tag) {}

  void Validate(const Graph& graph) const {
    DCHECK_NE(tag, kExternalPointerNullTag);
    DCHECK(ValidOpInputRep(graph, handle(), RegisterRepresentation::Word32()));
  }
  void PrintOptions(std::ostream& os) const;
  auto options() const { return std::tuple{tag}; }
};

// Retain a HeapObject to prevent it from being garbage collected too early.
struct RetainOp : FixedArityOperationT<1, RetainOp> {
  OpIndex retained() const { return input(0); }

  // Retain doesn't actually write, it just keeps a value alive. However, since
  // this must not be reordered with operations reading from the heap, we mark
  // it as writing to prevent such reorderings.
  static constexpr OpProperties properties = OpProperties::Writing();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  explicit RetainOp(OpIndex retained) : Base(retained) {}

  void Validate(const Graph& graph) const {
    DCHECK(
        ValidOpInputRep(graph, retained(), RegisterRepresentation::Tagged()));
  }
  auto options() const { return std::tuple{}; }
};

struct StackPointerGreaterThanOp
    : FixedArityOperationT<1, StackPointerGreaterThanOp> {
  StackCheckKind kind;

  static constexpr OpProperties properties = OpProperties::Reading();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Word32()>();
  }

  OpIndex stack_limit() const { return input(0); }

  StackPointerGreaterThanOp(OpIndex stack_limit, StackCheckKind kind)
      : Base(stack_limit), kind(kind) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, stack_limit(),
                           RegisterRepresentation::PointerSized()));
  }
  auto options() const { return std::tuple{kind}; }
};

// Allocate a piece of memory in the current stack frame. Every operation
// in the IR is a separate stack slot, but repeated execution in a loop
// produces the same stack slot.
struct StackSlotOp : FixedArityOperationT<0, StackSlotOp> {
  int size;
  int alignment;

  static constexpr OpProperties properties = OpProperties::Writing();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::PointerSized()>();
  }

  StackSlotOp(int size, int alignment) : size(size), alignment(alignment) {}
  void Validate(const Graph& graph) const {}
  auto options() const { return std::tuple{size, alignment}; }
};

// Values that are constant for the current stack frame/invocation.
// Therefore, they behaves like a constant, even though they are different for
// every invocation.
struct FrameConstantOp : FixedArityOperationT<0, FrameConstantOp> {
  enum class Kind { kStackCheckOffset, kFramePointer, kParentFramePointer };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    switch (kind) {
      case Kind::kStackCheckOffset:
        return RepVector<RegisterRepresentation::Tagged()>();
      case Kind::kFramePointer:
      case Kind::kParentFramePointer:
        return RepVector<RegisterRepresentation::PointerSized()>();
    }
  }

  explicit FrameConstantOp(Kind kind) : Base(), kind(kind) {}
  void Validate(const Graph& graph) const {}
  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, FrameConstantOp::Kind kind);

struct FrameStateOp : OperationT<FrameStateOp> {
  bool inlined;
  const FrameStateData* data;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  OpIndex parent_frame_state() const {
    DCHECK(inlined);
    return input(0);
  }
  base::Vector<const OpIndex> state_values() const {
    base::Vector<const OpIndex> result = inputs();
    if (inlined) result += 1;
    return result;
  }
  uint16_t state_values_count() const {
    DCHECK_EQ(input_count - inlined, state_values().size());
    return input_count - inlined;
  }
  const OpIndex state_value(size_t idx) const { return state_values()[idx]; }

  RegisterRepresentation state_value_rep(size_t idx) const {
    return RegisterRepresentation::FromMachineRepresentation(
        data->machine_types[idx].representation());
  }

  FrameStateOp(base::Vector<const OpIndex> inputs, bool inlined,
               const FrameStateData* data)
      : Base(inputs), inlined(inlined), data(data) {}

  void Validate(const Graph& graph) const {
    if (inlined) {
      DCHECK(Get(graph, parent_frame_state()).Is<FrameStateOp>());
    }
    // TODO(tebbi): Check frame state inputs using `FrameStateData`.
  }
  void PrintOptions(std::ostream& os) const;
  auto options() const { return std::tuple{inlined, data}; }
};

struct DeoptimizeOp : FixedArityOperationT<1, DeoptimizeOp> {
  const DeoptimizeParameters* parameters;

  static constexpr OpProperties properties = OpProperties::BlockTerminator();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  OpIndex frame_state() const { return input(0); }

  DeoptimizeOp(OpIndex frame_state, const DeoptimizeParameters* parameters)
      : Base(frame_state), parameters(parameters) {}
  void Validate(const Graph& graph) const {
    DCHECK(Get(graph, frame_state()).Is<FrameStateOp>());
  }
  auto options() const { return std::tuple{parameters}; }
};

struct DeoptimizeIfOp : FixedArityOperationT<2, DeoptimizeIfOp> {
  bool negated;
  const DeoptimizeParameters* parameters;

  static constexpr OpProperties properties = OpProperties::CanAbort();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  OpIndex condition() const { return input(0); }
  OpIndex frame_state() const { return input(1); }

  DeoptimizeIfOp(OpIndex condition, OpIndex frame_state, bool negated,
                 const DeoptimizeParameters* parameters)
      : Base(condition, frame_state),
        negated(negated),
        parameters(parameters) {}

  void Validate(const Graph& graph) const {
    DCHECK(
        ValidOpInputRep(graph, condition(), RegisterRepresentation::Word32()));
  }
  auto options() const { return std::tuple{negated, parameters}; }
};

struct TrapIfOp : FixedArityOperationT<1, TrapIfOp> {
  bool negated;
  const TrapId trap_id;

  static constexpr OpProperties properties = OpProperties::CanAbort();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  OpIndex condition() const { return input(0); }

  TrapIfOp(OpIndex condition, bool negated, const TrapId trap_id)
      : Base(condition), negated(negated), trap_id(trap_id) {}

  void Validate(const Graph& graph) const {
    DCHECK(
        ValidOpInputRep(graph, condition(), RegisterRepresentation::Word32()));
  }
  auto options() const { return std::tuple{negated, trap_id}; }
};

struct StaticAssertOp : FixedArityOperationT<1, StaticAssertOp> {
  static constexpr OpProperties properties = OpProperties::CanAbort();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }
  const char* source;

  OpIndex condition() const { return Base::input(0); }

  StaticAssertOp(OpIndex condition, const char* source)
      : Base(condition), source(source) {}

  void Validate(const Graph& graph) const {
    DCHECK(
        ValidOpInputRep(graph, condition(), RegisterRepresentation::Word32()));
  }
  auto options() const { return std::tuple{source}; }
};

struct ParameterOp : FixedArityOperationT<0, ParameterOp> {
  int32_t parameter_index;
  RegisterRepresentation rep;
  const char* debug_name;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return {&rep, 1};
  }

  explicit ParameterOp(int32_t parameter_index, RegisterRepresentation rep,
                       const char* debug_name = "")
      : Base(),
        parameter_index(parameter_index),
        rep(rep),
        debug_name(debug_name) {}
  void Validate(const Graph& graph) const {}
  auto options() const { return std::tuple{parameter_index, rep, debug_name}; }
  void PrintOptions(std::ostream& os) const;
};

struct OsrValueOp : FixedArityOperationT<0, OsrValueOp> {
  int32_t index;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  explicit OsrValueOp(int32_t index) : Base(), index(index) {}
  void Validate(const Graph& graph) const {}
  auto options() const { return std::tuple{index}; }
};

struct TSCallDescriptor : public NON_EXPORTED_BASE(ZoneObject) {
  const CallDescriptor* descriptor;
  base::Vector<const RegisterRepresentation> out_reps;

  TSCallDescriptor(const CallDescriptor* descriptor,
                   base::Vector<const RegisterRepresentation> out_reps)
      : descriptor(descriptor), out_reps(out_reps) {}

  static const TSCallDescriptor* Create(const CallDescriptor* descriptor,
                                        Zone* graph_zone) {
    base::Vector<RegisterRepresentation> out_reps =
        graph_zone->NewVector<RegisterRepresentation>(
            descriptor->ReturnCount());
    for (size_t i = 0; i < descriptor->ReturnCount(); ++i) {
      out_reps[i] = RegisterRepresentation::FromMachineRepresentation(
          descriptor->GetReturnType(i).representation());
    }
    return graph_zone->New<TSCallDescriptor>(descriptor, out_reps);
  }
};

struct CallOp : OperationT<CallOp> {
  const TSCallDescriptor* descriptor;

  static constexpr OpProperties properties = OpProperties::AnySideEffects();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return descriptor->out_reps;
  }

  bool HasFrameState() const {
    return descriptor->descriptor->NeedsFrameState();
  }

  OpIndex callee() const { return input(0); }
  OpIndex frame_state() const {
    return HasFrameState() ? input(1) : OpIndex::Invalid();
  }
  base::Vector<const OpIndex> arguments() const {
    return inputs().SubVector(1 + HasFrameState(), input_count);
  }

  CallOp(OpIndex callee, OpIndex frame_state,
         base::Vector<const OpIndex> arguments,
         const TSCallDescriptor* descriptor)
      : Base(1 + frame_state.valid() + arguments.size()),
        descriptor(descriptor) {
    base::Vector<OpIndex> inputs = this->inputs();
    inputs[0] = callee;
    if (frame_state.valid()) {
      inputs[1] = frame_state;
    }
    inputs.SubVector(1 + frame_state.valid(), inputs.size())
        .OverwriteWith(arguments);
  }
  void Validate(const Graph& graph) const {
    if (frame_state().valid()) {
      DCHECK(Get(graph, frame_state()).Is<FrameStateOp>());
    }
    // TODO(tebbi): Check call inputs based on `TSCallDescriptor`.
  }

  static CallOp& New(Graph* graph, OpIndex callee, OpIndex frame_state,
                     base::Vector<const OpIndex> arguments,
                     const TSCallDescriptor* descriptor) {
    return Base::New(graph, 1 + frame_state.valid() + arguments.size(), callee,
                     frame_state, arguments, descriptor);
  }
  auto options() const { return std::tuple{descriptor}; }
};

struct CallAndCatchExceptionOp : OperationT<CallAndCatchExceptionOp> {
  const TSCallDescriptor* descriptor;
  Block* if_success;
  Block* if_exception;

  static constexpr OpProperties properties =
      OpProperties::BlockTerminatorWithAnySideEffect();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return descriptor->out_reps;
  }

  bool HasFrameState() const {
    return descriptor->descriptor->NeedsFrameState();
  }

  OpIndex callee() const { return input(0); }
  OpIndex frame_state() const {
    return HasFrameState() ? input(1) : OpIndex::Invalid();
  }
  base::Vector<const OpIndex> arguments() const {
    return inputs().SubVector(1 + HasFrameState(), input_count);
  }

  CallAndCatchExceptionOp(OpIndex callee, OpIndex frame_state,
                          base::Vector<const OpIndex> arguments,
                          Block* if_success, Block* if_exception,
                          const TSCallDescriptor* descriptor)
      : Base(1 + frame_state.valid() + arguments.size()),
        descriptor(descriptor),
        if_success(if_success),
        if_exception(if_exception) {
    base::Vector<OpIndex> inputs = this->inputs();
    inputs[0] = callee;
    if (frame_state.valid()) {
      inputs[1] = frame_state;
    }
    inputs.SubVector(1 + frame_state.valid(), inputs.size())
        .OverwriteWith(arguments);
  }

  void Validate(const Graph& graph) const {
    if (frame_state().valid()) {
      DCHECK(Get(graph, frame_state()).Is<FrameStateOp>());
    }
  }

  static CallAndCatchExceptionOp& New(Graph* graph, OpIndex callee,
                                      OpIndex frame_state,
                                      base::Vector<const OpIndex> arguments,
                                      Block* if_success, Block* if_exception,
                                      const TSCallDescriptor* descriptor) {
    return Base::New(graph, 1 + frame_state.valid() + arguments.size(), callee,
                     frame_state, arguments, if_success, if_exception,
                     descriptor);
  }

  auto options() const {
    return std::tuple{descriptor, if_success, if_exception};
  }
};

struct LoadExceptionOp : FixedArityOperationT<0, LoadExceptionOp> {
  static constexpr OpProperties properties = OpProperties::Reading();

  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  LoadExceptionOp() : Base() {}
  void Validate(const Graph& graph) const {}

  auto options() const { return std::tuple{}; }
};

struct TailCallOp : OperationT<TailCallOp> {
  const TSCallDescriptor* descriptor;

  static constexpr OpProperties properties =
      OpProperties::BlockTerminatorWithAnySideEffect();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return descriptor->out_reps;
  }

  OpIndex callee() const { return input(0); }
  base::Vector<const OpIndex> arguments() const {
    return inputs().SubVector(1, input_count);
  }

  TailCallOp(OpIndex callee, base::Vector<const OpIndex> arguments,
             const TSCallDescriptor* descriptor)
      : Base(1 + arguments.size()), descriptor(descriptor) {
    base::Vector<OpIndex> inputs = this->inputs();
    inputs[0] = callee;
    inputs.SubVector(1, inputs.size()).OverwriteWith(arguments);
  }
  void Validate(const Graph& graph) const {}
  static TailCallOp& New(Graph* graph, OpIndex callee,
                         base::Vector<const OpIndex> arguments,
                         const TSCallDescriptor* descriptor) {
    return Base::New(graph, 1 + arguments.size(), callee, arguments,
                     descriptor);
  }
  auto options() const { return std::tuple{descriptor}; }
};

// Control-flow should never reach here.
struct UnreachableOp : FixedArityOperationT<0, UnreachableOp> {
  static constexpr OpProperties properties = OpProperties::BlockTerminator();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  UnreachableOp() : Base() {}
  void Validate(const Graph& graph) const {}
  auto options() const { return std::tuple{}; }
};

struct ReturnOp : OperationT<ReturnOp> {
  static constexpr OpProperties properties = OpProperties::BlockTerminator();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

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

  void Validate(const Graph& graph) const {
    DCHECK(
        ValidOpInputRep(graph, pop_count(), RegisterRepresentation::Word32()));
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
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  explicit GotoOp(Block* destination) : Base(), destination(destination) {}
  void Validate(const Graph& graph) const {}
  auto options() const { return std::tuple{destination}; }
};

struct BranchOp : FixedArityOperationT<1, BranchOp> {
  Block* if_true;
  Block* if_false;
  BranchHint hint;

  static constexpr OpProperties properties = OpProperties::BlockTerminator();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  OpIndex condition() const { return input(0); }

  BranchOp(OpIndex condition, Block* if_true, Block* if_false, BranchHint hint)
      : Base(condition), if_true(if_true), if_false(if_false), hint(hint) {}

  void Validate(const Graph& graph) const {
    DCHECK(
        ValidOpInputRep(graph, condition(), RegisterRepresentation::Word32()));
  }
  auto options() const { return std::tuple{if_true, if_false, hint}; }
};

struct SwitchOp : FixedArityOperationT<1, SwitchOp> {
  struct Case {
    int32_t value;
    Block* destination;
    BranchHint hint;

    Case(int32_t value, Block* destination, BranchHint hint)
        : value(value), destination(destination), hint(hint) {}

    bool operator==(const Case& other) const {
      return value == other.value && destination == other.destination &&
             hint == other.hint;
    }
  };
  base::Vector<const Case> cases;
  Block* default_case;
  BranchHint default_hint;

  static constexpr OpProperties properties = OpProperties::BlockTerminator();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  OpIndex input() const { return Base::input(0); }

  SwitchOp(OpIndex input, base::Vector<const Case> cases, Block* default_case,
           BranchHint default_hint)
      : Base(input),
        cases(cases),
        default_case(default_case),
        default_hint(default_hint) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), RegisterRepresentation::Word32()));
  }
  void PrintOptions(std::ostream& os) const;
  auto options() const { return std::tuple{cases, default_case, default_hint}; }
};

template <>
struct fast_hash<SwitchOp::Case> {
  size_t operator()(SwitchOp::Case v) {
    return fast_hash_combine(v.value, v.destination);
  }
};

inline base::SmallVector<Block*, 4> SuccessorBlocks(const Operation& op) {
  DCHECK(op.Properties().is_block_terminator);
  switch (op.opcode) {
    case Opcode::kCallAndCatchException: {
      auto& casted = op.Cast<CallAndCatchExceptionOp>();
      return {casted.if_success, casted.if_exception};
    }
    case Opcode::kGoto: {
      auto& casted = op.Cast<GotoOp>();
      return {casted.destination};
    }
    case Opcode::kBranch: {
      auto& casted = op.Cast<BranchOp>();
      return {casted.if_true, casted.if_false};
    }
    case Opcode::kReturn:
    case Opcode::kDeoptimize:
    case Opcode::kUnreachable:
      return base::SmallVector<Block*, 4>{};
    case Opcode::kSwitch: {
      auto& casted = op.Cast<SwitchOp>();
      base::SmallVector<Block*, 4> result;
      for (const SwitchOp::Case& c : casted.cases) {
        result.push_back(c.destination);
      }
      result.push_back(casted.default_case);
      return result;
    }
    default:
      UNREACHABLE();
  }
}

// Tuples are only used to lower operations with multiple outputs.
// `TupleOp` should be folded away by subsequent `ProjectionOp`s.
struct TupleOp : OperationT<TupleOp> {
  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  explicit TupleOp(base::Vector<const OpIndex> inputs) : Base(inputs) {}
  void Validate(const Graph& graph) const {}
  auto options() const { return std::tuple{}; }
};

// For operations that produce multiple results, we use `ProjectionOp` to
// distinguish them.
struct ProjectionOp : FixedArityOperationT<1, ProjectionOp> {
  uint16_t index;
  RegisterRepresentation rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(&rep, 1);
  }

  OpIndex input() const { return Base::input(0); }

  ProjectionOp(OpIndex input, uint16_t index, RegisterRepresentation rep)
      : Base(input), index(index), rep(rep) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), rep, index));
  }
  auto options() const { return std::tuple{index}; }
};

struct CheckTurboshaftTypeOfOp
    : FixedArityOperationT<1, CheckTurboshaftTypeOfOp> {
  RegisterRepresentation rep;
  Type type;
  bool successful;

  static constexpr OpProperties properties = OpProperties::AnySideEffects();
  base::Vector<const RegisterRepresentation> outputs_rep() const { return {}; }

  OpIndex input() const { return Base::input(0); }

  CheckTurboshaftTypeOfOp(OpIndex input, RegisterRepresentation rep, Type type,
                          bool successful)
      : Base(input), rep(rep), type(std::move(type)), successful(successful) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), rep));
  }
  auto options() const { return std::tuple{rep, type, successful}; }
};

struct ObjectIsOp : FixedArityOperationT<1, ObjectIsOp> {
  enum class Kind : uint8_t {
    kArrayBufferView,
    kBigInt,
    kBigInt64,
    kCallable,
    kConstructor,
    kDetectableCallable,
    kInternalizedString,
    kNonCallable,
    kNumber,
    kReceiver,
    kReceiverOrNullOrUndefined,
    kSmi,
    kString,
    kSymbol,
    kUndetectable,
  };
  enum class InputAssumptions : uint8_t {
    kNone,
    kHeapObject,
    kBigInt,
  };
  Kind kind;
  InputAssumptions input_assumptions;

  static constexpr OpProperties properties = OpProperties::Reading();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Word32()>();
  }

  OpIndex input() const { return Base::input(0); }

  ObjectIsOp(OpIndex input, Kind kind, InputAssumptions input_assumptions)
      : Base(input), kind(kind), input_assumptions(input_assumptions) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), RegisterRepresentation::Tagged()));
  }
  auto options() const { return std::tuple{kind, input_assumptions}; }
};
std::ostream& operator<<(std::ostream& os, ObjectIsOp::Kind kind);
std::ostream& operator<<(std::ostream& os,
                         ObjectIsOp::InputAssumptions input_assumptions);

struct FloatIsOp : FixedArityOperationT<1, FloatIsOp> {
  enum class Kind : uint8_t {
    kNaN,
  };
  Kind kind;
  FloatRepresentation input_rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Word32()>();
  }

  OpIndex input() const { return Base::input(0); }

  FloatIsOp(OpIndex input, Kind kind, FloatRepresentation input_rep)
      : Base(input), kind(kind), input_rep(input_rep) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), input_rep));
  }
  auto options() const { return std::tuple{kind, input_rep}; }
};
std::ostream& operator<<(std::ostream& os, FloatIsOp::Kind kind);

struct ConvertToObjectOp : FixedArityOperationT<1, ConvertToObjectOp> {
  enum class Kind : uint8_t {
    kBigInt,
    kBoolean,
    kHeapNumber,
    kNumber,
    kSmi,
    kString,
  };
  enum class InputInterpretation : uint8_t {
    kSigned,
    kUnsigned,
    kCharCode,
    kCodePoint,
  };
  Kind kind;
  RegisterRepresentation input_rep;
  InputInterpretation input_interpretation;
  CheckForMinusZeroMode minus_zero_mode;

  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex input() const { return Base::input(0); }

  ConvertToObjectOp(OpIndex input, Kind kind, RegisterRepresentation input_rep,
                    InputInterpretation input_interpretation,
                    CheckForMinusZeroMode minus_zero_mode)
      : Base(input),
        kind(kind),
        input_rep(input_rep),
        input_interpretation(input_interpretation),
        minus_zero_mode(minus_zero_mode) {}

  void Validate(const Graph& graph) const {
    switch (kind) {
      case Kind::kBigInt:
        DCHECK_EQ(input_rep, RegisterRepresentation::Word64());
        DCHECK(ValidOpInputRep(graph, input(), input_rep));
        DCHECK_EQ(minus_zero_mode,
                  CheckForMinusZeroMode::kDontCheckForMinusZero);
        break;
      case Kind::kBoolean:
        DCHECK_EQ(input_rep, RegisterRepresentation::Word32());
        DCHECK(ValidOpInputRep(graph, input(), input_rep));
        DCHECK_EQ(minus_zero_mode,
                  CheckForMinusZeroMode::kDontCheckForMinusZero);
        break;
      case Kind::kNumber:
      case Kind::kHeapNumber:
        DCHECK(ValidOpInputRep(graph, input(), input_rep));
        DCHECK_IMPLIES(
            minus_zero_mode == CheckForMinusZeroMode::kCheckForMinusZero,
            input_rep == RegisterRepresentation::Float64());
        break;
      case Kind::kSmi:
        DCHECK_EQ(input_rep, WordRepresentation::Word32());
        DCHECK_EQ(minus_zero_mode,
                  CheckForMinusZeroMode::kDontCheckForMinusZero);
        DCHECK(ValidOpInputRep(graph, input(), input_rep));
        break;
      case Kind::kString:
        DCHECK_EQ(input_rep, WordRepresentation::Word32());
        DCHECK_EQ(input_interpretation,
                  any_of(InputInterpretation::kCharCode,
                         InputInterpretation::kCodePoint));
        DCHECK(ValidOpInputRep(graph, input(), input_rep));
        break;
    }
  }

  auto options() const {
    return std::tuple{kind, input_rep, input_interpretation, minus_zero_mode};
  }
};
std::ostream& operator<<(std::ostream& os, ConvertToObjectOp::Kind kind);

struct ConvertToObjectOrDeoptOp
    : FixedArityOperationT<2, ConvertToObjectOrDeoptOp> {
  enum class Kind : uint8_t {
    kSmi,
  };
  enum class InputInterpretation : uint8_t {
    kSigned,
    kUnsigned,
  };
  Kind kind;
  RegisterRepresentation input_rep;
  InputInterpretation input_interpretation;
  FeedbackSource feedback;

  static constexpr OpProperties properties = OpProperties::CanAbort();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex input() const { return Base::input(0); }
  OpIndex frame_state() const { return Base::input(1); }

  ConvertToObjectOrDeoptOp(OpIndex input, OpIndex frame_state, Kind kind,
                           RegisterRepresentation input_rep,
                           InputInterpretation input_interpretation,
                           const FeedbackSource& feedback)
      : Base(input, frame_state),
        kind(kind),
        input_rep(input_rep),
        input_interpretation(input_interpretation),
        feedback(feedback) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), input_rep));
  }

  auto options() const {
    return std::tuple{kind, input_rep, input_interpretation, feedback};
  }
};
std::ostream& operator<<(std::ostream& os, ConvertToObjectOrDeoptOp::Kind kind);
std::ostream& operator<<(
    std::ostream& os,
    ConvertToObjectOrDeoptOp::InputInterpretation input_interpretation);

struct ConvertObjectToPrimitiveOp
    : FixedArityOperationT<1, ConvertObjectToPrimitiveOp> {
  enum class Kind : uint8_t {
    kInt32,
    kInt64,
    kUint32,
    kBit,
    kFloat64,
  };
  enum class InputAssumptions : uint8_t {
    kObject,
    kSmi,
    kNumberOrOddball,
  };
  Kind kind;
  InputAssumptions input_assumptions;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    switch (kind) {
      case Kind::kInt32:
      case Kind::kUint32:
      case Kind::kBit:
        return RepVector<RegisterRepresentation::Word32()>();
      case Kind::kInt64:
        return RepVector<RegisterRepresentation::Word64()>();
      case Kind::kFloat64:
        return RepVector<RegisterRepresentation::Float64()>();
    }
  }

  OpIndex input() const { return Base::input(0); }

  ConvertObjectToPrimitiveOp(OpIndex input, Kind kind,
                             InputAssumptions input_assumptions)
      : Base(input), kind(kind), input_assumptions(input_assumptions) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{kind, input_assumptions}; }
};
std::ostream& operator<<(std::ostream& os,
                         ConvertObjectToPrimitiveOp::Kind kind);
std::ostream& operator<<(
    std::ostream& os,
    ConvertObjectToPrimitiveOp::InputAssumptions input_assumptions);

struct ConvertObjectToPrimitiveOrDeoptOp
    : FixedArityOperationT<2, ConvertObjectToPrimitiveOrDeoptOp> {
  enum class PrimitiveKind : uint8_t {
    kInt32,
    kInt64,
    kFloat64,
    kArrayIndex,
  };
  enum class ObjectKind : uint8_t {
    kNumber,
    kNumberOrBoolean,
    kNumberOrOddball,
    kNumberOrString,
    kSmi,
  };
  ObjectKind from_kind;
  PrimitiveKind to_kind;
  CheckForMinusZeroMode minus_zero_mode;
  FeedbackSource feedback;

  static constexpr OpProperties properties = OpProperties::ReadingAndCanAbort();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    switch (to_kind) {
      case PrimitiveKind::kInt32:
        return RepVector<RegisterRepresentation::Word32()>();
      case PrimitiveKind::kInt64:
        return RepVector<RegisterRepresentation::Word64()>();
      case PrimitiveKind::kFloat64:
        return RepVector<RegisterRepresentation::Float64()>();
      case PrimitiveKind::kArrayIndex:
        return Is64() ? RepVector<RegisterRepresentation::Word64()>()
                      : RepVector<RegisterRepresentation::Word32()>();
    }
  }

  OpIndex input() const { return Base::input(0); }
  OpIndex frame_state() const { return Base::input(1); }

  ConvertObjectToPrimitiveOrDeoptOp(OpIndex input, OpIndex frame_state,
                                    ObjectKind from_kind, PrimitiveKind to_kind,
                                    CheckForMinusZeroMode minus_zero_mode,
                                    const FeedbackSource& feedback)
      : Base(input, frame_state),
        from_kind(from_kind),
        to_kind(to_kind),
        minus_zero_mode(minus_zero_mode),
        feedback(feedback) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), RegisterRepresentation::Tagged()));
    DCHECK(Get(graph, frame_state()).Is<FrameStateOp>());
  }

  auto options() const {
    return std::tuple{from_kind, to_kind, minus_zero_mode, feedback};
  }
};
std::ostream& operator<<(std::ostream& os,
                         ConvertObjectToPrimitiveOrDeoptOp::ObjectKind kind);
std::ostream& operator<<(std::ostream& os,
                         ConvertObjectToPrimitiveOrDeoptOp::PrimitiveKind kind);

struct TruncateObjectToPrimitiveOp
    : FixedArityOperationT<1, TruncateObjectToPrimitiveOp> {
  enum class Kind : uint8_t {
    kInt32,
    kInt64,
    kBit,
  };
  enum class InputAssumptions : uint8_t {
    kBigInt,
    kNumberOrOddball,
    kHeapObject,
    kObject,
  };
  Kind kind;
  InputAssumptions input_assumptions;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    switch (kind) {
      case Kind::kInt32:
      case Kind::kBit:
        return RepVector<RegisterRepresentation::Word32()>();
      case Kind::kInt64:
        return RepVector<RegisterRepresentation::Word64()>();
    }
  }

  OpIndex input() const { return Base::input(0); }

  TruncateObjectToPrimitiveOp(OpIndex input, Kind kind,
                              InputAssumptions input_assumptions)
      : Base(input), kind(kind), input_assumptions(input_assumptions) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{kind, input_assumptions}; }
};
std::ostream& operator<<(std::ostream& os,
                         TruncateObjectToPrimitiveOp::Kind kind);
std::ostream& operator<<(
    std::ostream& os,
    TruncateObjectToPrimitiveOp::InputAssumptions input_assumptions);

enum class TagKind {
  kSmiTag,
};
std::ostream& operator<<(std::ostream& os, TagKind kind);

struct TagOp : FixedArityOperationT<1, TagOp> {
  TagKind kind;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex input() const { return Base::input(0); }

  TagOp(OpIndex input, TagKind kind) : Base(input), kind(kind) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), RegisterRepresentation::Word32()));
  }

  auto options() const { return std::tuple{kind}; }
};

struct UntagOp : FixedArityOperationT<1, UntagOp> {
  TagKind kind;
  RegisterRepresentation rep;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return base::VectorOf(&rep, 1);
  }

  OpIndex input() const { return Base::input(0); }

  UntagOp(OpIndex input, TagKind kind, RegisterRepresentation rep)
      : Base(input), kind(kind), rep(rep) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{kind, rep}; }
};

struct NewConsStringOp : FixedArityOperationT<3, NewConsStringOp> {
  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex length() const { return Base::input(0); }
  OpIndex first() const { return Base::input(1); }
  OpIndex second() const { return Base::input(2); }

  NewConsStringOp(OpIndex length, OpIndex first, OpIndex second)
      : Base(length, first, second) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, length(), RegisterRepresentation::Word32()));
    DCHECK(ValidOpInputRep(graph, first(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, second(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{}; }
};

struct NewArrayOp : FixedArityOperationT<1, NewArrayOp> {
  enum class Kind : uint8_t {
    kDouble,
    kObject,
  };
  Kind kind;
  AllocationType allocation_type;

  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex length() const { return Base::input(0); }

  NewArrayOp(OpIndex length, Kind kind, AllocationType allocation_type)
      : Base(length), kind(kind), allocation_type(allocation_type) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, length(),
                           RegisterRepresentation::PointerSized()));
  }

  auto options() const { return std::tuple{kind, allocation_type}; }
};
std::ostream& operator<<(std::ostream& os, NewArrayOp::Kind kind);

struct DoubleArrayMinMaxOp : FixedArityOperationT<1, DoubleArrayMinMaxOp> {
  enum class Kind : uint8_t {
    kMin,
    kMax,
  };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex array() const { return Base::input(0); }

  DoubleArrayMinMaxOp(OpIndex array, Kind kind) : Base(array), kind(kind) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, array(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, DoubleArrayMinMaxOp::Kind kind);

// TODO(nicohartmann@): We should consider getting rid of the LoadFieldByIndex
// operation.
struct LoadFieldByIndexOp : FixedArityOperationT<2, LoadFieldByIndexOp> {
  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex object() const { return Base::input(0); }
  // Index encoding (see `src/objects/field-index-inl.h`):
  // For efficiency, the LoadByFieldIndex instruction takes an index that is
  // optimized for quick access. If the property is inline, the index is
  // positive. If it's out-of-line, the encoded index is -raw_index - 1 to
  // disambiguate the zero out-of-line index from the zero inobject case.
  // The index itself is shifted up by one bit, the lower-most bit
  // signifying if the field is a mutable double box (1) or not (0).
  OpIndex index() const { return Base::input(1); }

  LoadFieldByIndexOp(OpIndex object, OpIndex index) : Base(object, index) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, object(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, index(), RegisterRepresentation::Word32()));
  }

  auto options() const { return std::tuple{}; }
};

struct DebugBreakOp : FixedArityOperationT<0, DebugBreakOp> {
  static constexpr OpProperties properties = OpProperties::AnySideEffects();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<>();
  }

  DebugBreakOp() : Base() {}
  void Validate(const Graph& graph) const {}

  auto options() const { return std::tuple{}; }
};

struct BigIntBinopOp : FixedArityOperationT<3, BigIntBinopOp> {
  enum class Kind : uint8_t {
    kAdd,
    kSub,
    kMul,
    kDiv,
    kMod,
    kBitwiseAnd,
    kBitwiseOr,
    kBitwiseXor,
    kShiftLeft,
    kShiftRightArithmetic,
  };
  Kind kind;

  // TODO(nicohartmann@): Maybe we can specify more precise properties here.
  // These operations can deopt (abort), allocate and read immutable data.
  static constexpr OpProperties properties = OpProperties::AnySideEffects();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex left() const { return Base::input(0); }
  OpIndex right() const { return Base::input(1); }
  OpIndex frame_state() const { return Base::input(2); }

  BigIntBinopOp(OpIndex left, OpIndex right, OpIndex frame_state, Kind kind)
      : Base(left, right, frame_state), kind(kind) {}
  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, left(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, right(), RegisterRepresentation::Tagged()));
    DCHECK(Get(graph, frame_state()).Is<FrameStateOp>());
  }

  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, BigIntBinopOp::Kind kind);

struct BigIntEqualOp : FixedArityOperationT<2, BigIntEqualOp> {
  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex left() const { return Base::input(0); }
  OpIndex right() const { return Base::input(1); }

  BigIntEqualOp(OpIndex left, OpIndex right) : Base(left, right) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, left(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, right(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{}; }
};

struct BigIntComparisonOp : FixedArityOperationT<2, BigIntComparisonOp> {
  enum class Kind : uint8_t {
    kLessThan,
    kLessThanOrEqual,
  };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex left() const { return Base::input(0); }
  OpIndex right() const { return Base::input(1); }

  BigIntComparisonOp(OpIndex left, OpIndex right, Kind kind)
      : Base(left, right), kind(kind) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, left(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, right(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, BigIntComparisonOp::Kind kind);

struct BigIntUnaryOp : FixedArityOperationT<1, BigIntUnaryOp> {
  enum class Kind : uint8_t {
    kNegate,
  };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex input() const { return Base::input(0); }

  BigIntUnaryOp(OpIndex input, Kind kind) : Base(input), kind(kind) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, input(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, BigIntUnaryOp::Kind kind);

struct LoadRootRegisterOp : FixedArityOperationT<0, LoadRootRegisterOp> {
  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::PointerSized()>();
  }

  LoadRootRegisterOp() : Base() {}
  void Validate(const Graph& graph) const {}
  std::tuple<> options() const { return {}; }
};

struct StringAtOp : FixedArityOperationT<2, StringAtOp> {
  enum class Kind : uint8_t {
    kCharCode,
    kCodePoint,
  };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Word32()>();
  }

  OpIndex string() const { return Base::input(0); }
  OpIndex position() const { return Base::input(1); }

  StringAtOp(OpIndex string, OpIndex position, Kind kind)
      : Base(string, position), kind(kind) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, string(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, position(),
                           RegisterRepresentation::PointerSized()));
  }

  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, StringAtOp::Kind kind);

#ifdef V8_INTL_SUPPORT
struct StringToCaseIntlOp : FixedArityOperationT<1, StringToCaseIntlOp> {
  enum class Kind : uint8_t {
    kLower,
    kUpper,
  };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex string() const { return Base::input(0); }

  StringToCaseIntlOp(OpIndex string, Kind kind) : Base(string), kind(kind) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, string(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, StringToCaseIntlOp::Kind kind);
#endif  // V8_INTL_SUPPORT

struct StringLengthOp : FixedArityOperationT<1, StringLengthOp> {
  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Word32()>();
  }

  OpIndex string() const { return Base::input(0); }

  explicit StringLengthOp(OpIndex string) : Base(string) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, string(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{}; }
};

struct StringIndexOfOp : FixedArityOperationT<3, StringIndexOfOp> {
  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  // Search the string `search` within the string `string` starting at
  // `position`.
  OpIndex string() const { return Base::input(0); }
  OpIndex search() const { return Base::input(1); }
  OpIndex position() const { return Base::input(2); }

  StringIndexOfOp(OpIndex string, OpIndex search, OpIndex position)
      : Base(string, search, position) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, string(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, search(), RegisterRepresentation::Tagged()));
    DCHECK(
        ValidOpInputRep(graph, position(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{}; }
};

struct StringFromCodePointAtOp
    : FixedArityOperationT<2, StringFromCodePointAtOp> {
  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex string() const { return Base::input(0); }
  OpIndex index() const { return Base::input(1); }

  StringFromCodePointAtOp(OpIndex string, OpIndex index)
      : Base(string, index) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, string(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, index(),
                           RegisterRepresentation::PointerSized()));
  }

  auto options() const { return std::tuple{}; }
};

struct StringSubstringOp : FixedArityOperationT<3, StringSubstringOp> {
  static constexpr OpProperties properties = OpProperties::PureMayAllocate();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex string() const { return Base::input(0); }
  OpIndex start() const { return Base::input(1); }
  OpIndex end() const { return Base::input(2); }

  StringSubstringOp(OpIndex string, OpIndex start, OpIndex end)
      : Base(string, start, end) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, string(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, start(), RegisterRepresentation::Word32()));
    DCHECK(ValidOpInputRep(graph, end(), RegisterRepresentation::Word32()));
  }

  auto options() const { return std::tuple{}; }
};

struct StringEqualOp : FixedArityOperationT<2, StringEqualOp> {
  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex left() const { return Base::input(0); }
  OpIndex right() const { return Base::input(1); }

  StringEqualOp(OpIndex left, OpIndex right) : Base(left, right) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, left(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, right(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{}; }
};

struct StringComparisonOp : FixedArityOperationT<2, StringComparisonOp> {
  enum class Kind : uint8_t {
    kLessThan,
    kLessThanOrEqual,
  };
  Kind kind;

  static constexpr OpProperties properties = OpProperties::PureNoAllocation();
  base::Vector<const RegisterRepresentation> outputs_rep() const {
    return RepVector<RegisterRepresentation::Tagged()>();
  }

  OpIndex left() const { return Base::input(0); }
  OpIndex right() const { return Base::input(1); }

  StringComparisonOp(OpIndex left, OpIndex right, Kind kind)
      : Base(left, right), kind(kind) {}

  void Validate(const Graph& graph) const {
    DCHECK(ValidOpInputRep(graph, left(), RegisterRepresentation::Tagged()));
    DCHECK(ValidOpInputRep(graph, right(), RegisterRepresentation::Tagged()));
  }

  auto options() const { return std::tuple{kind}; }
};
std::ostream& operator<<(std::ostream& os, StringComparisonOp::Kind kind);

#define OPERATION_PROPERTIES_CASE(Name) Name##Op::PropertiesIfStatic(),
static constexpr base::Optional<OpProperties>
    kOperationPropertiesTable[kNumberOfOpcodes] = {
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

inline OpProperties Operation::Properties() const {
  if (auto prop = kOperationPropertiesTable[OpcodeIndex(opcode)]) {
    return *prop;
  }
  switch (opcode) {
    case Opcode::kLoad:
      return Cast<LoadOp>().Properties();
    case Opcode::kStore:
      return Cast<StoreOp>().Properties();
    default:
      UNREACHABLE();
  }
}

// static
inline size_t Operation::StorageSlotCount(Opcode opcode, size_t input_count) {
  size_t size = kOperationSizeDividedBySizeofOpIndexTable[OpcodeIndex(opcode)];
  constexpr size_t r = sizeof(OperationStorageSlot) / sizeof(OpIndex);
  static_assert(sizeof(OperationStorageSlot) % sizeof(OpIndex) == 0);
  return std::max<size_t>(2, (r - 1 + size + input_count) / r);
}

template <class Op>
V8_INLINE bool CanBeUsedAsInput(const Op& op) {
  if (std::is_same<Op, FrameStateOp>::value) {
    // FrameStateOp is the only Operation that can be used as an input but has
    // empty `outputs_rep`.
    return true;
  }
  // For all other Operations, they can only be used as an input if they have at
  // least one output.
  return op.outputs_rep().size() > 0;
}

inline base::Vector<const RegisterRepresentation> Operation::outputs_rep()
    const {
  switch (opcode) {
#define CASE(type)                         \
  case Opcode::k##type: {                  \
    const type##Op& op = Cast<type##Op>(); \
    return op.outputs_rep();               \
  }
    TURBOSHAFT_OPERATION_LIST(CASE)
#undef CASE
  }
}

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_OPERATIONS_H_
