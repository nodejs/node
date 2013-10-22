// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_HYDROGEN_INSTRUCTIONS_H_
#define V8_HYDROGEN_INSTRUCTIONS_H_

#include "v8.h"

#include "allocation.h"
#include "code-stubs.h"
#include "data-flow.h"
#include "deoptimizer.h"
#include "small-pointer-list.h"
#include "string-stream.h"
#include "v8conversions.h"
#include "v8utils.h"
#include "zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class HBasicBlock;
class HEnvironment;
class HInferRepresentationPhase;
class HInstruction;
class HLoopInformation;
class HStoreNamedField;
class HValue;
class LInstruction;
class LChunkBuilder;

#define HYDROGEN_ABSTRACT_INSTRUCTION_LIST(V)  \
  V(ArithmeticBinaryOperation)                 \
  V(BinaryOperation)                           \
  V(BitwiseBinaryOperation)                    \
  V(ControlInstruction)                        \
  V(Instruction)                               \


#define HYDROGEN_CONCRETE_INSTRUCTION_LIST(V)  \
  V(AccessArgumentsAt)                         \
  V(Add)                                       \
  V(Allocate)                                  \
  V(ApplyArguments)                            \
  V(ArgumentsElements)                         \
  V(ArgumentsLength)                           \
  V(ArgumentsObject)                           \
  V(Bitwise)                                   \
  V(BlockEntry)                                \
  V(BoundsCheck)                               \
  V(BoundsCheckBaseIndexInformation)           \
  V(Branch)                                    \
  V(CallConstantFunction)                      \
  V(CallFunction)                              \
  V(CallGlobal)                                \
  V(CallKeyed)                                 \
  V(CallKnownGlobal)                           \
  V(CallNamed)                                 \
  V(CallNew)                                   \
  V(CallNewArray)                              \
  V(CallRuntime)                               \
  V(CallStub)                                  \
  V(CapturedObject)                            \
  V(Change)                                    \
  V(CheckHeapObject)                           \
  V(CheckInstanceType)                         \
  V(CheckMaps)                                 \
  V(CheckMapValue)                             \
  V(CheckSmi)                                  \
  V(CheckValue)                                \
  V(ClampToUint8)                              \
  V(ClassOfTestAndBranch)                      \
  V(CompareNumericAndBranch)                   \
  V(CompareHoleAndBranch)                      \
  V(CompareGeneric)                            \
  V(CompareObjectEqAndBranch)                  \
  V(CompareMap)                                \
  V(Constant)                                  \
  V(Context)                                   \
  V(DateField)                                 \
  V(DebugBreak)                                \
  V(DeclareGlobals)                            \
  V(Deoptimize)                                \
  V(Div)                                       \
  V(DummyUse)                                  \
  V(ElementsKind)                              \
  V(EnterInlined)                              \
  V(EnvironmentMarker)                         \
  V(ForceRepresentation)                       \
  V(ForInCacheArray)                           \
  V(ForInPrepareMap)                           \
  V(FunctionLiteral)                           \
  V(GetCachedArrayIndex)                       \
  V(GlobalObject)                              \
  V(GlobalReceiver)                            \
  V(Goto)                                      \
  V(HasCachedArrayIndexAndBranch)              \
  V(HasInstanceTypeAndBranch)                  \
  V(InnerAllocatedObject)                      \
  V(InstanceOf)                                \
  V(InstanceOfKnownGlobal)                     \
  V(InstanceSize)                              \
  V(InvokeFunction)                            \
  V(IsConstructCallAndBranch)                  \
  V(IsObjectAndBranch)                         \
  V(IsNumberAndBranch)                         \
  V(IsStringAndBranch)                         \
  V(IsSmiAndBranch)                            \
  V(IsUndetectableAndBranch)                   \
  V(LeaveInlined)                              \
  V(LoadContextSlot)                           \
  V(LoadExternalArrayPointer)                  \
  V(LoadFieldByIndex)                          \
  V(LoadFunctionPrototype)                     \
  V(LoadGlobalCell)                            \
  V(LoadGlobalGeneric)                         \
  V(LoadKeyed)                                 \
  V(LoadKeyedGeneric)                          \
  V(LoadNamedField)                            \
  V(LoadNamedGeneric)                          \
  V(MapEnumLength)                             \
  V(MathFloorOfDiv)                            \
  V(MathMinMax)                                \
  V(Mod)                                       \
  V(Mul)                                       \
  V(OsrEntry)                                  \
  V(OuterContext)                              \
  V(Parameter)                                 \
  V(Power)                                     \
  V(PushArgument)                              \
  V(Random)                                    \
  V(RegExpLiteral)                             \
  V(Return)                                    \
  V(Ror)                                       \
  V(Sar)                                       \
  V(SeqStringSetChar)                          \
  V(Shl)                                       \
  V(Shr)                                       \
  V(Simulate)                                  \
  V(StackCheck)                                \
  V(StoreCodeEntry)                            \
  V(StoreContextSlot)                          \
  V(StoreGlobalCell)                           \
  V(StoreGlobalGeneric)                        \
  V(StoreKeyed)                                \
  V(StoreKeyedGeneric)                         \
  V(StoreNamedField)                           \
  V(StoreNamedGeneric)                         \
  V(StringAdd)                                 \
  V(StringCharCodeAt)                          \
  V(StringCharFromCode)                        \
  V(StringCompareAndBranch)                    \
  V(Sub)                                       \
  V(ThisFunction)                              \
  V(Throw)                                     \
  V(ToFastProperties)                          \
  V(TransitionElementsKind)                    \
  V(TrapAllocationMemento)                     \
  V(Typeof)                                    \
  V(TypeofIsAndBranch)                         \
  V(UnaryMathOperation)                        \
  V(UnknownOSRValue)                           \
  V(UseConst)                                  \
  V(ValueOf)                                   \
  V(WrapReceiver)

#define GVN_TRACKED_FLAG_LIST(V)               \
  V(Maps)                                      \
  V(NewSpacePromotion)

#define GVN_UNTRACKED_FLAG_LIST(V)             \
  V(ArrayElements)                             \
  V(ArrayLengths)                              \
  V(StringLengths)                             \
  V(BackingStoreFields)                        \
  V(Calls)                                     \
  V(ContextSlots)                              \
  V(DoubleArrayElements)                       \
  V(DoubleFields)                              \
  V(ElementsKind)                              \
  V(ElementsPointer)                           \
  V(GlobalVars)                                \
  V(InobjectFields)                            \
  V(OsrEntries)                                \
  V(ExternalMemory)


#define DECLARE_ABSTRACT_INSTRUCTION(type)                              \
  virtual bool Is##type() const V8_FINAL V8_OVERRIDE { return true; }   \
  static H##type* cast(HValue* value) {                                 \
    ASSERT(value->Is##type());                                          \
    return reinterpret_cast<H##type*>(value);                           \
  }


#define DECLARE_CONCRETE_INSTRUCTION(type)              \
  virtual LInstruction* CompileToLithium(               \
     LChunkBuilder* builder) V8_FINAL V8_OVERRIDE;      \
  static H##type* cast(HValue* value) {                 \
    ASSERT(value->Is##type());                          \
    return reinterpret_cast<H##type*>(value);           \
  }                                                     \
  virtual Opcode opcode() const V8_FINAL V8_OVERRIDE {  \
    return HValue::k##type;                             \
  }


class Range V8_FINAL : public ZoneObject {
 public:
  Range()
      : lower_(kMinInt),
        upper_(kMaxInt),
        next_(NULL),
        can_be_minus_zero_(false) { }

  Range(int32_t lower, int32_t upper)
      : lower_(lower),
        upper_(upper),
        next_(NULL),
        can_be_minus_zero_(false) { }

  int32_t upper() const { return upper_; }
  int32_t lower() const { return lower_; }
  Range* next() const { return next_; }
  Range* CopyClearLower(Zone* zone) const {
    return new(zone) Range(kMinInt, upper_);
  }
  Range* CopyClearUpper(Zone* zone) const {
    return new(zone) Range(lower_, kMaxInt);
  }
  Range* Copy(Zone* zone) const {
    Range* result = new(zone) Range(lower_, upper_);
    result->set_can_be_minus_zero(CanBeMinusZero());
    return result;
  }
  int32_t Mask() const;
  void set_can_be_minus_zero(bool b) { can_be_minus_zero_ = b; }
  bool CanBeMinusZero() const { return CanBeZero() && can_be_minus_zero_; }
  bool CanBeZero() const { return upper_ >= 0 && lower_ <= 0; }
  bool CanBeNegative() const { return lower_ < 0; }
  bool CanBePositive() const { return upper_ > 0; }
  bool Includes(int value) const { return lower_ <= value && upper_ >= value; }
  bool IsMostGeneric() const {
    return lower_ == kMinInt && upper_ == kMaxInt && CanBeMinusZero();
  }
  bool IsInSmiRange() const {
    return lower_ >= Smi::kMinValue && upper_ <= Smi::kMaxValue;
  }
  void ClampToSmi() {
    lower_ = Max(lower_, Smi::kMinValue);
    upper_ = Min(upper_, Smi::kMaxValue);
  }
  void KeepOrder();
#ifdef DEBUG
  void Verify() const;
#endif

  void StackUpon(Range* other) {
    Intersect(other);
    next_ = other;
  }

  void Intersect(Range* other);
  void Union(Range* other);
  void CombinedMax(Range* other);
  void CombinedMin(Range* other);

  void AddConstant(int32_t value);
  void Sar(int32_t value);
  void Shl(int32_t value);
  bool AddAndCheckOverflow(const Representation& r, Range* other);
  bool SubAndCheckOverflow(const Representation& r, Range* other);
  bool MulAndCheckOverflow(const Representation& r, Range* other);

 private:
  int32_t lower_;
  int32_t upper_;
  Range* next_;
  bool can_be_minus_zero_;
};


class UniqueValueId V8_FINAL {
 public:
  UniqueValueId() : raw_address_(NULL) { }

  explicit UniqueValueId(Handle<Object> handle) {
    ASSERT(!AllowHeapAllocation::IsAllowed());
    static const Address kEmptyHandleSentinel = reinterpret_cast<Address>(1);
    if (handle.is_null()) {
      raw_address_ = kEmptyHandleSentinel;
    } else {
      raw_address_ = reinterpret_cast<Address>(*handle);
      ASSERT_NE(kEmptyHandleSentinel, raw_address_);
    }
    ASSERT(IsInitialized());
  }

  bool IsInitialized() const { return raw_address_ != NULL; }

  bool operator==(const UniqueValueId& other) const {
    ASSERT(IsInitialized() && other.IsInitialized());
    return raw_address_ == other.raw_address_;
  }

  bool operator!=(const UniqueValueId& other) const {
    ASSERT(IsInitialized() && other.IsInitialized());
    return raw_address_ != other.raw_address_;
  }

  intptr_t Hashcode() const {
    ASSERT(IsInitialized());
    return reinterpret_cast<intptr_t>(raw_address_);
  }

#define IMMOVABLE_UNIQUE_VALUE_ID(name)   \
  static UniqueValueId name(Heap* heap) { return UniqueValueId(heap->name()); }

  IMMOVABLE_UNIQUE_VALUE_ID(free_space_map)
  IMMOVABLE_UNIQUE_VALUE_ID(minus_zero_value)
  IMMOVABLE_UNIQUE_VALUE_ID(nan_value)
  IMMOVABLE_UNIQUE_VALUE_ID(undefined_value)
  IMMOVABLE_UNIQUE_VALUE_ID(null_value)
  IMMOVABLE_UNIQUE_VALUE_ID(true_value)
  IMMOVABLE_UNIQUE_VALUE_ID(false_value)
  IMMOVABLE_UNIQUE_VALUE_ID(the_hole_value)
  IMMOVABLE_UNIQUE_VALUE_ID(empty_string)

#undef IMMOVABLE_UNIQUE_VALUE_ID

 private:
  Address raw_address_;

  explicit UniqueValueId(Object* object) {
    raw_address_ = reinterpret_cast<Address>(object);
    ASSERT(IsInitialized());
  }
};


class HType V8_FINAL {
 public:
  static HType None() { return HType(kNone); }
  static HType Tagged() { return HType(kTagged); }
  static HType TaggedPrimitive() { return HType(kTaggedPrimitive); }
  static HType TaggedNumber() { return HType(kTaggedNumber); }
  static HType Smi() { return HType(kSmi); }
  static HType HeapNumber() { return HType(kHeapNumber); }
  static HType String() { return HType(kString); }
  static HType Boolean() { return HType(kBoolean); }
  static HType NonPrimitive() { return HType(kNonPrimitive); }
  static HType JSArray() { return HType(kJSArray); }
  static HType JSObject() { return HType(kJSObject); }

  // Return the weakest (least precise) common type.
  HType Combine(HType other) {
    return HType(static_cast<Type>(type_ & other.type_));
  }

  bool Equals(const HType& other) const {
    return type_ == other.type_;
  }

  bool IsSubtypeOf(const HType& other) {
    return Combine(other).Equals(other);
  }

  bool IsTaggedPrimitive() const {
    return ((type_ & kTaggedPrimitive) == kTaggedPrimitive);
  }

  bool IsTaggedNumber() const {
    return ((type_ & kTaggedNumber) == kTaggedNumber);
  }

  bool IsSmi() const {
    return ((type_ & kSmi) == kSmi);
  }

  bool IsHeapNumber() const {
    return ((type_ & kHeapNumber) == kHeapNumber);
  }

  bool IsString() const {
    return ((type_ & kString) == kString);
  }

  bool IsNonString() const {
    return IsTaggedPrimitive() || IsSmi() || IsHeapNumber() ||
        IsBoolean() || IsJSArray();
  }

  bool IsBoolean() const {
    return ((type_ & kBoolean) == kBoolean);
  }

  bool IsNonPrimitive() const {
    return ((type_ & kNonPrimitive) == kNonPrimitive);
  }

  bool IsJSArray() const {
    return ((type_ & kJSArray) == kJSArray);
  }

  bool IsJSObject() const {
    return ((type_ & kJSObject) == kJSObject);
  }

  bool IsHeapObject() const {
    return IsHeapNumber() || IsString() || IsBoolean() || IsNonPrimitive();
  }

  bool ToStringOrToNumberCanBeObserved(Representation representation) {
    switch (type_) {
      case kTaggedPrimitive:  // fallthru
      case kTaggedNumber:     // fallthru
      case kSmi:              // fallthru
      case kHeapNumber:       // fallthru
      case kString:           // fallthru
      case kBoolean:
        return false;
      case kJSArray:          // fallthru
      case kJSObject:
        return true;
      case kTagged:
        break;
    }
    return !representation.IsSmiOrInteger32() && !representation.IsDouble();
  }

  static HType TypeFromValue(Handle<Object> value);

  const char* ToString();

 private:
  enum Type {
    kNone = 0x0,             // 0000 0000 0000 0000
    kTagged = 0x1,           // 0000 0000 0000 0001
    kTaggedPrimitive = 0x5,  // 0000 0000 0000 0101
    kTaggedNumber = 0xd,     // 0000 0000 0000 1101
    kSmi = 0x1d,             // 0000 0000 0001 1101
    kHeapNumber = 0x2d,      // 0000 0000 0010 1101
    kString = 0x45,          // 0000 0000 0100 0101
    kBoolean = 0x85,         // 0000 0000 1000 0101
    kNonPrimitive = 0x101,   // 0000 0001 0000 0001
    kJSObject = 0x301,       // 0000 0011 0000 0001
    kJSArray = 0x701         // 0000 0111 0000 0001
  };

  // Make sure type fits in int16.
  STATIC_ASSERT(kJSArray < (1 << (2 * kBitsPerByte)));

  explicit HType(Type t) : type_(t) { }

  int16_t type_;
};


class HUseListNode: public ZoneObject {
 public:
  HUseListNode(HValue* value, int index, HUseListNode* tail)
      : tail_(tail), value_(value), index_(index) {
  }

  HUseListNode* tail();
  HValue* value() const { return value_; }
  int index() const { return index_; }

  void set_tail(HUseListNode* list) { tail_ = list; }

#ifdef DEBUG
  void Zap() {
    tail_ = reinterpret_cast<HUseListNode*>(1);
    value_ = NULL;
    index_ = -1;
  }
#endif

 private:
  HUseListNode* tail_;
  HValue* value_;
  int index_;
};


// We reuse use list nodes behind the scenes as uses are added and deleted.
// This class is the safe way to iterate uses while deleting them.
class HUseIterator V8_FINAL BASE_EMBEDDED {
 public:
  bool Done() { return current_ == NULL; }
  void Advance();

  HValue* value() {
    ASSERT(!Done());
    return value_;
  }

  int index() {
    ASSERT(!Done());
    return index_;
  }

 private:
  explicit HUseIterator(HUseListNode* head);

  HUseListNode* current_;
  HUseListNode* next_;
  HValue* value_;
  int index_;

  friend class HValue;
};


// There must be one corresponding kDepends flag for every kChanges flag and
// the order of the kChanges flags must be exactly the same as of the kDepends
// flags. All tracked flags should appear before untracked ones.
enum GVNFlag {
  // Declare global value numbering flags.
#define DECLARE_FLAG(type) kChanges##type, kDependsOn##type,
  GVN_TRACKED_FLAG_LIST(DECLARE_FLAG)
  GVN_UNTRACKED_FLAG_LIST(DECLARE_FLAG)
#undef DECLARE_FLAG
  kAfterLastFlag,
  kLastFlag = kAfterLastFlag - 1,
#define COUNT_FLAG(type) + 1
  kNumberOfTrackedSideEffects = 0 GVN_TRACKED_FLAG_LIST(COUNT_FLAG)
#undef COUNT_FLAG
};


class DecompositionResult V8_FINAL BASE_EMBEDDED {
 public:
  DecompositionResult() : base_(NULL), offset_(0), scale_(0) {}

  HValue* base() { return base_; }
  int offset() { return offset_; }
  int scale() { return scale_; }

  bool Apply(HValue* other_base, int other_offset, int other_scale = 0) {
    if (base_ == NULL) {
      base_ = other_base;
      offset_ = other_offset;
      scale_ = other_scale;
      return true;
    } else {
      if (scale_ == 0) {
        base_ = other_base;
        offset_ += other_offset;
        scale_ = other_scale;
        return true;
      } else {
        return false;
      }
    }
  }

  void SwapValues(HValue** other_base, int* other_offset, int* other_scale) {
    swap(&base_, other_base);
    swap(&offset_, other_offset);
    swap(&scale_, other_scale);
  }

 private:
  template <class T> void swap(T* a, T* b) {
    T c(*a);
    *a = *b;
    *b = c;
  }

  HValue* base_;
  int offset_;
  int scale_;
};


typedef EnumSet<GVNFlag> GVNFlagSet;


class HValue : public ZoneObject {
 public:
  static const int kNoNumber = -1;

  enum Flag {
    kFlexibleRepresentation,
    kCannotBeTagged,
    // Participate in Global Value Numbering, i.e. elimination of
    // unnecessary recomputations. If an instruction sets this flag, it must
    // implement DataEquals(), which will be used to determine if other
    // occurrences of the instruction are indeed the same.
    kUseGVN,
    // Track instructions that are dominating side effects. If an instruction
    // sets this flag, it must implement HandleSideEffectDominator() and should
    // indicate which side effects to track by setting GVN flags.
    kTrackSideEffectDominators,
    kCanOverflow,
    kBailoutOnMinusZero,
    kCanBeDivByZero,
    kAllowUndefinedAsNaN,
    kIsArguments,
    kTruncatingToInt32,
    kAllUsesTruncatingToInt32,
    kTruncatingToSmi,
    kAllUsesTruncatingToSmi,
    // Set after an instruction is killed.
    kIsDead,
    // Instructions that are allowed to produce full range unsigned integer
    // values are marked with kUint32 flag. If arithmetic shift or a load from
    // EXTERNAL_UNSIGNED_INT_ELEMENTS array is not marked with this flag
    // it will deoptimize if result does not fit into signed integer range.
    // HGraph::ComputeSafeUint32Operations is responsible for setting this
    // flag.
    kUint32,
    kHasNoObservableSideEffects,
    // Indicates the instruction is live during dead code elimination.
    kIsLive,

    // HEnvironmentMarkers are deleted before dead code
    // elimination takes place, so they can repurpose the kIsLive flag:
    kEndsLiveRange = kIsLive,

    // TODO(everyone): Don't forget to update this!
    kLastFlag = kIsLive
  };

  STATIC_ASSERT(kLastFlag < kBitsPerInt);

  static const int kChangesToDependsFlagsLeftShift = 1;

  static GVNFlag ChangesFlagFromInt(int x) {
    return static_cast<GVNFlag>(x * 2);
  }
  static GVNFlag DependsOnFlagFromInt(int x) {
    return static_cast<GVNFlag>(x * 2 + 1);
  }
  static GVNFlagSet ConvertChangesToDependsFlags(GVNFlagSet flags) {
    return GVNFlagSet(flags.ToIntegral() << kChangesToDependsFlagsLeftShift);
  }

  static HValue* cast(HValue* value) { return value; }

  enum Opcode {
    // Declare a unique enum value for each hydrogen instruction.
  #define DECLARE_OPCODE(type) k##type,
    HYDROGEN_CONCRETE_INSTRUCTION_LIST(DECLARE_OPCODE)
    kPhi
  #undef DECLARE_OPCODE
  };
  virtual Opcode opcode() const = 0;

  // Declare a non-virtual predicates for each concrete HInstruction or HValue.
  #define DECLARE_PREDICATE(type) \
    bool Is##type() const { return opcode() == k##type; }
    HYDROGEN_CONCRETE_INSTRUCTION_LIST(DECLARE_PREDICATE)
  #undef DECLARE_PREDICATE
    bool IsPhi() const { return opcode() == kPhi; }

  // Declare virtual predicates for abstract HInstruction or HValue
  #define DECLARE_PREDICATE(type) \
    virtual bool Is##type() const { return false; }
    HYDROGEN_ABSTRACT_INSTRUCTION_LIST(DECLARE_PREDICATE)
  #undef DECLARE_PREDICATE

  HValue(HType type = HType::Tagged())
      : block_(NULL),
        id_(kNoNumber),
        type_(type),
        use_list_(NULL),
        range_(NULL),
        flags_(0) {}
  virtual ~HValue() {}

  HBasicBlock* block() const { return block_; }
  void SetBlock(HBasicBlock* block);
  int LoopWeight() const;

  // Note: Never call this method for an unlinked value.
  Isolate* isolate() const;

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  HUseIterator uses() const { return HUseIterator(use_list_); }

  virtual bool EmitAtUses() { return false; }

  Representation representation() const { return representation_; }
  void ChangeRepresentation(Representation r) {
    ASSERT(CheckFlag(kFlexibleRepresentation));
    ASSERT(!CheckFlag(kCannotBeTagged) || !r.IsTagged());
    RepresentationChanged(r);
    representation_ = r;
    if (r.IsTagged()) {
      // Tagged is the bottom of the lattice, don't go any further.
      ClearFlag(kFlexibleRepresentation);
    }
  }
  virtual void AssumeRepresentation(Representation r);

  virtual Representation KnownOptimalRepresentation() {
    Representation r = representation();
    if (r.IsTagged()) {
      HType t = type();
      if (t.IsSmi()) return Representation::Smi();
      if (t.IsHeapNumber()) return Representation::Double();
      if (t.IsHeapObject()) return r;
      return Representation::None();
    }
    return r;
  }

  HType type() const { return type_; }
  void set_type(HType new_type) {
    ASSERT(new_type.IsSubtypeOf(type_));
    type_ = new_type;
  }

  bool IsHeapObject() {
    return representation_.IsHeapObject() || type_.IsHeapObject();
  }

  // An operation needs to override this function iff:
  //   1) it can produce an int32 output.
  //   2) the true value of its output can potentially be minus zero.
  // The implementation must set a flag so that it bails out in the case where
  // it would otherwise output what should be a minus zero as an int32 zero.
  // If the operation also exists in a form that takes int32 and outputs int32
  // then the operation should return its input value so that we can propagate
  // back.  There are three operations that need to propagate back to more than
  // one input.  They are phi and binary div and mul.  They always return NULL
  // and expect the caller to take care of things.
  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited) {
    visited->Add(id());
    return NULL;
  }

  // There are HInstructions that do not really change a value, they
  // only add pieces of information to it (like bounds checks, map checks,
  // smi checks...).
  // We call these instructions "informative definitions", or "iDef".
  // One of the iDef operands is special because it is the value that is
  // "transferred" to the output, we call it the "redefined operand".
  // If an HValue is an iDef it must override RedefinedOperandIndex() so that
  // it does not return kNoRedefinedOperand;
  static const int kNoRedefinedOperand = -1;
  virtual int RedefinedOperandIndex() { return kNoRedefinedOperand; }
  bool IsInformativeDefinition() {
    return RedefinedOperandIndex() != kNoRedefinedOperand;
  }
  HValue* RedefinedOperand() {
    int index = RedefinedOperandIndex();
    return index == kNoRedefinedOperand ? NULL : OperandAt(index);
  }

  // A purely informative definition is an idef that will not emit code and
  // should therefore be removed from the graph in the RestoreActualValues
  // phase (so that live ranges will be shorter).
  virtual bool IsPurelyInformativeDefinition() { return false; }

  // This method must always return the original HValue SSA definition
  // (regardless of any iDef of this value).
  HValue* ActualValue() {
    int index = RedefinedOperandIndex();
    return index == kNoRedefinedOperand ? this : OperandAt(index);
  }

  bool IsInteger32Constant();
  int32_t GetInteger32Constant();
  bool EqualsInteger32Constant(int32_t value);

  bool IsDefinedAfter(HBasicBlock* other) const;

  // Operands.
  virtual int OperandCount() = 0;
  virtual HValue* OperandAt(int index) const = 0;
  void SetOperandAt(int index, HValue* value);

  void DeleteAndReplaceWith(HValue* other);
  void ReplaceAllUsesWith(HValue* other);
  bool HasNoUses() const { return use_list_ == NULL; }
  bool HasMultipleUses() const {
    return use_list_ != NULL && use_list_->tail() != NULL;
  }
  int UseCount() const;

  // Mark this HValue as dead and to be removed from other HValues' use lists.
  void Kill();

  int flags() const { return flags_; }
  void SetFlag(Flag f) { flags_ |= (1 << f); }
  void ClearFlag(Flag f) { flags_ &= ~(1 << f); }
  bool CheckFlag(Flag f) const { return (flags_ & (1 << f)) != 0; }

  // Returns true if the flag specified is set for all uses, false otherwise.
  bool CheckUsesForFlag(Flag f) const;
  // Same as before and the first one without the flag is returned in value.
  bool CheckUsesForFlag(Flag f, HValue** value) const;
  // Returns true if the flag specified is set for all uses, and this set
  // of uses is non-empty.
  bool HasAtLeastOneUseWithFlagAndNoneWithout(Flag f) const;

  GVNFlagSet gvn_flags() const { return gvn_flags_; }
  void SetGVNFlag(GVNFlag f) { gvn_flags_.Add(f); }
  void ClearGVNFlag(GVNFlag f) { gvn_flags_.Remove(f); }
  bool CheckGVNFlag(GVNFlag f) const { return gvn_flags_.Contains(f); }
  void SetAllSideEffects() { gvn_flags_.Add(AllSideEffectsFlagSet()); }
  void ClearAllSideEffects() {
    gvn_flags_.Remove(AllSideEffectsFlagSet());
  }
  bool HasSideEffects() const {
    return gvn_flags_.ContainsAnyOf(AllSideEffectsFlagSet());
  }
  bool HasObservableSideEffects() const {
    return !CheckFlag(kHasNoObservableSideEffects) &&
        gvn_flags_.ContainsAnyOf(AllObservableSideEffectsFlagSet());
  }

  GVNFlagSet DependsOnFlags() const {
    GVNFlagSet result = gvn_flags_;
    result.Intersect(AllDependsOnFlagSet());
    return result;
  }

  GVNFlagSet SideEffectFlags() const {
    GVNFlagSet result = gvn_flags_;
    result.Intersect(AllSideEffectsFlagSet());
    return result;
  }

  GVNFlagSet ChangesFlags() const {
    GVNFlagSet result = gvn_flags_;
    result.Intersect(AllChangesFlagSet());
    return result;
  }

  GVNFlagSet ObservableChangesFlags() const {
    GVNFlagSet result = gvn_flags_;
    result.Intersect(AllChangesFlagSet());
    result.Intersect(AllObservableSideEffectsFlagSet());
    return result;
  }

  Range* range() const { return range_; }
  // TODO(svenpanne) We should really use the null object pattern here.
  bool HasRange() const { return range_ != NULL; }
  bool CanBeNegative() const { return !HasRange() || range()->CanBeNegative(); }
  bool CanBeZero() const { return !HasRange() || range()->CanBeZero(); }
  bool RangeCanInclude(int value) const {
    return !HasRange() || range()->Includes(value);
  }
  void AddNewRange(Range* r, Zone* zone);
  void RemoveLastAddedRange();
  void ComputeInitialRange(Zone* zone);

  // Escape analysis helpers.
  virtual bool HasEscapingOperandAt(int index) { return true; }
  virtual bool HasOutOfBoundsAccess(int size) { return false; }

  // Representation helpers.
  virtual Representation observed_input_representation(int index) {
    return Representation::None();
  }
  virtual Representation RequiredInputRepresentation(int index) = 0;
  virtual void InferRepresentation(HInferRepresentationPhase* h_infer);

  // This gives the instruction an opportunity to replace itself with an
  // instruction that does the same in some better way.  To replace an
  // instruction with a new one, first add the new instruction to the graph,
  // then return it.  Return NULL to have the instruction deleted.
  virtual HValue* Canonicalize() { return this; }

  bool Equals(HValue* other);
  virtual intptr_t Hashcode();

  // Compute unique ids upfront that is safe wrt GC and concurrent compilation.
  virtual void FinalizeUniqueValueId() { }

  // Printing support.
  virtual void PrintTo(StringStream* stream) = 0;
  void PrintNameTo(StringStream* stream);
  void PrintTypeTo(StringStream* stream);
  void PrintRangeTo(StringStream* stream);
  void PrintChangesTo(StringStream* stream);

  const char* Mnemonic() const;

  // Type information helpers.
  bool HasMonomorphicJSObjectType();

  // TODO(mstarzinger): For now instructions can override this function to
  // specify statically known types, once HType can convey more information
  // it should be based on the HType.
  virtual Handle<Map> GetMonomorphicJSObjectMap() { return Handle<Map>(); }

  // Updated the inferred type of this instruction and returns true if
  // it has changed.
  bool UpdateInferredType();

  virtual HType CalculateInferredType();

  // This function must be overridden for instructions which have the
  // kTrackSideEffectDominators flag set, to track instructions that are
  // dominating side effects.
  virtual void HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) {
    UNREACHABLE();
  }

  // Check if this instruction has some reason that prevents elimination.
  bool CannotBeEliminated() const {
    return HasObservableSideEffects() || !IsDeletable();
  }

#ifdef DEBUG
  virtual void Verify() = 0;
#endif

  virtual bool TryDecompose(DecompositionResult* decomposition) {
    if (RedefinedOperand() != NULL) {
      return RedefinedOperand()->TryDecompose(decomposition);
    } else {
      return false;
    }
  }

  // Returns true conservatively if the program might be able to observe a
  // ToString() operation on this value.
  bool ToStringCanBeObserved() const {
    return type().ToStringOrToNumberCanBeObserved(representation());
  }

  // Returns true conservatively if the program might be able to observe a
  // ToNumber() operation on this value.
  bool ToNumberCanBeObserved() const {
    return type().ToStringOrToNumberCanBeObserved(representation());
  }

  MinusZeroMode GetMinusZeroMode() {
    return CheckFlag(kBailoutOnMinusZero)
        ? FAIL_ON_MINUS_ZERO : TREAT_MINUS_ZERO_AS_ZERO;
  }

 protected:
  // This function must be overridden for instructions with flag kUseGVN, to
  // compare the non-Operand parts of the instruction.
  virtual bool DataEquals(HValue* other) {
    UNREACHABLE();
    return false;
  }

  virtual Representation RepresentationFromInputs() {
    return representation();
  }
  Representation RepresentationFromUses();
  Representation RepresentationFromUseRequirements();
  bool HasNonSmiUse();
  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason);
  void AddDependantsToWorklist(HInferRepresentationPhase* h_infer);

  virtual void RepresentationChanged(Representation to) { }

  virtual Range* InferRange(Zone* zone);
  virtual void DeleteFromGraph() = 0;
  virtual void InternalSetOperandAt(int index, HValue* value) = 0;
  void clear_block() {
    ASSERT(block_ != NULL);
    block_ = NULL;
  }

  void set_representation(Representation r) {
    ASSERT(representation_.IsNone() && !r.IsNone());
    representation_ = r;
  }

  static GVNFlagSet AllDependsOnFlagSet() {
    GVNFlagSet result;
    // Create changes mask.
#define ADD_FLAG(type) result.Add(kDependsOn##type);
  GVN_TRACKED_FLAG_LIST(ADD_FLAG)
  GVN_UNTRACKED_FLAG_LIST(ADD_FLAG)
#undef ADD_FLAG
    return result;
  }

  static GVNFlagSet AllChangesFlagSet() {
    GVNFlagSet result;
    // Create changes mask.
#define ADD_FLAG(type) result.Add(kChanges##type);
  GVN_TRACKED_FLAG_LIST(ADD_FLAG)
  GVN_UNTRACKED_FLAG_LIST(ADD_FLAG)
#undef ADD_FLAG
    return result;
  }

  // A flag mask to mark an instruction as having arbitrary side effects.
  static GVNFlagSet AllSideEffectsFlagSet() {
    GVNFlagSet result = AllChangesFlagSet();
    result.Remove(kChangesOsrEntries);
    return result;
  }

  // A flag mask of all side effects that can make observable changes in
  // an executing program (i.e. are not safe to repeat, move or remove);
  static GVNFlagSet AllObservableSideEffectsFlagSet() {
    GVNFlagSet result = AllChangesFlagSet();
    result.Remove(kChangesNewSpacePromotion);
    result.Remove(kChangesElementsKind);
    result.Remove(kChangesElementsPointer);
    result.Remove(kChangesMaps);
    return result;
  }

  // Remove the matching use from the use list if present.  Returns the
  // removed list node or NULL.
  HUseListNode* RemoveUse(HValue* value, int index);

  void RegisterUse(int index, HValue* new_value);

  HBasicBlock* block_;

  // The id of this instruction in the hydrogen graph, assigned when first
  // added to the graph. Reflects creation order.
  int id_;

  Representation representation_;
  HType type_;
  HUseListNode* use_list_;
  Range* range_;
  int flags_;
  GVNFlagSet gvn_flags_;

 private:
  virtual bool IsDeletable() const { return false; }

  DISALLOW_COPY_AND_ASSIGN(HValue);
};


#define DECLARE_INSTRUCTION_FACTORY_P0(I)                                      \
  static I* New(Zone* zone, HValue* context) {                                 \
    return new(zone) I();                                                      \
}

#define DECLARE_INSTRUCTION_FACTORY_P1(I, P1)                                  \
  static I* New(Zone* zone, HValue* context, P1 p1) {                          \
    return new(zone) I(p1);                                                    \
  }

#define DECLARE_INSTRUCTION_FACTORY_P2(I, P1, P2)                              \
  static I* New(Zone* zone, HValue* context, P1 p1, P2 p2) {                   \
    return new(zone) I(p1, p2);                                                \
  }

#define DECLARE_INSTRUCTION_FACTORY_P3(I, P1, P2, P3)                          \
  static I* New(Zone* zone, HValue* context, P1 p1, P2 p2, P3 p3) {            \
    return new(zone) I(p1, p2, p3);                                            \
  }

#define DECLARE_INSTRUCTION_FACTORY_P4(I, P1, P2, P3, P4)                      \
  static I* New(Zone* zone,                                                    \
                HValue* context,                                               \
                P1 p1,                                                         \
                P2 p2,                                                         \
                P3 p3,                                                         \
                P4 p4) {                                                       \
    return new(zone) I(p1, p2, p3, p4);                                        \
  }

#define DECLARE_INSTRUCTION_FACTORY_P5(I, P1, P2, P3, P4, P5)                  \
  static I* New(Zone* zone,                                                    \
                HValue* context,                                               \
                P1 p1,                                                         \
                P2 p2,                                                         \
                P3 p3,                                                         \
                P4 p4,                                                         \
                P5 p5) {                                                       \
    return new(zone) I(p1, p2, p3, p4, p5);                                    \
  }


class HInstruction : public HValue {
 public:
  HInstruction* next() const { return next_; }
  HInstruction* previous() const { return previous_; }

  virtual void PrintTo(StringStream* stream) V8_OVERRIDE;
  virtual void PrintDataTo(StringStream* stream);

  bool IsLinked() const { return block() != NULL; }
  void Unlink();
  void InsertBefore(HInstruction* next);
  void InsertAfter(HInstruction* previous);

  // The position is a write-once variable.
  int position() const { return position_; }
  bool has_position() const { return position_ != RelocInfo::kNoPosition; }
  void set_position(int position) {
    ASSERT(!has_position());
    ASSERT(position != RelocInfo::kNoPosition);
    position_ = position;
  }

  bool CanTruncateToInt32() const { return CheckFlag(kTruncatingToInt32); }

  virtual LInstruction* CompileToLithium(LChunkBuilder* builder) = 0;

#ifdef DEBUG
  virtual void Verify() V8_OVERRIDE;
#endif

  virtual bool IsCall() { return false; }

  DECLARE_ABSTRACT_INSTRUCTION(Instruction)

 protected:
  HInstruction(HType type = HType::Tagged())
      : HValue(type),
        next_(NULL),
        previous_(NULL),
        position_(RelocInfo::kNoPosition) {
    SetGVNFlag(kDependsOnOsrEntries);
  }

  virtual void DeleteFromGraph() V8_OVERRIDE { Unlink(); }

 private:
  void InitializeAsFirst(HBasicBlock* block) {
    ASSERT(!IsLinked());
    SetBlock(block);
  }

  void PrintMnemonicTo(StringStream* stream);

  HInstruction* next_;
  HInstruction* previous_;
  int position_;

  friend class HBasicBlock;
};


template<int V>
class HTemplateInstruction : public HInstruction {
 public:
  virtual int OperandCount() V8_FINAL V8_OVERRIDE { return V; }
  virtual HValue* OperandAt(int i) const V8_FINAL V8_OVERRIDE {
    return inputs_[i];
  }

 protected:
  HTemplateInstruction(HType type = HType::Tagged()) : HInstruction(type) {}

  virtual void InternalSetOperandAt(int i, HValue* value) V8_FINAL V8_OVERRIDE {
    inputs_[i] = value;
  }

 private:
  EmbeddedContainer<HValue*, V> inputs_;
};


class HControlInstruction : public HInstruction {
 public:
  virtual HBasicBlock* SuccessorAt(int i) = 0;
  virtual int SuccessorCount() = 0;
  virtual void SetSuccessorAt(int i, HBasicBlock* block) = 0;

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HBasicBlock* FirstSuccessor() {
    return SuccessorCount() > 0 ? SuccessorAt(0) : NULL;
  }
  HBasicBlock* SecondSuccessor() {
    return SuccessorCount() > 1 ? SuccessorAt(1) : NULL;
  }

  DECLARE_ABSTRACT_INSTRUCTION(ControlInstruction)
};


class HSuccessorIterator V8_FINAL BASE_EMBEDDED {
 public:
  explicit HSuccessorIterator(HControlInstruction* instr)
      : instr_(instr), current_(0) { }

  bool Done() { return current_ >= instr_->SuccessorCount(); }
  HBasicBlock* Current() { return instr_->SuccessorAt(current_); }
  void Advance() { current_++; }

 private:
  HControlInstruction* instr_;
  int current_;
};


template<int S, int V>
class HTemplateControlInstruction : public HControlInstruction {
 public:
  int SuccessorCount() V8_OVERRIDE { return S; }
  HBasicBlock* SuccessorAt(int i) V8_OVERRIDE { return successors_[i]; }
  void SetSuccessorAt(int i, HBasicBlock* block) V8_OVERRIDE {
    successors_[i] = block;
  }

  int OperandCount() V8_OVERRIDE { return V; }
  HValue* OperandAt(int i) const V8_OVERRIDE { return inputs_[i]; }


 protected:
  void InternalSetOperandAt(int i, HValue* value) V8_OVERRIDE {
    inputs_[i] = value;
  }

 private:
  EmbeddedContainer<HBasicBlock*, S> successors_;
  EmbeddedContainer<HValue*, V> inputs_;
};


class HBlockEntry V8_FINAL : public HTemplateInstruction<0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(BlockEntry)
};


class HDummyUse V8_FINAL : public HTemplateInstruction<1> {
 public:
  explicit HDummyUse(HValue* value)
      : HTemplateInstruction<1>(HType::Smi()) {
    SetOperandAt(0, value);
    // Pretend to be a Smi so that the HChange instructions inserted
    // before any use generate as little code as possible.
    set_representation(Representation::Tagged());
  }

  HValue* value() { return OperandAt(0); }

  virtual bool HasEscapingOperandAt(int index) V8_OVERRIDE { return false; }
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(DummyUse);
};


class HDeoptimize V8_FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HDeoptimize, const char*,
                                 Deoptimizer::BailoutType);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  const char* reason() const { return reason_; }
  Deoptimizer::BailoutType type() { return type_; }

  DECLARE_CONCRETE_INSTRUCTION(Deoptimize)

 private:
  explicit HDeoptimize(const char* reason, Deoptimizer::BailoutType type)
      : reason_(reason), type_(type) {}

  const char* reason_;
  Deoptimizer::BailoutType type_;
};


// Inserts an int3/stop break instruction for debugging purposes.
class HDebugBreak V8_FINAL : public HTemplateInstruction<0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(DebugBreak)
};


class HGoto V8_FINAL : public HTemplateControlInstruction<1, 0> {
 public:
  explicit HGoto(HBasicBlock* target) {
    SetSuccessorAt(0, target);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(Goto)
};


class HUnaryControlInstruction : public HTemplateControlInstruction<2, 1> {
 public:
  HUnaryControlInstruction(HValue* value,
                           HBasicBlock* true_target,
                           HBasicBlock* false_target) {
    SetOperandAt(0, value);
    SetSuccessorAt(0, true_target);
    SetSuccessorAt(1, false_target);
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* value() { return OperandAt(0); }
};


class HBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  HBranch(HValue* value,
          ToBooleanStub::Types expected_input_types = ToBooleanStub::Types(),
          HBasicBlock* true_target = NULL,
          HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target),
        expected_input_types_(expected_input_types) {
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }
  virtual Representation observed_input_representation(int index) V8_OVERRIDE;

  ToBooleanStub::Types expected_input_types() const {
    return expected_input_types_;
  }

  DECLARE_CONCRETE_INSTRUCTION(Branch)

 private:
  ToBooleanStub::Types expected_input_types_;
};


class HCompareMap V8_FINAL : public HUnaryControlInstruction {
 public:
  HCompareMap(HValue* value,
              Handle<Map> map,
              HBasicBlock* true_target = NULL,
              HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target),
      map_(map) {
    ASSERT(!map.is_null());
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<Map> map() const { return map_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareMap)

 protected:
  virtual int RedefinedOperandIndex() { return 0; }

 private:
  Handle<Map> map_;
};


class HContext V8_FINAL : public HTemplateInstruction<0> {
 public:
  static HContext* New(Zone* zone) {
    return new(zone) HContext();
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Context)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  HContext() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HReturn V8_FINAL : public HTemplateControlInstruction<0, 3> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* value,
                           HValue* parameter_count) {
    return new(zone) HReturn(value, context, parameter_count);
  }

  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* value) {
    return new(zone) HReturn(value, context, 0);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* value() { return OperandAt(0); }
  HValue* context() { return OperandAt(1); }
  HValue* parameter_count() { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(Return)

 private:
  HReturn(HValue* value, HValue* context, HValue* parameter_count) {
    SetOperandAt(0, value);
    SetOperandAt(1, context);
    SetOperandAt(2, parameter_count);
  }
};


class HUnaryOperation : public HTemplateInstruction<1> {
 public:
  HUnaryOperation(HValue* value, HType type = HType::Tagged())
      : HTemplateInstruction<1>(type) {
    SetOperandAt(0, value);
  }

  static HUnaryOperation* cast(HValue* value) {
    return reinterpret_cast<HUnaryOperation*>(value);
  }

  HValue* value() const { return OperandAt(0); }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class HThrow V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HThrow* New(Zone* zone,
                     HValue* context,
                     HValue* value) {
    return new(zone) HThrow(context, value);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(Throw)

 private:
  HThrow(HValue* context, HValue* value) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    SetAllSideEffects();
  }
};


class HUseConst V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HUseConst, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(UseConst)

 private:
    explicit HUseConst(HValue* old_value) : HUnaryOperation(old_value) { }
};


class HForceRepresentation V8_FINAL : public HTemplateInstruction<1> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HForceRepresentation, HValue*, Representation);

  HValue* value() { return OperandAt(0); }

  virtual HValue* EnsureAndPropagateNotMinusZero(
      BitVector* visited) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return representation();  // Same as the output representation.
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(ForceRepresentation)

 private:
  HForceRepresentation(HValue* value, Representation required_representation) {
    SetOperandAt(0, value);
    set_representation(required_representation);
  }
};


class HChange V8_FINAL : public HUnaryOperation {
 public:
  HChange(HValue* value,
          Representation to,
          bool is_truncating_to_smi,
          bool is_truncating_to_int32)
      : HUnaryOperation(value) {
    ASSERT(!value->representation().IsNone());
    ASSERT(!to.IsNone());
    ASSERT(!value->representation().Equals(to));
    set_representation(to);
    SetFlag(kUseGVN);
    if (is_truncating_to_smi) {
      SetFlag(kTruncatingToSmi);
      SetFlag(kTruncatingToInt32);
    }
    if (is_truncating_to_int32) SetFlag(kTruncatingToInt32);
    if (value->representation().IsSmi() || value->type().IsSmi()) {
      set_type(HType::Smi());
    } else {
      set_type(HType::TaggedNumber());
      if (to.IsTagged()) SetGVNFlag(kChangesNewSpacePromotion);
    }
  }

  bool can_convert_undefined_to_nan() {
    return CheckUsesForFlag(kAllowUndefinedAsNaN);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(
      BitVector* visited) V8_OVERRIDE;
  virtual HType CalculateInferredType() V8_OVERRIDE;
  virtual HValue* Canonicalize() V8_OVERRIDE;

  Representation from() const { return value()->representation(); }
  Representation to() const { return representation(); }
  bool deoptimize_on_minus_zero() const {
    return CheckFlag(kBailoutOnMinusZero);
  }
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return from();
  }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(Change)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  virtual bool IsDeletable() const V8_OVERRIDE {
    return !from().IsTagged() || value()->type().IsSmi();
  }
};


class HClampToUint8 V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HClampToUint8, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ClampToUint8)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HClampToUint8(HValue* value)
      : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kAllowUndefinedAsNaN);
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


enum RemovableSimulate {
  REMOVABLE_SIMULATE,
  FIXED_SIMULATE
};


class HSimulate V8_FINAL : public HInstruction {
 public:
  HSimulate(BailoutId ast_id,
            int pop_count,
            Zone* zone,
            RemovableSimulate removable)
      : ast_id_(ast_id),
        pop_count_(pop_count),
        values_(2, zone),
        assigned_indexes_(2, zone),
        zone_(zone),
        removable_(removable) {}
  ~HSimulate() {}

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  bool HasAstId() const { return !ast_id_.IsNone(); }
  BailoutId ast_id() const { return ast_id_; }
  void set_ast_id(BailoutId id) {
    ASSERT(!HasAstId());
    ast_id_ = id;
  }

  int pop_count() const { return pop_count_; }
  const ZoneList<HValue*>* values() const { return &values_; }
  int GetAssignedIndexAt(int index) const {
    ASSERT(HasAssignedIndexAt(index));
    return assigned_indexes_[index];
  }
  bool HasAssignedIndexAt(int index) const {
    return assigned_indexes_[index] != kNoIndex;
  }
  void AddAssignedValue(int index, HValue* value) {
    AddValue(index, value);
  }
  void AddPushedValue(HValue* value) {
    AddValue(kNoIndex, value);
  }
  int ToOperandIndex(int environment_index) {
    for (int i = 0; i < assigned_indexes_.length(); ++i) {
      if (assigned_indexes_[i] == environment_index) return i;
    }
    return -1;
  }
  virtual int OperandCount() V8_OVERRIDE { return values_.length(); }
  virtual HValue* OperandAt(int index) const V8_OVERRIDE {
    return values_[index];
  }

  virtual bool HasEscapingOperandAt(int index) V8_OVERRIDE { return false; }
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  void MergeWith(ZoneList<HSimulate*>* list);
  bool is_candidate_for_removal() { return removable_ == REMOVABLE_SIMULATE; }

  // Replay effects of this instruction on the given environment.
  void ReplayEnvironment(HEnvironment* env);

  DECLARE_CONCRETE_INSTRUCTION(Simulate)

#ifdef DEBUG
  virtual void Verify() V8_OVERRIDE;
  void set_closure(Handle<JSFunction> closure) { closure_ = closure; }
  Handle<JSFunction> closure() const { return closure_; }
#endif

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) V8_OVERRIDE {
    values_[index] = value;
  }

 private:
  static const int kNoIndex = -1;
  void AddValue(int index, HValue* value) {
    assigned_indexes_.Add(index, zone_);
    // Resize the list of pushed values.
    values_.Add(NULL, zone_);
    // Set the operand through the base method in HValue to make sure that the
    // use lists are correctly updated.
    SetOperandAt(values_.length() - 1, value);
  }
  bool HasValueForIndex(int index) {
    for (int i = 0; i < assigned_indexes_.length(); ++i) {
      if (assigned_indexes_[i] == index) return true;
    }
    return false;
  }
  BailoutId ast_id_;
  int pop_count_;
  ZoneList<HValue*> values_;
  ZoneList<int> assigned_indexes_;
  Zone* zone_;
  RemovableSimulate removable_;

#ifdef DEBUG
  Handle<JSFunction> closure_;
#endif
};


class HEnvironmentMarker V8_FINAL : public HTemplateInstruction<1> {
 public:
  enum Kind { BIND, LOOKUP };

  HEnvironmentMarker(Kind kind, int index)
      : kind_(kind), index_(index), next_simulate_(NULL) { }

  Kind kind() { return kind_; }
  int index() { return index_; }
  HSimulate* next_simulate() { return next_simulate_; }
  void set_next_simulate(HSimulate* simulate) {
    next_simulate_ = simulate;
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

#ifdef DEBUG
  void set_closure(Handle<JSFunction> closure) {
    ASSERT(closure_.is_null());
    ASSERT(!closure.is_null());
    closure_ = closure;
  }
  Handle<JSFunction> closure() const { return closure_; }
#endif

  DECLARE_CONCRETE_INSTRUCTION(EnvironmentMarker);

 private:
  Kind kind_;
  int index_;
  HSimulate* next_simulate_;

#ifdef DEBUG
  Handle<JSFunction> closure_;
#endif
};


class HStackCheck V8_FINAL : public HTemplateInstruction<1> {
 public:
  enum Type {
    kFunctionEntry,
    kBackwardsBranch
  };

  DECLARE_INSTRUCTION_FACTORY_P2(HStackCheck, HValue*, Type);

  HValue* context() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  void Eliminate() {
    // The stack check eliminator might try to eliminate the same stack
    // check instruction multiple times.
    if (IsLinked()) {
      DeleteAndReplaceWith(NULL);
    }
  }

  bool is_function_entry() { return type_ == kFunctionEntry; }
  bool is_backwards_branch() { return type_ == kBackwardsBranch; }

  DECLARE_CONCRETE_INSTRUCTION(StackCheck)

 private:
  HStackCheck(HValue* context, Type type) : type_(type) {
    SetOperandAt(0, context);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  Type type_;
};


enum InliningKind {
  NORMAL_RETURN,          // Normal function/method call and return.
  DROP_EXTRA_ON_RETURN,   // Drop an extra value from the environment on return.
  CONSTRUCT_CALL_RETURN,  // Either use allocated receiver or return value.
  GETTER_CALL_RETURN,     // Returning from a getter, need to restore context.
  SETTER_CALL_RETURN      // Use the RHS of the assignment as the return value.
};


class HArgumentsObject;


class HEnterInlined V8_FINAL : public HTemplateInstruction<0> {
 public:
  static HEnterInlined* New(Zone* zone,
                            HValue* context,
                            Handle<JSFunction> closure,
                            int arguments_count,
                            FunctionLiteral* function,
                            InliningKind inlining_kind,
                            Variable* arguments_var,
                            HArgumentsObject* arguments_object,
                            bool undefined_receiver) {
    return new(zone) HEnterInlined(closure, arguments_count, function,
                                   inlining_kind, arguments_var,
                                   arguments_object, undefined_receiver, zone);
  }

  void RegisterReturnTarget(HBasicBlock* return_target, Zone* zone);
  ZoneList<HBasicBlock*>* return_targets() { return &return_targets_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<JSFunction> closure() const { return closure_; }
  int arguments_count() const { return arguments_count_; }
  bool arguments_pushed() const { return arguments_pushed_; }
  void set_arguments_pushed() { arguments_pushed_ = true; }
  FunctionLiteral* function() const { return function_; }
  InliningKind inlining_kind() const { return inlining_kind_; }
  bool undefined_receiver() const { return undefined_receiver_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  Variable* arguments_var() { return arguments_var_; }
  HArgumentsObject* arguments_object() { return arguments_object_; }

  DECLARE_CONCRETE_INSTRUCTION(EnterInlined)

 private:
  HEnterInlined(Handle<JSFunction> closure,
                int arguments_count,
                FunctionLiteral* function,
                InliningKind inlining_kind,
                Variable* arguments_var,
                HArgumentsObject* arguments_object,
                bool undefined_receiver,
                Zone* zone)
      : closure_(closure),
        arguments_count_(arguments_count),
        arguments_pushed_(false),
        function_(function),
        inlining_kind_(inlining_kind),
        arguments_var_(arguments_var),
        arguments_object_(arguments_object),
        undefined_receiver_(undefined_receiver),
        return_targets_(2, zone) {
  }

  Handle<JSFunction> closure_;
  int arguments_count_;
  bool arguments_pushed_;
  FunctionLiteral* function_;
  InliningKind inlining_kind_;
  Variable* arguments_var_;
  HArgumentsObject* arguments_object_;
  bool undefined_receiver_;
  ZoneList<HBasicBlock*> return_targets_;
};


class HLeaveInlined V8_FINAL : public HTemplateInstruction<0> {
 public:
  HLeaveInlined() { }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(LeaveInlined)
};


class HPushArgument V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HPushArgument, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* argument() { return OperandAt(0); }

  DECLARE_CONCRETE_INSTRUCTION(PushArgument)

 private:
  explicit HPushArgument(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
  }
};


class HThisFunction V8_FINAL : public HTemplateInstruction<0> {
 public:
  HThisFunction() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ThisFunction)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HOuterContext V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HOuterContext, HValue*);

  DECLARE_CONCRETE_INSTRUCTION(OuterContext);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HOuterContext(HValue* inner) : HUnaryOperation(inner) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HDeclareGlobals V8_FINAL : public HUnaryOperation {
 public:
  HDeclareGlobals(HValue* context,
                  Handle<FixedArray> pairs,
                  int flags)
      : HUnaryOperation(context),
        pairs_(pairs),
        flags_(flags) {
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  static HDeclareGlobals* New(Zone* zone,
                              HValue* context,
                              Handle<FixedArray> pairs,
                              int flags) {
    return new(zone) HDeclareGlobals(context, pairs, flags);
  }

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> pairs() const { return pairs_; }
  int flags() const { return flags_; }

  DECLARE_CONCRETE_INSTRUCTION(DeclareGlobals)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

 private:
  Handle<FixedArray> pairs_;
  int flags_;
};


class HGlobalObject V8_FINAL : public HUnaryOperation {
 public:
  explicit HGlobalObject(HValue* context) : HUnaryOperation(context) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  static HGlobalObject* New(Zone* zone, HValue* context) {
    return new(zone) HGlobalObject(context);
  }

  DECLARE_CONCRETE_INSTRUCTION(GlobalObject)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HGlobalReceiver V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HGlobalReceiver, HValue*);

  DECLARE_CONCRETE_INSTRUCTION(GlobalReceiver)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HGlobalReceiver(HValue* global_object)
      : HUnaryOperation(global_object) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


template <int V>
class HCall : public HTemplateInstruction<V> {
 public:
  // The argument count includes the receiver.
  explicit HCall<V>(int argument_count) : argument_count_(argument_count) {
    this->set_representation(Representation::Tagged());
    this->SetAllSideEffects();
  }

  virtual HType CalculateInferredType() V8_FINAL V8_OVERRIDE {
    return HType::Tagged();
  }

  virtual int argument_count() const { return argument_count_; }

  virtual bool IsCall() V8_FINAL V8_OVERRIDE { return true; }

 private:
  int argument_count_;
};


class HUnaryCall : public HCall<1> {
 public:
  HUnaryCall(HValue* value, int argument_count)
      : HCall<1>(argument_count) {
    SetOperandAt(0, value);
  }

  virtual Representation RequiredInputRepresentation(
      int index) V8_FINAL V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* value() { return OperandAt(0); }
};


class HBinaryCall : public HCall<2> {
 public:
  HBinaryCall(HValue* first, HValue* second, int argument_count)
      : HCall<2>(argument_count) {
    SetOperandAt(0, first);
    SetOperandAt(1, second);
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(
      int index) V8_FINAL V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* first() { return OperandAt(0); }
  HValue* second() { return OperandAt(1); }
};


class HInvokeFunction V8_FINAL : public HBinaryCall {
 public:
  HInvokeFunction(HValue* context, HValue* function, int argument_count)
      : HBinaryCall(context, function, argument_count) {
  }

  static HInvokeFunction* New(Zone* zone,
                              HValue* context,
                              HValue* function,
                              int argument_count) {
    return new(zone) HInvokeFunction(context, function, argument_count);
  }

  HInvokeFunction(HValue* context,
                  HValue* function,
                  Handle<JSFunction> known_function,
                  int argument_count)
      : HBinaryCall(context, function, argument_count),
        known_function_(known_function) {
    formal_parameter_count_ = known_function.is_null()
        ? 0 : known_function->shared()->formal_parameter_count();
  }

  static HInvokeFunction* New(Zone* zone,
                              HValue* context,
                              HValue* function,
                              Handle<JSFunction> known_function,
                              int argument_count) {
    return new(zone) HInvokeFunction(context, function,
                                     known_function, argument_count);
  }

  HValue* context() { return first(); }
  HValue* function() { return second(); }
  Handle<JSFunction> known_function() { return known_function_; }
  int formal_parameter_count() const { return formal_parameter_count_; }

  DECLARE_CONCRETE_INSTRUCTION(InvokeFunction)

 private:
  Handle<JSFunction> known_function_;
  int formal_parameter_count_;
};


class HCallConstantFunction V8_FINAL : public HCall<0> {
 public:
  HCallConstantFunction(Handle<JSFunction> function, int argument_count)
      : HCall<0>(argument_count),
        function_(function),
        formal_parameter_count_(function->shared()->formal_parameter_count()) {}

  Handle<JSFunction> function() const { return function_; }
  int formal_parameter_count() const { return formal_parameter_count_; }

  bool IsApplyFunction() const {
    return function_->code() ==
        function_->GetIsolate()->builtins()->builtin(Builtins::kFunctionApply);
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallConstantFunction)

 private:
  Handle<JSFunction> function_;
  int formal_parameter_count_;
};


class HCallKeyed V8_FINAL : public HBinaryCall {
 public:
  HCallKeyed(HValue* context, HValue* key, int argument_count)
      : HBinaryCall(context, key, argument_count) {
  }

  HValue* context() { return first(); }
  HValue* key() { return second(); }

  DECLARE_CONCRETE_INSTRUCTION(CallKeyed)
};


class HCallNamed V8_FINAL : public HUnaryCall {
 public:
  HCallNamed(HValue* context, Handle<String> name, int argument_count)
      : HUnaryCall(context, argument_count), name_(name) {
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* context() { return value(); }
  Handle<String> name() const { return name_; }

  DECLARE_CONCRETE_INSTRUCTION(CallNamed)

 private:
  Handle<String> name_;
};


class HCallFunction V8_FINAL : public HBinaryCall {
 public:
  HCallFunction(HValue* context, HValue* function, int argument_count)
      : HBinaryCall(context, function, argument_count) {
  }

  static HCallFunction* New(Zone* zone,
                            HValue* context,
                            HValue* function,
                            int argument_count) {
    return new(zone) HCallFunction(context, function, argument_count);
  }

  HValue* context() { return first(); }
  HValue* function() { return second(); }

  DECLARE_CONCRETE_INSTRUCTION(CallFunction)
};


class HCallGlobal V8_FINAL : public HUnaryCall {
 public:
  HCallGlobal(HValue* context, Handle<String> name, int argument_count)
      : HUnaryCall(context, argument_count), name_(name) {
  }

  static HCallGlobal* New(Zone* zone,
                          HValue* context,
                          Handle<String> name,
                          int argument_count) {
    return new(zone) HCallGlobal(context, name, argument_count);
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* context() { return value(); }
  Handle<String> name() const { return name_; }

  DECLARE_CONCRETE_INSTRUCTION(CallGlobal)

 private:
  Handle<String> name_;
};


class HCallKnownGlobal V8_FINAL : public HCall<0> {
 public:
  HCallKnownGlobal(Handle<JSFunction> target, int argument_count)
      : HCall<0>(argument_count),
        target_(target),
        formal_parameter_count_(target->shared()->formal_parameter_count()) { }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<JSFunction> target() const { return target_; }
  int formal_parameter_count() const { return formal_parameter_count_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallKnownGlobal)

 private:
  Handle<JSFunction> target_;
  int formal_parameter_count_;
};


class HCallNew V8_FINAL : public HBinaryCall {
 public:
  HCallNew(HValue* context, HValue* constructor, int argument_count)
      : HBinaryCall(context, constructor, argument_count) {}

  HValue* context() { return first(); }
  HValue* constructor() { return second(); }

  DECLARE_CONCRETE_INSTRUCTION(CallNew)
};


class HCallNewArray V8_FINAL : public HBinaryCall {
 public:
  HCallNewArray(HValue* context, HValue* constructor, int argument_count,
                Handle<Cell> type_cell, ElementsKind elements_kind)
      : HBinaryCall(context, constructor, argument_count),
        elements_kind_(elements_kind),
        type_cell_(type_cell) {}

  HValue* context() { return first(); }
  HValue* constructor() { return second(); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<Cell> property_cell() const {
    return type_cell_;
  }

  ElementsKind elements_kind() const { return elements_kind_; }

  DECLARE_CONCRETE_INSTRUCTION(CallNewArray)

 private:
  ElementsKind elements_kind_;
  Handle<Cell> type_cell_;
};


class HCallRuntime V8_FINAL : public HCall<1> {
 public:
  static HCallRuntime* New(Zone* zone,
                           HValue* context,
                           Handle<String> name,
                           const Runtime::Function* c_function,
                           int argument_count) {
    return new(zone) HCallRuntime(context, name, c_function, argument_count);
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* context() { return OperandAt(0); }
  const Runtime::Function* function() const { return c_function_; }
  Handle<String> name() const { return name_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallRuntime)

 private:
  HCallRuntime(HValue* context,
               Handle<String> name,
               const Runtime::Function* c_function,
               int argument_count)
      : HCall<1>(argument_count), c_function_(c_function), name_(name) {
    SetOperandAt(0, context);
  }

  const Runtime::Function* c_function_;
  Handle<String> name_;
};


class HMapEnumLength V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HMapEnumLength, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(MapEnumLength)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HMapEnumLength(HValue* value)
      : HUnaryOperation(value, HType::Smi()) {
    set_representation(Representation::Smi());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HElementsKind V8_FINAL : public HUnaryOperation {
 public:
  explicit HElementsKind(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnElementsKind);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ElementsKind)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HUnaryMathOperation V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* value,
                           BuiltinFunctionId op);

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual HValue* EnsureAndPropagateNotMinusZero(
      BitVector* visited) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      switch (op_) {
        case kMathFloor:
        case kMathRound:
        case kMathSqrt:
        case kMathPowHalf:
        case kMathLog:
        case kMathExp:
        case kMathSin:
        case kMathCos:
        case kMathTan:
          return Representation::Double();
        case kMathAbs:
          return representation();
        default:
          UNREACHABLE();
          return Representation::None();
      }
    }
  }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

  virtual HValue* Canonicalize() V8_OVERRIDE;
  virtual Representation RepresentationFromInputs() V8_OVERRIDE;

  BuiltinFunctionId op() const { return op_; }
  const char* OpName() const;

  DECLARE_CONCRETE_INSTRUCTION(UnaryMathOperation)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HUnaryMathOperation* b = HUnaryMathOperation::cast(other);
    return op_ == b->op();
  }

 private:
  HUnaryMathOperation(HValue* context, HValue* value, BuiltinFunctionId op)
      : HTemplateInstruction<2>(HType::TaggedNumber()), op_(op) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    switch (op) {
      case kMathFloor:
      case kMathRound:
        set_representation(Representation::Integer32());
        break;
      case kMathAbs:
        // Not setting representation here: it is None intentionally.
        SetFlag(kFlexibleRepresentation);
        // TODO(svenpanne) This flag is actually only needed if representation()
        // is tagged, and not when it is an unboxed double or unboxed integer.
        SetGVNFlag(kChangesNewSpacePromotion);
        break;
      case kMathLog:
      case kMathSin:
      case kMathCos:
      case kMathTan:
        set_representation(Representation::Double());
        // These operations use the TranscendentalCache, so they may allocate.
        SetGVNFlag(kChangesNewSpacePromotion);
        break;
      case kMathExp:
      case kMathSqrt:
      case kMathPowHalf:
        set_representation(Representation::Double());
        break;
      default:
        UNREACHABLE();
    }
    SetFlag(kUseGVN);
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  BuiltinFunctionId op_;
};


class HLoadExternalArrayPointer V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HLoadExternalArrayPointer, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() V8_OVERRIDE {
    return HType::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadExternalArrayPointer)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HLoadExternalArrayPointer(HValue* value)
      : HUnaryOperation(value) {
    set_representation(Representation::External());
    // The result of this instruction is idempotent as long as its inputs don't
    // change.  The external array of a specialized array elements object cannot
    // change once set, so it's no necessary to introduce any additional
    // dependencies on top of the inputs.
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HCheckMaps V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HCheckMaps* New(Zone* zone, HValue* context, HValue* value,
                         Handle<Map> map, CompilationInfo* info,
                         HValue *typecheck = NULL);
  static HCheckMaps* New(Zone* zone, HValue* context,
                         HValue* value, SmallMapList* maps,
                         HValue *typecheck = NULL) {
    HCheckMaps* check_map = new(zone) HCheckMaps(value, zone, typecheck);
    for (int i = 0; i < maps->length(); i++) {
      check_map->Add(maps->at(i), zone);
    }
    check_map->map_set_.Sort();
    return check_map;
  }

  bool CanOmitMapChecks() { return omit_; }

  virtual bool HasEscapingOperandAt(int index) V8_OVERRIDE { return false; }
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }
  virtual void HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) V8_OVERRIDE;
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* value() { return OperandAt(0); }
  SmallMapList* map_set() { return &map_set_; }
  ZoneList<UniqueValueId>* map_unique_ids() { return &map_unique_ids_; }

  bool has_migration_target() {
    return has_migration_target_;
  }

  virtual void FinalizeUniqueValueId() V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CheckMaps)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    ASSERT_EQ(map_set_.length(), map_unique_ids_.length());
    HCheckMaps* b = HCheckMaps::cast(other);
    // Relies on the fact that map_set has been sorted before.
    if (map_unique_ids_.length() != b->map_unique_ids_.length()) {
      return false;
    }
    for (int i = 0; i < map_unique_ids_.length(); i++) {
      if (map_unique_ids_.at(i) != b->map_unique_ids_.at(i)) {
        return false;
      }
    }
    return true;
  }

  virtual int RedefinedOperandIndex() { return 0; }

 private:
  void Add(Handle<Map> map, Zone* zone) {
    map_set_.Add(map, zone);
    if (!has_migration_target_ && map->is_migration_target()) {
      has_migration_target_ = true;
      SetGVNFlag(kChangesNewSpacePromotion);
    }
  }

  // Clients should use one of the static New* methods above.
  HCheckMaps(HValue* value, Zone *zone, HValue* typecheck)
      : HTemplateInstruction<2>(value->type()),
        omit_(false), has_migration_target_(false), map_unique_ids_(0, zone) {
    SetOperandAt(0, value);
    // Use the object value for the dependency if NULL is passed.
    // TODO(titzer): do GVN flags already express this dependency?
    SetOperandAt(1, typecheck != NULL ? typecheck : value);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kTrackSideEffectDominators);
    SetGVNFlag(kDependsOnMaps);
    SetGVNFlag(kDependsOnElementsKind);
  }

  void omit(CompilationInfo* info) {
    omit_ = true;
    for (int i = 0; i < map_set_.length(); i++) {
      Handle<Map> map = map_set_.at(i);
      if (!map->CanTransition()) continue;
      map->AddDependentCompilationInfo(DependentCode::kPrototypeCheckGroup,
                                       info);
    }
  }

  bool omit_;
  bool has_migration_target_;
  SmallMapList map_set_;
  ZoneList<UniqueValueId> map_unique_ids_;
};


class HCheckValue V8_FINAL : public HUnaryOperation {
 public:
  static HCheckValue* New(Zone* zone, HValue* context,
                          HValue* value, Handle<JSFunction> target) {
    bool in_new_space = zone->isolate()->heap()->InNewSpace(*target);
    HCheckValue* check = new(zone) HCheckValue(value, target, in_new_space);
    return check;
  }
  static HCheckValue* New(Zone* zone, HValue* context,
                          HValue* value, Handle<Map> map, UniqueValueId id) {
    HCheckValue* check = new(zone) HCheckValue(value, map, false);
    check->object_unique_id_ = id;
    return check;
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual HValue* Canonicalize() V8_OVERRIDE;

#ifdef DEBUG
  virtual void Verify() V8_OVERRIDE;
#endif

  virtual void FinalizeUniqueValueId() V8_OVERRIDE {
    object_unique_id_ = UniqueValueId(object_);
  }

  Handle<HeapObject> object() const { return object_; }
  bool object_in_new_space() const { return object_in_new_space_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckValue)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HCheckValue* b = HCheckValue::cast(other);
    return object_unique_id_ == b->object_unique_id_;
  }

 private:
  HCheckValue(HValue* value, Handle<HeapObject> object, bool in_new_space)
      : HUnaryOperation(value, value->type()),
        object_(object), object_in_new_space_(in_new_space) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  Handle<HeapObject> object_;
  UniqueValueId object_unique_id_;
  bool object_in_new_space_;
};


class HCheckInstanceType V8_FINAL : public HUnaryOperation {
 public:
  static HCheckInstanceType* NewIsSpecObject(HValue* value, Zone* zone) {
    return new(zone) HCheckInstanceType(value, IS_SPEC_OBJECT);
  }
  static HCheckInstanceType* NewIsJSArray(HValue* value, Zone* zone) {
    return new(zone) HCheckInstanceType(value, IS_JS_ARRAY);
  }
  static HCheckInstanceType* NewIsString(HValue* value, Zone* zone) {
    return new(zone) HCheckInstanceType(value, IS_STRING);
  }
  static HCheckInstanceType* NewIsInternalizedString(
      HValue* value, Zone* zone) {
    return new(zone) HCheckInstanceType(value, IS_INTERNALIZED_STRING);
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual HValue* Canonicalize() V8_OVERRIDE;

  bool is_interval_check() const { return check_ <= LAST_INTERVAL_CHECK; }
  void GetCheckInterval(InstanceType* first, InstanceType* last);
  void GetCheckMaskAndTag(uint8_t* mask, uint8_t* tag);

  DECLARE_CONCRETE_INSTRUCTION(CheckInstanceType)

 protected:
  // TODO(ager): It could be nice to allow the ommision of instance
  // type checks if we have already performed an instance type check
  // with a larger range.
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HCheckInstanceType* b = HCheckInstanceType::cast(other);
    return check_ == b->check_;
  }

  virtual int RedefinedOperandIndex() { return 0; }

 private:
  enum Check {
    IS_SPEC_OBJECT,
    IS_JS_ARRAY,
    IS_STRING,
    IS_INTERNALIZED_STRING,
    LAST_INTERVAL_CHECK = IS_JS_ARRAY
  };

  const char* GetCheckName();

  HCheckInstanceType(HValue* value, Check check)
      : HUnaryOperation(value), check_(check) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  const Check check_;
};


class HCheckSmi V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCheckSmi, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual HValue* Canonicalize() V8_OVERRIDE {
    HType value_type = value()->type();
    if (value_type.IsSmi()) {
      return NULL;
    }
    return this;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckSmi)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HCheckSmi(HValue* value) : HUnaryOperation(value, HType::Smi()) {
    set_representation(Representation::Smi());
    SetFlag(kUseGVN);
  }
};


class HIsNumberAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  explicit HIsNumberAndBranch(HValue* value)
    : HUnaryControlInstruction(value, NULL, NULL) {
    SetFlag(kFlexibleRepresentation);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsNumberAndBranch)
};


class HCheckHeapObject V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCheckHeapObject, HValue*);

  virtual bool HasEscapingOperandAt(int index) V8_OVERRIDE { return false; }
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

#ifdef DEBUG
  virtual void Verify() V8_OVERRIDE;
#endif

  virtual HValue* Canonicalize() V8_OVERRIDE {
    return value()->type().IsHeapObject() ? NULL : this;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckHeapObject)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HCheckHeapObject(HValue* value)
      : HUnaryOperation(value, HType::NonPrimitive()) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }
};


class InductionVariableData;


struct InductionVariableLimitUpdate {
  InductionVariableData* updated_variable;
  HValue* limit;
  bool limit_is_upper;
  bool limit_is_included;

  InductionVariableLimitUpdate()
      : updated_variable(NULL), limit(NULL),
        limit_is_upper(false), limit_is_included(false) {}
};


class HBoundsCheck;
class HPhi;
class HConstant;
class HBitwise;


class InductionVariableData V8_FINAL : public ZoneObject {
 public:
  class InductionVariableCheck : public ZoneObject {
   public:
    HBoundsCheck* check() { return check_; }
    InductionVariableCheck* next() { return next_; }
    bool HasUpperLimit() { return upper_limit_ >= 0; }
    int32_t upper_limit() {
      ASSERT(HasUpperLimit());
      return upper_limit_;
    }
    void set_upper_limit(int32_t upper_limit) {
      upper_limit_ = upper_limit;
    }

    bool processed() { return processed_; }
    void set_processed() { processed_ = true; }

    InductionVariableCheck(HBoundsCheck* check,
                           InductionVariableCheck* next,
                           int32_t upper_limit = kNoLimit)
        : check_(check), next_(next), upper_limit_(upper_limit),
          processed_(false) {}

   private:
    HBoundsCheck* check_;
    InductionVariableCheck* next_;
    int32_t upper_limit_;
    bool processed_;
  };

  class ChecksRelatedToLength : public ZoneObject {
   public:
    HValue* length() { return length_; }
    ChecksRelatedToLength* next() { return next_; }
    InductionVariableCheck* checks() { return checks_; }

    void AddCheck(HBoundsCheck* check, int32_t upper_limit = kNoLimit);
    void CloseCurrentBlock();

    ChecksRelatedToLength(HValue* length, ChecksRelatedToLength* next)
      : length_(length), next_(next), checks_(NULL),
        first_check_in_block_(NULL),
        added_index_(NULL),
        added_constant_(NULL),
        current_and_mask_in_block_(0),
        current_or_mask_in_block_(0) {}

   private:
    void UseNewIndexInCurrentBlock(Token::Value token,
                                   int32_t mask,
                                   HValue* index_base,
                                   HValue* context);

    HBoundsCheck* first_check_in_block() { return first_check_in_block_; }
    HBitwise* added_index() { return added_index_; }
    void set_added_index(HBitwise* index) { added_index_ = index; }
    HConstant* added_constant() { return added_constant_; }
    void set_added_constant(HConstant* constant) { added_constant_ = constant; }
    int32_t current_and_mask_in_block() { return current_and_mask_in_block_; }
    int32_t current_or_mask_in_block() { return current_or_mask_in_block_; }
    int32_t current_upper_limit() { return current_upper_limit_; }

    HValue* length_;
    ChecksRelatedToLength* next_;
    InductionVariableCheck* checks_;

    HBoundsCheck* first_check_in_block_;
    HBitwise* added_index_;
    HConstant* added_constant_;
    int32_t current_and_mask_in_block_;
    int32_t current_or_mask_in_block_;
    int32_t current_upper_limit_;
  };

  struct LimitFromPredecessorBlock {
    InductionVariableData* variable;
    Token::Value token;
    HValue* limit;
    HBasicBlock* other_target;

    bool LimitIsValid() { return token != Token::ILLEGAL; }

    bool LimitIsIncluded() {
      return Token::IsEqualityOp(token) ||
          token == Token::GTE || token == Token::LTE;
    }
    bool LimitIsUpper() {
      return token == Token::LTE || token == Token::LT || token == Token::NE;
    }

    LimitFromPredecessorBlock()
        : variable(NULL),
          token(Token::ILLEGAL),
          limit(NULL),
          other_target(NULL) {}
  };

  static const int32_t kNoLimit = -1;

  static InductionVariableData* ExaminePhi(HPhi* phi);
  static void ComputeLimitFromPredecessorBlock(
      HBasicBlock* block,
      LimitFromPredecessorBlock* result);
  static bool ComputeInductionVariableLimit(
      HBasicBlock* block,
      InductionVariableLimitUpdate* additional_limit);

  struct BitwiseDecompositionResult {
    HValue* base;
    int32_t and_mask;
    int32_t or_mask;
    HValue* context;

    BitwiseDecompositionResult()
        : base(NULL), and_mask(0), or_mask(0), context(NULL) {}
  };
  static void DecomposeBitwise(HValue* value,
                               BitwiseDecompositionResult* result);

  void AddCheck(HBoundsCheck* check, int32_t upper_limit = kNoLimit);

  bool CheckIfBranchIsLoopGuard(Token::Value token,
                                HBasicBlock* current_branch,
                                HBasicBlock* other_branch);

  void UpdateAdditionalLimit(InductionVariableLimitUpdate* update);

  HPhi* phi() { return phi_; }
  HValue* base() { return base_; }
  int32_t increment() { return increment_; }
  HValue* limit() { return limit_; }
  bool limit_included() { return limit_included_; }
  HBasicBlock* limit_validity() { return limit_validity_; }
  HBasicBlock* induction_exit_block() { return induction_exit_block_; }
  HBasicBlock* induction_exit_target() { return induction_exit_target_; }
  ChecksRelatedToLength* checks() { return checks_; }
  HValue* additional_upper_limit() { return additional_upper_limit_; }
  bool additional_upper_limit_is_included() {
    return additional_upper_limit_is_included_;
  }
  HValue* additional_lower_limit() { return additional_lower_limit_; }
  bool additional_lower_limit_is_included() {
    return additional_lower_limit_is_included_;
  }

  bool LowerLimitIsNonNegativeConstant() {
    if (base()->IsInteger32Constant() && base()->GetInteger32Constant() >= 0) {
      return true;
    }
    if (additional_lower_limit() != NULL &&
        additional_lower_limit()->IsInteger32Constant() &&
        additional_lower_limit()->GetInteger32Constant() >= 0) {
      // Ignoring the corner case of !additional_lower_limit_is_included()
      // is safe, handling it adds unneeded complexity.
      return true;
    }
    return false;
  }

  int32_t ComputeUpperLimit(int32_t and_mask, int32_t or_mask);

 private:
  template <class T> void swap(T* a, T* b) {
    T c(*a);
    *a = *b;
    *b = c;
  }

  InductionVariableData(HPhi* phi, HValue* base, int32_t increment)
      : phi_(phi), base_(IgnoreOsrValue(base)), increment_(increment),
        limit_(NULL), limit_included_(false), limit_validity_(NULL),
        induction_exit_block_(NULL), induction_exit_target_(NULL),
        checks_(NULL),
        additional_upper_limit_(NULL),
        additional_upper_limit_is_included_(false),
        additional_lower_limit_(NULL),
        additional_lower_limit_is_included_(false) {}

  static int32_t ComputeIncrement(HPhi* phi, HValue* phi_operand);

  static HValue* IgnoreOsrValue(HValue* v);
  static InductionVariableData* GetInductionVariableData(HValue* v);

  HPhi* phi_;
  HValue* base_;
  int32_t increment_;
  HValue* limit_;
  bool limit_included_;
  HBasicBlock* limit_validity_;
  HBasicBlock* induction_exit_block_;
  HBasicBlock* induction_exit_target_;
  ChecksRelatedToLength* checks_;
  HValue* additional_upper_limit_;
  bool additional_upper_limit_is_included_;
  HValue* additional_lower_limit_;
  bool additional_lower_limit_is_included_;
};


class HPhi V8_FINAL : public HValue {
 public:
  HPhi(int merged_index, Zone* zone)
      : inputs_(2, zone),
        merged_index_(merged_index),
        phi_id_(-1),
        induction_variable_data_(NULL) {
    for (int i = 0; i < Representation::kNumRepresentations; i++) {
      non_phi_uses_[i] = 0;
      indirect_uses_[i] = 0;
    }
    ASSERT(merged_index >= 0 || merged_index == kInvalidMergedIndex);
    SetFlag(kFlexibleRepresentation);
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual Representation RepresentationFromInputs() V8_OVERRIDE;

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;
  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) V8_OVERRIDE;
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return representation();
  }
  virtual Representation KnownOptimalRepresentation() V8_OVERRIDE {
    return representation();
  }
  virtual HType CalculateInferredType() V8_OVERRIDE;
  virtual int OperandCount() V8_OVERRIDE { return inputs_.length(); }
  virtual HValue* OperandAt(int index) const V8_OVERRIDE {
    return inputs_[index];
  }
  HValue* GetRedundantReplacement();
  void AddInput(HValue* value);
  bool HasRealUses();

  bool IsReceiver() const { return merged_index_ == 0; }
  bool HasMergedIndex() const { return merged_index_ != kInvalidMergedIndex; }

  int merged_index() const { return merged_index_; }

  InductionVariableData* induction_variable_data() {
    return induction_variable_data_;
  }
  bool IsInductionVariable() {
    return induction_variable_data_ != NULL;
  }
  bool IsLimitedInductionVariable() {
    return IsInductionVariable() &&
        induction_variable_data_->limit() != NULL;
  }
  void DetectInductionVariable() {
    ASSERT(induction_variable_data_ == NULL);
    induction_variable_data_ = InductionVariableData::ExaminePhi(this);
  }

  virtual void PrintTo(StringStream* stream) V8_OVERRIDE;

#ifdef DEBUG
  virtual void Verify() V8_OVERRIDE;
#endif

  void InitRealUses(int id);
  void AddNonPhiUsesFrom(HPhi* other);
  void AddIndirectUsesTo(int* use_count);

  int tagged_non_phi_uses() const {
    return non_phi_uses_[Representation::kTagged];
  }
  int smi_non_phi_uses() const {
    return non_phi_uses_[Representation::kSmi];
  }
  int int32_non_phi_uses() const {
    return non_phi_uses_[Representation::kInteger32];
  }
  int double_non_phi_uses() const {
    return non_phi_uses_[Representation::kDouble];
  }
  int tagged_indirect_uses() const {
    return indirect_uses_[Representation::kTagged];
  }
  int smi_indirect_uses() const {
    return indirect_uses_[Representation::kSmi];
  }
  int int32_indirect_uses() const {
    return indirect_uses_[Representation::kInteger32];
  }
  int double_indirect_uses() const {
    return indirect_uses_[Representation::kDouble];
  }
  int phi_id() { return phi_id_; }

  static HPhi* cast(HValue* value) {
    ASSERT(value->IsPhi());
    return reinterpret_cast<HPhi*>(value);
  }
  virtual Opcode opcode() const V8_OVERRIDE { return HValue::kPhi; }

  void SimplifyConstantInputs();

  // Marker value representing an invalid merge index.
  static const int kInvalidMergedIndex = -1;

 protected:
  virtual void DeleteFromGraph() V8_OVERRIDE;
  virtual void InternalSetOperandAt(int index, HValue* value) V8_OVERRIDE {
    inputs_[index] = value;
  }

 private:
  ZoneList<HValue*> inputs_;
  int merged_index_;

  int non_phi_uses_[Representation::kNumRepresentations];
  int indirect_uses_[Representation::kNumRepresentations];
  int phi_id_;
  InductionVariableData* induction_variable_data_;

  // TODO(titzer): we can't eliminate the receiver for generating backtraces
  virtual bool IsDeletable() const V8_OVERRIDE { return !IsReceiver(); }
};


// Common base class for HArgumentsObject and HCapturedObject.
class HDematerializedObject : public HInstruction {
 public:
  HDematerializedObject(int count, Zone* zone) : values_(count, zone) {}

  virtual int OperandCount() V8_FINAL V8_OVERRIDE { return values_.length(); }
  virtual HValue* OperandAt(int index) const V8_FINAL V8_OVERRIDE {
    return values_[index];
  }

  virtual bool HasEscapingOperandAt(int index) V8_FINAL V8_OVERRIDE {
    return false;
  }
  virtual Representation RequiredInputRepresentation(
      int index) V8_FINAL V8_OVERRIDE {
    return Representation::None();
  }

 protected:
  virtual void InternalSetOperandAt(int index,
                                    HValue* value) V8_FINAL V8_OVERRIDE {
    values_[index] = value;
  }

  // List of values tracked by this marker.
  ZoneList<HValue*> values_;

 private:
  virtual bool IsDeletable() const V8_FINAL V8_OVERRIDE { return true; }
};


class HArgumentsObject V8_FINAL : public HDematerializedObject {
 public:
  static HArgumentsObject* New(Zone* zone, HValue* context, int count) {
    return new(zone) HArgumentsObject(count, zone);
  }

  // The values contain a list of all elements in the arguments object
  // including the receiver object, which is skipped when materializing.
  const ZoneList<HValue*>* arguments_values() const { return &values_; }
  int arguments_count() const { return values_.length(); }

  void AddArgument(HValue* argument, Zone* zone) {
    values_.Add(NULL, zone);  // Resize list.
    SetOperandAt(values_.length() - 1, argument);
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsObject)

 private:
  HArgumentsObject(int count, Zone* zone)
      : HDematerializedObject(count, zone) {
    set_representation(Representation::Tagged());
    SetFlag(kIsArguments);
  }
};


class HCapturedObject V8_FINAL : public HDematerializedObject {
 public:
  HCapturedObject(int length, int id, Zone* zone)
      : HDematerializedObject(length, zone), capture_id_(id) {
    set_representation(Representation::Tagged());
    values_.AddBlock(NULL, length, zone);  // Resize list.
  }

  // The values contain a list of all in-object properties inside the
  // captured object and is index by field index. Properties in the
  // properties or elements backing store are not tracked here.
  const ZoneList<HValue*>* values() const { return &values_; }
  int length() const { return values_.length(); }
  int capture_id() const { return capture_id_; }

  // Shortcut for the map value of this captured object.
  HValue* map_value() const { return values()->first(); }

  void ReuseSideEffectsFromStore(HInstruction* store) {
    ASSERT(store->HasObservableSideEffects());
    ASSERT(store->IsStoreNamedField());
    gvn_flags_.Add(store->gvn_flags());
  }

  // Replay effects of this instruction on the given environment.
  void ReplayEnvironment(HEnvironment* env);

  DECLARE_CONCRETE_INSTRUCTION(CapturedObject)

 private:
  int capture_id_;
};


class HConstant V8_FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, int32_t);
  DECLARE_INSTRUCTION_FACTORY_P2(HConstant, int32_t, Representation);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, double);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, Handle<Object>);
  DECLARE_INSTRUCTION_FACTORY_P2(HConstant, Handle<Map>, UniqueValueId);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, ExternalReference);

  static HConstant* CreateAndInsertAfter(Zone* zone,
                                         HValue* context,
                                         int32_t value,
                                         Representation representation,
                                         HInstruction* instruction) {
    HConstant* new_constant =
        HConstant::New(zone, context, value, representation);
    new_constant->InsertAfter(instruction);
    return new_constant;
  }

  static HConstant* CreateAndInsertBefore(Zone* zone,
                                          HValue* context,
                                          int32_t value,
                                          Representation representation,
                                          HInstruction* instruction) {
    HConstant* new_constant =
        HConstant::New(zone, context, value, representation);
    new_constant->InsertBefore(instruction);
    return new_constant;
  }

  Handle<Object> handle(Isolate* isolate) {
    if (handle_.is_null()) {
      Factory* factory = isolate->factory();
      // Default arguments to is_not_in_new_space depend on this heap number
      // to be tenured so that it's guaranteed not be be located in new space.
      handle_ = factory->NewNumber(double_value_, TENURED);
    }
    AllowDeferredHandleDereference smi_check;
    ASSERT(has_int32_value_ || !handle_->IsSmi());
    return handle_;
  }

  bool HasMap(Handle<Map> map) {
    Handle<Object> constant_object = handle(map->GetIsolate());
    return constant_object->IsHeapObject() &&
        Handle<HeapObject>::cast(constant_object)->map() == *map;
  }

  bool IsSpecialDouble() const {
    return has_double_value_ &&
        (BitCast<int64_t>(double_value_) == BitCast<int64_t>(-0.0) ||
         FixedDoubleArray::is_the_hole_nan(double_value_) ||
         std::isnan(double_value_));
  }

  bool NotInNewSpace() const {
    return is_not_in_new_space_;
  }

  bool ImmortalImmovable() const {
    if (has_int32_value_) {
      return false;
    }
    if (has_double_value_) {
      if (IsSpecialDouble()) {
        return true;
      }
      return false;
    }
    if (has_external_reference_value_) {
      return false;
    }

    ASSERT(!handle_.is_null());
    Heap* heap = isolate()->heap();
    ASSERT(unique_id_ != UniqueValueId::minus_zero_value(heap));
    ASSERT(unique_id_ != UniqueValueId::nan_value(heap));
    return unique_id_ == UniqueValueId::undefined_value(heap) ||
           unique_id_ == UniqueValueId::null_value(heap) ||
           unique_id_ == UniqueValueId::true_value(heap) ||
           unique_id_ == UniqueValueId::false_value(heap) ||
           unique_id_ == UniqueValueId::the_hole_value(heap) ||
           unique_id_ == UniqueValueId::empty_string(heap);
  }

  bool IsCell() const {
    return is_cell_;
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  virtual Representation KnownOptimalRepresentation() V8_OVERRIDE {
    if (HasSmiValue() && SmiValuesAre31Bits()) return Representation::Smi();
    if (HasInteger32Value()) return Representation::Integer32();
    if (HasNumberValue()) return Representation::Double();
    if (HasExternalReferenceValue()) return Representation::External();
    return Representation::Tagged();
  }

  virtual bool EmitAtUses() V8_OVERRIDE;
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
  HConstant* CopyToRepresentation(Representation r, Zone* zone) const;
  Maybe<HConstant*> CopyToTruncatedInt32(Zone* zone);
  Maybe<HConstant*> CopyToTruncatedNumber(Zone* zone);
  bool HasInteger32Value() const { return has_int32_value_; }
  int32_t Integer32Value() const {
    ASSERT(HasInteger32Value());
    return int32_value_;
  }
  bool HasSmiValue() const { return has_smi_value_; }
  bool HasDoubleValue() const { return has_double_value_; }
  double DoubleValue() const {
    ASSERT(HasDoubleValue());
    return double_value_;
  }
  bool IsTheHole() const {
    if (HasDoubleValue() && FixedDoubleArray::is_the_hole_nan(double_value_)) {
      return true;
    }
    Heap* heap = isolate()->heap();
    if (!handle_.is_null() && *handle_ == heap->the_hole_value()) {
      return true;
    }
    return false;
  }
  bool HasNumberValue() const { return has_double_value_; }
  int32_t NumberValueAsInteger32() const {
    ASSERT(HasNumberValue());
    // Irrespective of whether a numeric HConstant can be safely
    // represented as an int32, we store the (in some cases lossy)
    // representation of the number in int32_value_.
    return int32_value_;
  }
  bool HasStringValue() const {
    if (has_double_value_ || has_int32_value_) return false;
    ASSERT(!handle_.is_null());
    return type_.IsString();
  }
  Handle<String> StringValue() const {
    ASSERT(HasStringValue());
    return Handle<String>::cast(handle_);
  }
  bool HasInternalizedStringValue() const {
    return HasStringValue() && is_internalized_string_;
  }

  bool HasExternalReferenceValue() const {
    return has_external_reference_value_;
  }
  ExternalReference ExternalReferenceValue() const {
    return external_reference_value_;
  }

  bool HasBooleanValue() const { return type_.IsBoolean(); }
  bool BooleanValue() const { return boolean_value_; }

  virtual intptr_t Hashcode() V8_OVERRIDE {
    if (has_int32_value_) {
      return static_cast<intptr_t>(int32_value_);
    } else if (has_double_value_) {
      return static_cast<intptr_t>(BitCast<int64_t>(double_value_));
    } else if (has_external_reference_value_) {
      return reinterpret_cast<intptr_t>(external_reference_value_.address());
    } else {
      ASSERT(!handle_.is_null());
      return unique_id_.Hashcode();
    }
  }

  virtual void FinalizeUniqueValueId() V8_OVERRIDE {
    if (!has_double_value_ && !has_external_reference_value_) {
      ASSERT(!handle_.is_null());
      unique_id_ = UniqueValueId(handle_);
    }
  }

  bool UniqueValueIdsMatch(UniqueValueId other) {
    return !has_double_value_ && !has_external_reference_value_ &&
        unique_id_ == other;
  }

#ifdef DEBUG
  virtual void Verify() V8_OVERRIDE { }
#endif

  DECLARE_CONCRETE_INSTRUCTION(Constant)

 protected:
  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HConstant* other_constant = HConstant::cast(other);
    if (has_int32_value_) {
      return other_constant->has_int32_value_ &&
          int32_value_ == other_constant->int32_value_;
    } else if (has_double_value_) {
      return other_constant->has_double_value_ &&
          BitCast<int64_t>(double_value_) ==
          BitCast<int64_t>(other_constant->double_value_);
    } else if (has_external_reference_value_) {
      return other_constant->has_external_reference_value_ &&
          external_reference_value_ ==
          other_constant->external_reference_value_;
    } else {
      ASSERT(!handle_.is_null());
      return !other_constant->handle_.is_null() &&
             unique_id_ == other_constant->unique_id_;
    }
  }

 private:
  friend class HGraph;
  HConstant(Handle<Object> handle, Representation r = Representation::None());
  HConstant(int32_t value,
            Representation r = Representation::None(),
            bool is_not_in_new_space = true,
            Handle<Object> optional_handle = Handle<Object>::null());
  HConstant(double value,
            Representation r = Representation::None(),
            bool is_not_in_new_space = true,
            Handle<Object> optional_handle = Handle<Object>::null());
  HConstant(Handle<Object> handle,
            UniqueValueId unique_id,
            Representation r,
            HType type,
            bool is_internalized_string,
            bool is_not_in_new_space,
            bool is_cell,
            bool boolean_value);
  HConstant(Handle<Map> handle,
            UniqueValueId unique_id);
  explicit HConstant(ExternalReference reference);

  void Initialize(Representation r);

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  // If this is a numerical constant, handle_ either points to to the
  // HeapObject the constant originated from or is null.  If the
  // constant is non-numeric, handle_ always points to a valid
  // constant HeapObject.
  Handle<Object> handle_;
  UniqueValueId unique_id_;

  // We store the HConstant in the most specific form safely possible.
  // The two flags, has_int32_value_ and has_double_value_ tell us if
  // int32_value_ and double_value_ hold valid, safe representations
  // of the constant.  has_int32_value_ implies has_double_value_ but
  // not the converse.
  bool has_smi_value_ : 1;
  bool has_int32_value_ : 1;
  bool has_double_value_ : 1;
  bool has_external_reference_value_ : 1;
  bool is_internalized_string_ : 1;  // TODO(yangguo): make this part of HType.
  bool is_not_in_new_space_ : 1;
  bool is_cell_ : 1;
  bool boolean_value_ : 1;
  int32_t int32_value_;
  double double_value_;
  ExternalReference external_reference_value_;
};


class HBinaryOperation : public HTemplateInstruction<3> {
 public:
  HBinaryOperation(HValue* context, HValue* left, HValue* right,
                   HType type = HType::Tagged())
      : HTemplateInstruction<3>(type),
        observed_output_representation_(Representation::None()) {
    ASSERT(left != NULL && right != NULL);
    SetOperandAt(0, context);
    SetOperandAt(1, left);
    SetOperandAt(2, right);
    observed_input_representation_[0] = Representation::None();
    observed_input_representation_[1] = Representation::None();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* left() const { return OperandAt(1); }
  HValue* right() const { return OperandAt(2); }

  // True if switching left and right operands likely generates better code.
  bool AreOperandsBetterSwitched() {
    if (!IsCommutative()) return false;

    // Constant operands are better off on the right, they can be inlined in
    // many situations on most platforms.
    if (left()->IsConstant()) return true;
    if (right()->IsConstant()) return false;

    // Otherwise, if there is only one use of the right operand, it would be
    // better off on the left for platforms that only have 2-arg arithmetic
    // ops (e.g ia32, x64) that clobber the left operand.
    return right()->UseCount() == 1;
  }

  HValue* BetterLeftOperand() {
    return AreOperandsBetterSwitched() ? right() : left();
  }

  HValue* BetterRightOperand() {
    return AreOperandsBetterSwitched() ? left() : right();
  }

  void set_observed_input_representation(int index, Representation rep) {
    ASSERT(index >= 1 && index <= 2);
    observed_input_representation_[index - 1] = rep;
  }

  virtual void initialize_output_representation(Representation observed) {
    observed_output_representation_ = observed;
  }

  virtual Representation observed_input_representation(int index) V8_OVERRIDE {
    if (index == 0) return Representation::Tagged();
    return observed_input_representation_[index - 1];
  }

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) V8_OVERRIDE {
    Representation rep = !FLAG_smi_binop && new_rep.IsSmi()
        ? Representation::Integer32() : new_rep;
    HValue::UpdateRepresentation(rep, h_infer, reason);
  }

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) V8_OVERRIDE;
  virtual Representation RepresentationFromInputs() V8_OVERRIDE;
  Representation RepresentationFromOutput();
  virtual void AssumeRepresentation(Representation r) V8_OVERRIDE;

  virtual bool IsCommutative() const { return false; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    if (index == 0) return Representation::Tagged();
    return representation();
  }

  DECLARE_ABSTRACT_INSTRUCTION(BinaryOperation)

 private:
  bool IgnoreObservedOutputRepresentation(Representation current_rep);

  Representation observed_input_representation_[2];
  Representation observed_output_representation_;
};


class HWrapReceiver V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HWrapReceiver, HValue*, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* receiver() { return OperandAt(0); }
  HValue* function() { return OperandAt(1); }

  virtual HValue* Canonicalize() V8_OVERRIDE;

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(WrapReceiver)

 private:
  HWrapReceiver(HValue* receiver, HValue* function) {
    set_representation(Representation::Tagged());
    SetOperandAt(0, receiver);
    SetOperandAt(1, function);
  }
};


class HApplyArguments V8_FINAL : public HTemplateInstruction<4> {
 public:
  HApplyArguments(HValue* function,
                  HValue* receiver,
                  HValue* length,
                  HValue* elements) {
    set_representation(Representation::Tagged());
    SetOperandAt(0, function);
    SetOperandAt(1, receiver);
    SetOperandAt(2, length);
    SetOperandAt(3, elements);
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    // The length is untagged, all other inputs are tagged.
    return (index == 2)
        ? Representation::Integer32()
        : Representation::Tagged();
  }

  HValue* function() { return OperandAt(0); }
  HValue* receiver() { return OperandAt(1); }
  HValue* length() { return OperandAt(2); }
  HValue* elements() { return OperandAt(3); }

  DECLARE_CONCRETE_INSTRUCTION(ApplyArguments)
};


class HArgumentsElements V8_FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HArgumentsElements, bool);

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  bool from_inlined() const { return from_inlined_; }

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HArgumentsElements(bool from_inlined) : from_inlined_(from_inlined) {
    // The value produced by this instruction is a pointer into the stack
    // that looks as if it was a smi because of alignment.
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  bool from_inlined_;
};


class HArgumentsLength V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HArgumentsLength, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HArgumentsLength(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HAccessArgumentsAt V8_FINAL : public HTemplateInstruction<3> {
 public:
  HAccessArgumentsAt(HValue* arguments, HValue* length, HValue* index) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetOperandAt(0, arguments);
    SetOperandAt(1, length);
    SetOperandAt(2, index);
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    // The arguments elements is considered tagged.
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }

  HValue* arguments() { return OperandAt(0); }
  HValue* length() { return OperandAt(1); }
  HValue* index() { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(AccessArgumentsAt)

  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }
};


class HBoundsCheckBaseIndexInformation;


class HBoundsCheck V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HBoundsCheck, HValue*, HValue*);

  bool skip_check() const { return skip_check_; }
  void set_skip_check() { skip_check_ = true; }

  HValue* base() { return base_; }
  int offset() { return offset_; }
  int scale() { return scale_; }

  void ApplyIndexChange();
  bool DetectCompoundIndex() {
    ASSERT(base() == NULL);

    DecompositionResult decomposition;
    if (index()->TryDecompose(&decomposition)) {
      base_ = decomposition.base();
      offset_ = decomposition.offset();
      scale_ = decomposition.scale();
      return true;
    } else {
      base_ = index();
      offset_ = 0;
      scale_ = 0;
      return false;
    }
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return representation();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) V8_OVERRIDE;

  HValue* index() { return OperandAt(0); }
  HValue* length() { return OperandAt(1); }
  bool allow_equality() { return allow_equality_; }
  void set_allow_equality(bool v) { allow_equality_ = v; }

  virtual int RedefinedOperandIndex() V8_OVERRIDE { return 0; }
  virtual bool IsPurelyInformativeDefinition() V8_OVERRIDE {
    return skip_check();
  }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheck)

 protected:
  friend class HBoundsCheckBaseIndexInformation;

  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }
  bool skip_check_;
  HValue* base_;
  int offset_;
  int scale_;
  bool allow_equality_;

 private:
  // Normally HBoundsCheck should be created using the
  // HGraphBuilder::AddBoundsCheck() helper.
  // However when building stubs, where we know that the arguments are Int32,
  // it makes sense to invoke this constructor directly.
  HBoundsCheck(HValue* index, HValue* length)
    : skip_check_(false),
      base_(NULL), offset_(0), scale_(0),
      allow_equality_(false) {
    SetOperandAt(0, index);
    SetOperandAt(1, length);
    SetFlag(kFlexibleRepresentation);
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE {
    return skip_check() && !FLAG_debug_code;
  }
};


class HBoundsCheckBaseIndexInformation V8_FINAL
    : public HTemplateInstruction<2> {
 public:
  explicit HBoundsCheckBaseIndexInformation(HBoundsCheck* check) {
    DecompositionResult decomposition;
    if (check->index()->TryDecompose(&decomposition)) {
      SetOperandAt(0, decomposition.base());
      SetOperandAt(1, check);
    } else {
      UNREACHABLE();
    }
  }

  HValue* base_index() { return OperandAt(0); }
  HBoundsCheck* bounds_check() { return HBoundsCheck::cast(OperandAt(1)); }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheckBaseIndexInformation)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return representation();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual int RedefinedOperandIndex() V8_OVERRIDE { return 0; }
  virtual bool IsPurelyInformativeDefinition() V8_OVERRIDE { return true; }
};


class HBitwiseBinaryOperation : public HBinaryOperation {
 public:
  HBitwiseBinaryOperation(HValue* context, HValue* left, HValue* right,
                          HType type = HType::Tagged())
      : HBinaryOperation(context, left, right, type) {
    SetFlag(kFlexibleRepresentation);
    SetFlag(kTruncatingToInt32);
    SetFlag(kAllowUndefinedAsNaN);
    SetAllSideEffects();
  }

  virtual void RepresentationChanged(Representation to) V8_OVERRIDE {
    if (!to.IsTagged()) {
      ASSERT(to.IsSmiOrInteger32());
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    } else {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    }
  }

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) V8_OVERRIDE {
    // We only generate either int32 or generic tagged bitwise operations.
    if (new_rep.IsDouble()) new_rep = Representation::Integer32();
    HBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  virtual Representation observed_input_representation(int index) V8_OVERRIDE {
    Representation r = HBinaryOperation::observed_input_representation(index);
    if (r.IsDouble()) return Representation::Integer32();
    return r;
  }

  virtual void initialize_output_representation(Representation observed) {
    if (observed.IsDouble()) observed = Representation::Integer32();
    HBinaryOperation::initialize_output_representation(observed);
  }

  DECLARE_ABSTRACT_INSTRUCTION(BitwiseBinaryOperation)

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HMathFloorOfDiv V8_FINAL : public HBinaryOperation {
 public:
  static HMathFloorOfDiv* New(Zone* zone,
                              HValue* context,
                              HValue* left,
                              HValue* right) {
    return new(zone) HMathFloorOfDiv(context, left, right);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(
      BitVector* visited) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(MathFloorOfDiv)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  HMathFloorOfDiv(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetFlag(kCanOverflow);
    if (!right->IsConstant()) {
      SetFlag(kCanBeDivByZero);
    }
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HArithmeticBinaryOperation : public HBinaryOperation {
 public:
  HArithmeticBinaryOperation(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right, HType::TaggedNumber()) {
    SetAllSideEffects();
    SetFlag(kFlexibleRepresentation);
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual void RepresentationChanged(Representation to) V8_OVERRIDE {
    if (to.IsTagged()) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
  }

  DECLARE_ABSTRACT_INSTRUCTION(ArithmeticBinaryOperation)

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HCompareGeneric V8_FINAL : public HBinaryOperation {
 public:
  HCompareGeneric(HValue* context,
                  HValue* left,
                  HValue* right,
                  Token::Value token)
      : HBinaryOperation(context, left, right, HType::Boolean()),
        token_(token) {
    ASSERT(Token::IsCompareOp(token));
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return index == 0
        ? Representation::Tagged()
        : representation();
  }

  Token::Value token() const { return token_; }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CompareGeneric)

 private:
  Token::Value token_;
};


class HCompareNumericAndBranch : public HTemplateControlInstruction<2, 2> {
 public:
  HCompareNumericAndBranch(HValue* left, HValue* right, Token::Value token)
      : token_(token) {
    SetFlag(kFlexibleRepresentation);
    ASSERT(Token::IsCompareOp(token));
    SetOperandAt(0, left);
    SetOperandAt(1, right);
  }

  HValue* left() { return OperandAt(0); }
  HValue* right() { return OperandAt(1); }
  Token::Value token() const { return token_; }

  void set_observed_input_representation(Representation left,
                                         Representation right) {
      observed_input_representation_[0] = left;
      observed_input_representation_[1] = right;
  }

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return representation();
  }
  virtual Representation observed_input_representation(int index) V8_OVERRIDE {
    return observed_input_representation_[index];
  }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CompareNumericAndBranch)

 private:
  Representation observed_input_representation_[2];
  Token::Value token_;
};


class HCompareHoleAndBranch V8_FINAL
    : public HTemplateControlInstruction<2, 1> {
 public:
  // TODO(danno): make this private when the IfBuilder properly constructs
  // control flow instructions.
  explicit HCompareHoleAndBranch(HValue* object) {
    SetFlag(kFlexibleRepresentation);
    SetFlag(kAllowUndefinedAsNaN);
    SetOperandAt(0, object);
  }

  DECLARE_INSTRUCTION_FACTORY_P1(HCompareHoleAndBranch, HValue*);

  HValue* object() { return OperandAt(0); }

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return representation();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CompareHoleAndBranch)
};


class HCompareObjectEqAndBranch : public HTemplateControlInstruction<2, 2> {
 public:
  // TODO(danno): make this private when the IfBuilder properly constructs
  // control flow instructions.
  HCompareObjectEqAndBranch(HValue* left,
                            HValue* right) {
    SetOperandAt(0, left);
    SetOperandAt(1, right);
  }

  DECLARE_INSTRUCTION_FACTORY_P2(HCompareObjectEqAndBranch, HValue*, HValue*);

  HValue* left() { return OperandAt(0); }
  HValue* right() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual Representation observed_input_representation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareObjectEqAndBranch)
};


class HIsObjectAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  explicit HIsObjectAndBranch(HValue* value)
    : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsObjectAndBranch)
};

class HIsStringAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  explicit HIsStringAndBranch(HValue* value)
    : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsStringAndBranch)
};


class HIsSmiAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  explicit HIsSmiAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }
};


class HIsUndetectableAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  explicit HIsUndetectableAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsUndetectableAndBranch)
};


class HStringCompareAndBranch : public HTemplateControlInstruction<2, 3> {
 public:
  HStringCompareAndBranch(HValue* context,
                           HValue* left,
                           HValue* right,
                           Token::Value token)
      : token_(token) {
    ASSERT(Token::IsCompareOp(token));
    SetOperandAt(0, context);
    SetOperandAt(1, left);
    SetOperandAt(2, right);
    set_representation(Representation::Tagged());
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  HValue* context() { return OperandAt(0); }
  HValue* left() { return OperandAt(1); }
  HValue* right() { return OperandAt(2); }
  Token::Value token() const { return token_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  Representation GetInputRepresentation() const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StringCompareAndBranch)

 private:
  Token::Value token_;
};


class HIsConstructCallAndBranch : public HTemplateControlInstruction<2, 0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsConstructCallAndBranch)
};


class HHasInstanceTypeAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  HHasInstanceTypeAndBranch(HValue* value, InstanceType type)
      : HUnaryControlInstruction(value, NULL, NULL), from_(type), to_(type) { }
  HHasInstanceTypeAndBranch(HValue* value, InstanceType from, InstanceType to)
      : HUnaryControlInstruction(value, NULL, NULL), from_(from), to_(to) {
    ASSERT(to == LAST_TYPE);  // Others not implemented yet in backend.
  }

  InstanceType from() { return from_; }
  InstanceType to() { return to_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceTypeAndBranch)

 private:
  InstanceType from_;
  InstanceType to_;  // Inclusive range, not all combinations work.
};


class HHasCachedArrayIndexAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  explicit HHasCachedArrayIndexAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndexAndBranch)
};


class HGetCachedArrayIndex V8_FINAL : public HUnaryOperation {
 public:
  explicit HGetCachedArrayIndex(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(GetCachedArrayIndex)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HClassOfTestAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  HClassOfTestAndBranch(HValue* value, Handle<String> class_name)
      : HUnaryControlInstruction(value, NULL, NULL),
        class_name_(class_name) { }

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<String> class_name() const { return class_name_; }

 private:
  Handle<String> class_name_;
};


class HTypeofIsAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  HTypeofIsAndBranch(HValue* value, Handle<String> type_literal)
      : HUnaryControlInstruction(value, NULL, NULL),
        type_literal_(type_literal) { }

  Handle<String> type_literal() { return type_literal_; }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(TypeofIsAndBranch)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

 private:
  Handle<String> type_literal_;
};


class HInstanceOf V8_FINAL : public HBinaryOperation {
 public:
  HInstanceOf(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right, HType::Boolean()) {
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(InstanceOf)
};


class HInstanceOfKnownGlobal V8_FINAL : public HTemplateInstruction<2> {
 public:
  HInstanceOfKnownGlobal(HValue* context,
                         HValue* left,
                         Handle<JSFunction> right)
      : HTemplateInstruction<2>(HType::Boolean()), function_(right) {
    SetOperandAt(0, context);
    SetOperandAt(1, left);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  HValue* context() { return OperandAt(0); }
  HValue* left() { return OperandAt(1); }
  Handle<JSFunction> function() { return function_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfKnownGlobal)

 private:
  Handle<JSFunction> function_;
};


// TODO(mstarzinger): This instruction should be modeled as a load of the map
// field followed by a load of the instance size field once HLoadNamedField is
// flexible enough to accommodate byte-field loads.
class HInstanceSize V8_FINAL : public HTemplateInstruction<1> {
 public:
  explicit HInstanceSize(HValue* object) {
    SetOperandAt(0, object);
    set_representation(Representation::Integer32());
  }

  HValue* object() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(InstanceSize)
};


class HPower V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  HValue* left() { return OperandAt(0); }
  HValue* right() const { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return index == 0
      ? Representation::Double()
      : Representation::None();
  }
  virtual Representation observed_input_representation(int index) V8_OVERRIDE {
    return RequiredInputRepresentation(index);
  }

  DECLARE_CONCRETE_INSTRUCTION(Power)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  HPower(HValue* left, HValue* right) {
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    set_representation(Representation::Double());
    SetFlag(kUseGVN);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  virtual bool IsDeletable() const V8_OVERRIDE {
    return !right()->representation().IsTagged();
  }
};


class HRandom V8_FINAL : public HTemplateInstruction<1> {
 public:
  explicit HRandom(HValue* global_object) {
    SetOperandAt(0, global_object);
    set_representation(Representation::Double());
  }

  HValue* global_object() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Random)

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HAdd V8_FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  // Add is only commutative if two integer values are added and not if two
  // tagged values are added (because it might be a String concatenation).
  virtual bool IsCommutative() const V8_OVERRIDE {
    return !representation().IsTagged();
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(
      BitVector* visited) V8_OVERRIDE;

  virtual HValue* Canonicalize() V8_OVERRIDE;

  virtual bool TryDecompose(DecompositionResult* decomposition) V8_OVERRIDE {
    if (left()->IsInteger32Constant()) {
      decomposition->Apply(right(), left()->GetInteger32Constant());
      return true;
    } else if (right()->IsInteger32Constant()) {
      decomposition->Apply(left(), right()->GetInteger32Constant());
      return true;
    } else {
      return false;
    }
  }

  virtual void RepresentationChanged(Representation to) V8_OVERRIDE {
    if (to.IsTagged()) ClearFlag(kAllowUndefinedAsNaN);
    HArithmeticBinaryOperation::RepresentationChanged(to);
  }

  DECLARE_CONCRETE_INSTRUCTION(Add)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

 private:
  HAdd(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HSub V8_FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual HValue* EnsureAndPropagateNotMinusZero(
      BitVector* visited) V8_OVERRIDE;

  virtual HValue* Canonicalize() V8_OVERRIDE;

  virtual bool TryDecompose(DecompositionResult* decomposition) V8_OVERRIDE {
    if (right()->IsInteger32Constant()) {
      decomposition->Apply(left(), -right()->GetInteger32Constant());
      return true;
    } else {
      return false;
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(Sub)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

 private:
  HSub(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HMul V8_FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  static HInstruction* NewImul(Zone* zone,
                               HValue* context,
                               HValue* left,
                               HValue* right) {
    HMul* mul = new(zone) HMul(context, left, right);
    // TODO(mstarzinger): Prevent bailout on minus zero for imul.
    mul->AssumeRepresentation(Representation::Integer32());
    mul->ClearFlag(HValue::kCanOverflow);
    return mul;
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(
      BitVector* visited) V8_OVERRIDE;

  virtual HValue* Canonicalize() V8_OVERRIDE;

  // Only commutative if it is certain that not two objects are multiplicated.
  virtual bool IsCommutative() const V8_OVERRIDE {
    return !representation().IsTagged();
  }

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) V8_OVERRIDE {
    HArithmeticBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Mul)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

 private:
  HMul(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HMod V8_FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right,
                           Maybe<int> fixed_right_arg);

  Maybe<int> fixed_right_arg() const { return fixed_right_arg_; }

  bool HasPowerOf2Divisor() {
    if (right()->IsConstant() &&
        HConstant::cast(right())->HasInteger32Value()) {
      int32_t value = HConstant::cast(right())->Integer32Value();
      return value != 0 && (IsPowerOf2(value) || IsPowerOf2(-value));
    }

    return false;
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(
      BitVector* visited) V8_OVERRIDE;

  virtual HValue* Canonicalize() V8_OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) V8_OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HArithmeticBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Mod)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

 private:
  HMod(HValue* context,
       HValue* left,
       HValue* right,
       Maybe<int> fixed_right_arg)
      : HArithmeticBinaryOperation(context, left, right),
        fixed_right_arg_(fixed_right_arg) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
  }

  const Maybe<int> fixed_right_arg_;
};


class HDiv V8_FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  bool HasPowerOf2Divisor() {
    if (right()->IsInteger32Constant()) {
      int32_t value = right()->GetInteger32Constant();
      return value != 0 && (IsPowerOf2(value) || IsPowerOf2(-value));
    }

    return false;
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(
      BitVector* visited) V8_OVERRIDE;

  virtual HValue* Canonicalize() V8_OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) V8_OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HArithmeticBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Div)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

 private:
  HDiv(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
  }
};


class HMathMinMax V8_FINAL : public HArithmeticBinaryOperation {
 public:
  enum Operation { kMathMin, kMathMax };

  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right,
                           Operation op);

  virtual Representation observed_input_representation(int index) V8_OVERRIDE {
    return RequiredInputRepresentation(index);
  }

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) V8_OVERRIDE;

  virtual Representation RepresentationFromInputs() V8_OVERRIDE {
    Representation left_rep = left()->representation();
    Representation right_rep = right()->representation();
    Representation result = Representation::Smi();
    result = result.generalize(left_rep);
    result = result.generalize(right_rep);
    if (result.IsTagged()) return Representation::Double();
    return result;
  }

  virtual bool IsCommutative() const V8_OVERRIDE { return true; }

  Operation operation() { return operation_; }

  DECLARE_CONCRETE_INSTRUCTION(MathMinMax)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    return other->IsMathMinMax() &&
        HMathMinMax::cast(other)->operation_ == operation_;
  }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

 private:
  HMathMinMax(HValue* context, HValue* left, HValue* right, Operation op)
      : HArithmeticBinaryOperation(context, left, right),
        operation_(op) { }

  Operation operation_;
};


class HBitwise V8_FINAL : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           Token::Value op,
                           HValue* left,
                           HValue* right);

  Token::Value op() const { return op_; }

  virtual bool IsCommutative() const V8_OVERRIDE { return true; }

  virtual HValue* Canonicalize() V8_OVERRIDE;

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(Bitwise)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    return op() == HBitwise::cast(other)->op();
  }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

 private:
  HBitwise(HValue* context,
           Token::Value op,
           HValue* left,
           HValue* right)
      : HBitwiseBinaryOperation(context, left, right, HType::TaggedNumber()),
        op_(op) {
    ASSERT(op == Token::BIT_AND || op == Token::BIT_OR || op == Token::BIT_XOR);
    // BIT_AND with a smi-range positive value will always unset the
    // entire sign-extension of the smi-sign.
    if (op == Token::BIT_AND &&
        ((left->IsConstant() &&
          left->representation().IsSmi() &&
          HConstant::cast(left)->Integer32Value() >= 0) ||
         (right->IsConstant() &&
          right->representation().IsSmi() &&
          HConstant::cast(right)->Integer32Value() >= 0))) {
      SetFlag(kTruncatingToSmi);
      SetFlag(kTruncatingToInt32);
    // BIT_OR with a smi-range negative value will always set the entire
    // sign-extension of the smi-sign.
    } else if (op == Token::BIT_OR &&
        ((left->IsConstant() &&
          left->representation().IsSmi() &&
          HConstant::cast(left)->Integer32Value() < 0) ||
         (right->IsConstant() &&
          right->representation().IsSmi() &&
          HConstant::cast(right)->Integer32Value() < 0))) {
      SetFlag(kTruncatingToSmi);
      SetFlag(kTruncatingToInt32);
    }
  }

  Token::Value op_;
};


class HShl V8_FINAL : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) V8_OVERRIDE {
    if (new_rep.IsSmi() &&
        !(right()->IsInteger32Constant() &&
          right()->GetInteger32Constant() >= 0)) {
      new_rep = Representation::Integer32();
    }
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Shl)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  HShl(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }
};


class HShr V8_FINAL : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual bool TryDecompose(DecompositionResult* decomposition) V8_OVERRIDE {
    if (right()->IsInteger32Constant()) {
      if (decomposition->Apply(left(), 0, right()->GetInteger32Constant())) {
        // This is intended to look for HAdd and HSub, to handle compounds
        // like ((base + offset) >> scale) with one single decomposition.
        left()->TryDecompose(decomposition);
        return true;
      }
    }
    return false;
  }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) V8_OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Shr)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  HShr(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }
};


class HSar V8_FINAL : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual bool TryDecompose(DecompositionResult* decomposition) V8_OVERRIDE {
    if (right()->IsInteger32Constant()) {
      if (decomposition->Apply(left(), 0, right()->GetInteger32Constant())) {
        // This is intended to look for HAdd and HSub, to handle compounds
        // like ((base + offset) >> scale) with one single decomposition.
        left()->TryDecompose(decomposition);
        return true;
      }
    }
    return false;
  }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) V8_OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Sar)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  HSar(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }
};


class HRor V8_FINAL : public HBitwiseBinaryOperation {
 public:
  HRor(HValue* context, HValue* left, HValue* right)
       : HBitwiseBinaryOperation(context, left, right) {
    ChangeRepresentation(Representation::Integer32());
  }

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) V8_OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Ror)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }
};


class HOsrEntry V8_FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HOsrEntry, BailoutId);

  BailoutId ast_id() const { return ast_id_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(OsrEntry)

 private:
  explicit HOsrEntry(BailoutId ast_id) : ast_id_(ast_id) {
    SetGVNFlag(kChangesOsrEntries);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  BailoutId ast_id_;
};


class HParameter V8_FINAL : public HTemplateInstruction<0> {
 public:
  enum ParameterKind {
    STACK_PARAMETER,
    REGISTER_PARAMETER
  };

  DECLARE_INSTRUCTION_FACTORY_P1(HParameter, unsigned);
  DECLARE_INSTRUCTION_FACTORY_P2(HParameter, unsigned, ParameterKind);
  DECLARE_INSTRUCTION_FACTORY_P3(HParameter, unsigned, ParameterKind,
                                 Representation);

  unsigned index() const { return index_; }
  ParameterKind kind() const { return kind_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Parameter)

 private:
  explicit HParameter(unsigned index,
                      ParameterKind kind = STACK_PARAMETER)
      : index_(index),
        kind_(kind) {
    set_representation(Representation::Tagged());
  }

  explicit HParameter(unsigned index,
                      ParameterKind kind,
                      Representation r)
      : index_(index),
        kind_(kind) {
    set_representation(r);
  }

  unsigned index_;
  ParameterKind kind_;
};


class HCallStub V8_FINAL : public HUnaryCall {
 public:
  HCallStub(HValue* context, CodeStub::Major major_key, int argument_count)
      : HUnaryCall(context, argument_count),
        major_key_(major_key),
        transcendental_type_(TranscendentalCache::kNumberOfCaches) {
  }

  CodeStub::Major major_key() { return major_key_; }

  HValue* context() { return value(); }

  void set_transcendental_type(TranscendentalCache::Type transcendental_type) {
    transcendental_type_ = transcendental_type;
  }
  TranscendentalCache::Type transcendental_type() {
    return transcendental_type_;
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CallStub)

 private:
  CodeStub::Major major_key_;
  TranscendentalCache::Type transcendental_type_;
};


class HUnknownOSRValue V8_FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HUnknownOSRValue, HEnvironment*, int);

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  void set_incoming_value(HPhi* value) { incoming_value_ = value; }
  HPhi* incoming_value() { return incoming_value_; }
  HEnvironment *environment() { return environment_; }
  int index() { return index_; }

  virtual Representation KnownOptimalRepresentation() V8_OVERRIDE {
    if (incoming_value_ == NULL) return Representation::None();
    return incoming_value_->KnownOptimalRepresentation();
  }

  DECLARE_CONCRETE_INSTRUCTION(UnknownOSRValue)

 private:
  HUnknownOSRValue(HEnvironment* environment, int index)
      : environment_(environment),
        index_(index),
        incoming_value_(NULL) {
    set_representation(Representation::Tagged());
  }

  HEnvironment* environment_;
  int index_;
  HPhi* incoming_value_;
};


class HLoadGlobalCell V8_FINAL : public HTemplateInstruction<0> {
 public:
  HLoadGlobalCell(Handle<Cell> cell, PropertyDetails details)
      : cell_(cell), details_(details), unique_id_() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnGlobalVars);
  }

  Handle<Cell> cell() const { return cell_; }
  bool RequiresHoleCheck() const;

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual intptr_t Hashcode() V8_OVERRIDE {
    return unique_id_.Hashcode();
  }

  virtual void FinalizeUniqueValueId() V8_OVERRIDE {
    unique_id_ = UniqueValueId(cell_);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalCell)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HLoadGlobalCell* b = HLoadGlobalCell::cast(other);
    return unique_id_ == b->unique_id_;
  }

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return !RequiresHoleCheck(); }

  Handle<Cell> cell_;
  PropertyDetails details_;
  UniqueValueId unique_id_;
};


class HLoadGlobalGeneric V8_FINAL : public HTemplateInstruction<2> {
 public:
  HLoadGlobalGeneric(HValue* context,
                     HValue* global_object,
                     Handle<Object> name,
                     bool for_typeof)
      : name_(name),
        for_typeof_(for_typeof) {
    SetOperandAt(0, context);
    SetOperandAt(1, global_object);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  HValue* context() { return OperandAt(0); }
  HValue* global_object() { return OperandAt(1); }
  Handle<Object> name() const { return name_; }
  bool for_typeof() const { return for_typeof_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalGeneric)

 private:
  Handle<Object> name_;
  bool for_typeof_;
};


class HAllocate V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HAllocate* New(Zone* zone,
                        HValue* context,
                        HValue* size,
                        HType type,
                        PretenureFlag pretenure_flag,
                        InstanceType instance_type) {
    return new(zone) HAllocate(context, size, type, pretenure_flag,
        instance_type);
  }

  // Maximum instance size for which allocations will be inlined.
  static const int kMaxInlineSize = 64 * kPointerSize;

  HValue* context() { return OperandAt(0); }
  HValue* size() { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      return Representation::Integer32();
    }
  }

  virtual Handle<Map> GetMonomorphicJSObjectMap() {
    return known_initial_map_;
  }

  void set_known_initial_map(Handle<Map> known_initial_map) {
    known_initial_map_ = known_initial_map;
  }

  bool IsNewSpaceAllocation() const {
    return (flags_ & ALLOCATE_IN_NEW_SPACE) != 0;
  }

  bool IsOldDataSpaceAllocation() const {
    return (flags_ & ALLOCATE_IN_OLD_DATA_SPACE) != 0;
  }

  bool IsOldPointerSpaceAllocation() const {
    return (flags_ & ALLOCATE_IN_OLD_POINTER_SPACE) != 0;
  }

  bool MustAllocateDoubleAligned() const {
    return (flags_ & ALLOCATE_DOUBLE_ALIGNED) != 0;
  }

  bool MustPrefillWithFiller() const {
    return (flags_ & PREFILL_WITH_FILLER) != 0;
  }

  void MakePrefillWithFiller() {
    flags_ = static_cast<HAllocate::Flags>(flags_ | PREFILL_WITH_FILLER);
  }

  void MakeDoubleAligned() {
    flags_ = static_cast<HAllocate::Flags>(flags_ | ALLOCATE_DOUBLE_ALIGNED);
  }

  virtual void HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) V8_OVERRIDE;

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(Allocate)

 private:
  enum Flags {
    ALLOCATE_IN_NEW_SPACE = 1 << 0,
    ALLOCATE_IN_OLD_DATA_SPACE = 1 << 1,
    ALLOCATE_IN_OLD_POINTER_SPACE = 1 << 2,
    ALLOCATE_DOUBLE_ALIGNED = 1 << 3,
    PREFILL_WITH_FILLER = 1 << 4
  };

  HAllocate(HValue* context,
            HValue* size,
            HType type,
            PretenureFlag pretenure_flag,
            InstanceType instance_type)
      : HTemplateInstruction<2>(type),
        dominating_allocate_(NULL),
        filler_free_space_size_(NULL),
        clear_next_map_word_(false) {
    SetOperandAt(0, context);
    SetOperandAt(1, size);
    set_representation(Representation::Tagged());
    SetFlag(kTrackSideEffectDominators);
    SetGVNFlag(kChangesNewSpacePromotion);
    SetGVNFlag(kDependsOnNewSpacePromotion);
    flags_ = pretenure_flag == TENURED
        ? (Heap::TargetSpaceId(instance_type) == OLD_POINTER_SPACE
            ? ALLOCATE_IN_OLD_POINTER_SPACE : ALLOCATE_IN_OLD_DATA_SPACE)
        : ALLOCATE_IN_NEW_SPACE;
    if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) {
      flags_ = static_cast<HAllocate::Flags>(flags_ | ALLOCATE_DOUBLE_ALIGNED);
    }
    // We have to fill the allocated object with one word fillers if we do
    // not use allocation folding since some allocations may depend on each
    // other, i.e., have a pointer to each other. A GC in between these
    // allocations may leave such objects behind in a not completely initialized
    // state.
    if (!FLAG_use_gvn || !FLAG_use_allocation_folding) {
      flags_ = static_cast<HAllocate::Flags>(flags_ | PREFILL_WITH_FILLER);
    }
    clear_next_map_word_ = pretenure_flag == NOT_TENURED &&
        AllocationSite::CanTrack(instance_type);
  }

  void UpdateSize(HValue* size) {
    SetOperandAt(1, size);
  }

  HAllocate* GetFoldableDominator(HAllocate* dominator);

  void UpdateFreeSpaceFiller(int32_t filler_size);

  void CreateFreeSpaceFiller(int32_t filler_size);

  bool IsFoldable(HAllocate* allocate) {
    return (IsNewSpaceAllocation() && allocate->IsNewSpaceAllocation()) ||
        (IsOldDataSpaceAllocation() && allocate->IsOldDataSpaceAllocation()) ||
        (IsOldPointerSpaceAllocation() &&
            allocate->IsOldPointerSpaceAllocation());
  }

  void ClearNextMapWord(int offset);

  Flags flags_;
  Handle<Map> known_initial_map_;
  HAllocate* dominating_allocate_;
  HStoreNamedField* filler_free_space_size_;
  bool clear_next_map_word_;
};


class HStoreCodeEntry V8_FINAL: public HTemplateInstruction<2> {
 public:
  static HStoreCodeEntry* New(Zone* zone,
                              HValue* context,
                              HValue* function,
                              HValue* code) {
    return new(zone) HStoreCodeEntry(function, code);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* function() { return OperandAt(0); }
  HValue* code_object() { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(StoreCodeEntry)

 private:
  HStoreCodeEntry(HValue* function, HValue* code) {
    SetOperandAt(0, function);
    SetOperandAt(1, code);
  }
};


class HInnerAllocatedObject V8_FINAL: public HTemplateInstruction<1> {
 public:
  static HInnerAllocatedObject* New(Zone* zone,
                                    HValue* context,
                                    HValue* value,
                                    int offset,
                                    HType type = HType::Tagged()) {
    return new(zone) HInnerAllocatedObject(value, offset, type);
  }

  HValue* base_object() { return OperandAt(0); }
  int offset() { return offset_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(InnerAllocatedObject)

 private:
  HInnerAllocatedObject(HValue* value, int offset, HType type = HType::Tagged())
      : HTemplateInstruction<1>(type), offset_(offset) {
    ASSERT(value->IsAllocate());
    SetOperandAt(0, value);
    set_type(type);
    set_representation(Representation::Tagged());
  }

  int offset_;
};


inline bool StoringValueNeedsWriteBarrier(HValue* value) {
  return !value->type().IsBoolean()
      && !value->type().IsSmi()
      && !(value->IsConstant() && HConstant::cast(value)->ImmortalImmovable());
}


inline bool ReceiverObjectNeedsWriteBarrier(HValue* object,
                                            HValue* new_space_dominator) {
  if (object->IsInnerAllocatedObject()) {
    return ReceiverObjectNeedsWriteBarrier(
        HInnerAllocatedObject::cast(object)->base_object(),
        new_space_dominator);
  }
  if (object->IsConstant() && HConstant::cast(object)->IsCell()) {
    return false;
  }
  if (object->IsConstant() &&
      HConstant::cast(object)->HasExternalReferenceValue()) {
    // Stores to external references require no write barriers
    return false;
  }
  if (object != new_space_dominator) return true;
  if (object->IsAllocate()) {
    return !HAllocate::cast(object)->IsNewSpaceAllocation();
  }
  return true;
}


class HStoreGlobalCell V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HStoreGlobalCell, HValue*,
                                 Handle<PropertyCell>, PropertyDetails);

  Handle<PropertyCell> cell() const { return cell_; }
  bool RequiresHoleCheck() {
    return !details_.IsDontDelete() || details_.IsReadOnly();
  }
  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobalCell)

 private:
  HStoreGlobalCell(HValue* value,
                   Handle<PropertyCell> cell,
                   PropertyDetails details)
      : HUnaryOperation(value),
        cell_(cell),
        details_(details) {
    SetGVNFlag(kChangesGlobalVars);
  }

  Handle<PropertyCell> cell_;
  PropertyDetails details_;
};


class HStoreGlobalGeneric : public HTemplateInstruction<3> {
 public:
  inline static HStoreGlobalGeneric* New(Zone* zone,
                                         HValue* context,
                                         HValue* global_object,
                                         Handle<Object> name,
                                         HValue* value,
                                         StrictModeFlag strict_mode_flag) {
    return new(zone) HStoreGlobalGeneric(context, global_object,
                                         name, value, strict_mode_flag);
  }

  HValue* context() { return OperandAt(0); }
  HValue* global_object() { return OperandAt(1); }
  Handle<Object> name() const { return name_; }
  HValue* value() { return OperandAt(2); }
  StrictModeFlag strict_mode_flag() { return strict_mode_flag_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobalGeneric)

 private:
  HStoreGlobalGeneric(HValue* context,
                      HValue* global_object,
                      Handle<Object> name,
                      HValue* value,
                      StrictModeFlag strict_mode_flag)
      : name_(name),
        strict_mode_flag_(strict_mode_flag) {
    SetOperandAt(0, context);
    SetOperandAt(1, global_object);
    SetOperandAt(2, value);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  Handle<Object> name_;
  StrictModeFlag strict_mode_flag_;
};


class HLoadContextSlot V8_FINAL : public HUnaryOperation {
 public:
  enum Mode {
    // Perform a normal load of the context slot without checking its value.
    kNoCheck,
    // Load and check the value of the context slot. Deoptimize if it's the
    // hole value. This is used for checking for loading of uninitialized
    // harmony bindings where we deoptimize into full-codegen generated code
    // which will subsequently throw a reference error.
    kCheckDeoptimize,
    // Load and check the value of the context slot. Return undefined if it's
    // the hole value. This is used for non-harmony const assignments
    kCheckReturnUndefined
  };

  HLoadContextSlot(HValue* context, Variable* var)
      : HUnaryOperation(context), slot_index_(var->index()) {
    ASSERT(var->IsContextSlot());
    switch (var->mode()) {
      case LET:
      case CONST_HARMONY:
        mode_ = kCheckDeoptimize;
        break;
      case CONST:
        mode_ = kCheckReturnUndefined;
        break;
      default:
        mode_ = kNoCheck;
    }
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnContextSlots);
  }

  int slot_index() const { return slot_index_; }
  Mode mode() const { return mode_; }

  bool DeoptimizesOnHole() {
    return mode_ == kCheckDeoptimize;
  }

  bool RequiresHoleCheck() const {
    return mode_ != kNoCheck;
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(LoadContextSlot)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HLoadContextSlot* b = HLoadContextSlot::cast(other);
    return (slot_index() == b->slot_index());
  }

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return !RequiresHoleCheck(); }

  int slot_index_;
  Mode mode_;
};


class HStoreContextSlot V8_FINAL : public HTemplateInstruction<2> {
 public:
  enum Mode {
    // Perform a normal store to the context slot without checking its previous
    // value.
    kNoCheck,
    // Check the previous value of the context slot and deoptimize if it's the
    // hole value. This is used for checking for assignments to uninitialized
    // harmony bindings where we deoptimize into full-codegen generated code
    // which will subsequently throw a reference error.
    kCheckDeoptimize,
    // Check the previous value and ignore assignment if it isn't a hole value
    kCheckIgnoreAssignment
  };

  DECLARE_INSTRUCTION_FACTORY_P4(HStoreContextSlot, HValue*, int,
                                 Mode, HValue*);

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }
  int slot_index() const { return slot_index_; }
  Mode mode() const { return mode_; }

  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

  bool DeoptimizesOnHole() {
    return mode_ == kCheckDeoptimize;
  }

  bool RequiresHoleCheck() {
    return mode_ != kNoCheck;
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(StoreContextSlot)

 private:
  HStoreContextSlot(HValue* context, int slot_index, Mode mode, HValue* value)
      : slot_index_(slot_index), mode_(mode) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    SetGVNFlag(kChangesContextSlots);
  }

  int slot_index_;
  Mode mode_;
};


// Represents an access to a portion of an object, such as the map pointer,
// array elements pointer, etc, but not accesses to array elements themselves.
class HObjectAccess V8_FINAL {
 public:
  inline bool IsInobject() const {
    return portion() != kBackingStore && portion() != kExternalMemory;
  }

  inline bool IsExternalMemory() const {
    return portion() == kExternalMemory;
  }

  inline bool IsStringLength() const {
    return portion() == kStringLengths;
  }

  inline int offset() const {
    return OffsetField::decode(value_);
  }

  inline Representation representation() const {
    return Representation::FromKind(RepresentationField::decode(value_));
  }

  inline Handle<String> name() const {
    return name_;
  }

  inline HObjectAccess WithRepresentation(Representation representation) {
    return HObjectAccess(portion(), offset(), representation, name());
  }

  static HObjectAccess ForHeapNumberValue() {
    return HObjectAccess(
        kDouble, HeapNumber::kValueOffset, Representation::Double());
  }

  static HObjectAccess ForElementsPointer() {
    return HObjectAccess(kElementsPointer, JSObject::kElementsOffset);
  }

  static HObjectAccess ForLiteralsPointer() {
    return HObjectAccess(kInobject, JSFunction::kLiteralsOffset);
  }

  static HObjectAccess ForNextFunctionLinkPointer() {
    return HObjectAccess(kInobject, JSFunction::kNextFunctionLinkOffset);
  }

  static HObjectAccess ForArrayLength(ElementsKind elements_kind) {
    return HObjectAccess(
        kArrayLengths,
        JSArray::kLengthOffset,
        IsFastElementsKind(elements_kind) &&
            FLAG_track_fields
                ? Representation::Smi() : Representation::Tagged());
  }

  static HObjectAccess ForAllocationSiteTransitionInfo() {
    return HObjectAccess(kInobject, AllocationSite::kTransitionInfoOffset);
  }

  static HObjectAccess ForAllocationSiteWeakNext() {
    return HObjectAccess(kInobject, AllocationSite::kWeakNextOffset);
  }

  static HObjectAccess ForAllocationSiteList() {
    return HObjectAccess(kExternalMemory, 0, Representation::Tagged());
  }

  static HObjectAccess ForFixedArrayLength() {
    return HObjectAccess(
        kArrayLengths,
        FixedArray::kLengthOffset,
        FLAG_track_fields ? Representation::Smi() : Representation::Tagged());
  }

  static HObjectAccess ForStringLength() {
    STATIC_ASSERT(String::kMaxLength <= Smi::kMaxValue);
    return HObjectAccess(
        kStringLengths,
        String::kLengthOffset,
        FLAG_track_fields ? Representation::Smi() : Representation::Tagged());
  }

  static HObjectAccess ForPropertiesPointer() {
    return HObjectAccess(kInobject, JSObject::kPropertiesOffset);
  }

  static HObjectAccess ForPrototypeOrInitialMap() {
    return HObjectAccess(kInobject, JSFunction::kPrototypeOrInitialMapOffset);
  }

  static HObjectAccess ForSharedFunctionInfoPointer() {
    return HObjectAccess(kInobject, JSFunction::kSharedFunctionInfoOffset);
  }

  static HObjectAccess ForCodeEntryPointer() {
    return HObjectAccess(kInobject, JSFunction::kCodeEntryOffset);
  }

  static HObjectAccess ForCodeOffset() {
    return HObjectAccess(kInobject, SharedFunctionInfo::kCodeOffset);
  }

  static HObjectAccess ForFirstCodeSlot() {
    return HObjectAccess(kInobject, SharedFunctionInfo::kFirstCodeSlot);
  }

  static HObjectAccess ForFirstContextSlot() {
    return HObjectAccess(kInobject, SharedFunctionInfo::kFirstContextSlot);
  }

  static HObjectAccess ForOptimizedCodeMap() {
    return HObjectAccess(kInobject,
                         SharedFunctionInfo::kOptimizedCodeMapOffset);
  }

  static HObjectAccess ForFunctionContextPointer() {
    return HObjectAccess(kInobject, JSFunction::kContextOffset);
  }

  static HObjectAccess ForMap() {
    return HObjectAccess(kMaps, JSObject::kMapOffset);
  }

  static HObjectAccess ForPropertyCellValue() {
    return HObjectAccess(kInobject, PropertyCell::kValueOffset);
  }

  static HObjectAccess ForCellValue() {
    return HObjectAccess(kInobject, Cell::kValueOffset);
  }

  static HObjectAccess ForAllocationMementoSite() {
    return HObjectAccess(kInobject, AllocationMemento::kAllocationSiteOffset);
  }

  static HObjectAccess ForCounter() {
    return HObjectAccess(kExternalMemory, 0, Representation::Integer32());
  }

  // Create an access to an offset in a fixed array header.
  static HObjectAccess ForFixedArrayHeader(int offset);

  // Create an access to an in-object property in a JSObject.
  static HObjectAccess ForJSObjectOffset(int offset,
      Representation representation = Representation::Tagged());

  // Create an access to an in-object property in a JSArray.
  static HObjectAccess ForJSArrayOffset(int offset);

  static HObjectAccess ForContextSlot(int index);

  // Create an access to the backing store of an object.
  static HObjectAccess ForBackingStoreOffset(int offset,
      Representation representation = Representation::Tagged());

  // Create an access to a resolved field (in-object or backing store).
  static HObjectAccess ForField(Handle<Map> map,
      LookupResult *lookup, Handle<String> name = Handle<String>::null());

  // Create an access for the payload of a Cell or JSGlobalPropertyCell.
  static HObjectAccess ForCellPayload(Isolate* isolate);

  void PrintTo(StringStream* stream);

  inline bool Equals(HObjectAccess that) const {
    return value_ == that.value_;  // portion and offset must match
  }

 protected:
  void SetGVNFlags(HValue *instr, bool is_store);

 private:
  // internal use only; different parts of an object or array
  enum Portion {
    kMaps,             // map of an object
    kArrayLengths,     // the length of an array
    kStringLengths,    // the length of a string
    kElementsPointer,  // elements pointer
    kBackingStore,     // some field in the backing store
    kDouble,           // some double field
    kInobject,         // some other in-object field
    kExternalMemory    // some field in external memory
  };

  HObjectAccess(Portion portion, int offset,
                Representation representation = Representation::Tagged(),
                Handle<String> name = Handle<String>::null())
    : value_(PortionField::encode(portion) |
             RepresentationField::encode(representation.kind()) |
             OffsetField::encode(offset)),
      name_(name) {
    // assert that the fields decode correctly
    ASSERT(this->offset() == offset);
    ASSERT(this->portion() == portion);
    ASSERT(RepresentationField::decode(value_) == representation.kind());
  }

  class PortionField : public BitField<Portion, 0, 3> {};
  class RepresentationField : public BitField<Representation::Kind, 3, 3> {};
  class OffsetField : public BitField<int, 6, 26> {};

  uint32_t value_;  // encodes portion, representation, and offset
  Handle<String> name_;

  friend class HLoadNamedField;
  friend class HStoreNamedField;

  inline Portion portion() const {
    return PortionField::decode(value_);
  }
};


class HLoadNamedField V8_FINAL : public HTemplateInstruction<1> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HLoadNamedField, HValue*, HObjectAccess);

  HValue* object() { return OperandAt(0); }
  bool HasTypeCheck() { return object()->IsCheckMaps(); }
  HObjectAccess access() const { return access_; }
  Representation field_representation() const {
      return access_.representation();
  }

  virtual bool HasEscapingOperandAt(int index) V8_OVERRIDE { return false; }
  virtual bool HasOutOfBoundsAccess(int size) V8_OVERRIDE {
    return !access().IsInobject() || access().offset() >= size;
  }
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    if (index == 0 && access().IsExternalMemory()) {
      // object must be external in case of external memory access
      return Representation::External();
    }
    return Representation::Tagged();
  }
  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HLoadNamedField* b = HLoadNamedField::cast(other);
    return access_.Equals(b->access_);
  }

 private:
  HLoadNamedField(HValue* object, HObjectAccess access) : access_(access) {
    ASSERT(object != NULL);
    SetOperandAt(0, object);

    Representation representation = access.representation();
    if (representation.IsSmi()) {
      set_type(HType::Smi());
      set_representation(representation);
    } else if (representation.IsDouble() ||
               representation.IsExternal() ||
               representation.IsInteger32()) {
      set_representation(representation);
    } else if (FLAG_track_heap_object_fields &&
               representation.IsHeapObject()) {
      set_type(HType::NonPrimitive());
      set_representation(Representation::Tagged());
    } else {
      set_representation(Representation::Tagged());
    }
    access.SetGVNFlags(this, false);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  HObjectAccess access_;
};


class HLoadNamedGeneric V8_FINAL : public HTemplateInstruction<2> {
 public:
  HLoadNamedGeneric(HValue* context, HValue* object, Handle<Object> name)
      : name_(name) {
    SetOperandAt(0, context);
    SetOperandAt(1, object);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  HValue* context() { return OperandAt(0); }
  HValue* object() { return OperandAt(1); }
  Handle<Object> name() const { return name_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedGeneric)

 private:
  Handle<Object> name_;
};


class HLoadFunctionPrototype V8_FINAL : public HUnaryOperation {
 public:
  explicit HLoadFunctionPrototype(HValue* function)
      : HUnaryOperation(function) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnCalls);
  }

  HValue* function() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }
};

class ArrayInstructionInterface {
 public:
  virtual HValue* GetKey() = 0;
  virtual void SetKey(HValue* key) = 0;
  virtual void SetIndexOffset(uint32_t index_offset) = 0;
  virtual bool IsDehoisted() = 0;
  virtual void SetDehoisted(bool is_dehoisted) = 0;
  virtual ~ArrayInstructionInterface() { };

  static Representation KeyedAccessIndexRequirement(Representation r) {
    return r.IsInteger32() || SmiValuesAre32Bits()
        ? Representation::Integer32() : Representation::Smi();
  }
};


enum LoadKeyedHoleMode {
  NEVER_RETURN_HOLE,
  ALLOW_RETURN_HOLE
};


class HLoadKeyed V8_FINAL
    : public HTemplateInstruction<3>, public ArrayInstructionInterface {
 public:
  DECLARE_INSTRUCTION_FACTORY_P4(HLoadKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind);
  DECLARE_INSTRUCTION_FACTORY_P5(HLoadKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind, LoadKeyedHoleMode);

  bool is_external() const {
    return IsExternalArrayElementsKind(elements_kind());
  }
  HValue* elements() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }
  HValue* dependency() {
    ASSERT(HasDependency());
    return OperandAt(2);
  }
  bool HasDependency() const { return OperandAt(0) != OperandAt(2); }
  uint32_t index_offset() { return IndexOffsetField::decode(bit_field_); }
  void SetIndexOffset(uint32_t index_offset) {
    bit_field_ = IndexOffsetField::update(bit_field_, index_offset);
  }
  HValue* GetKey() { return key(); }
  void SetKey(HValue* key) { SetOperandAt(1, key); }
  bool IsDehoisted() { return IsDehoistedField::decode(bit_field_); }
  void SetDehoisted(bool is_dehoisted) {
    bit_field_ = IsDehoistedField::update(bit_field_, is_dehoisted);
  }
  ElementsKind elements_kind() const {
    return ElementsKindField::decode(bit_field_);
  }
  LoadKeyedHoleMode hole_mode() const {
    return HoleModeField::decode(bit_field_);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    // kind_fast:       tagged[int32] (none)
    // kind_double:     tagged[int32] (none)
    // kind_external: external[int32] (none)
    if (index == 0) {
      return is_external() ? Representation::External()
          : Representation::Tagged();
    }
    if (index == 1) {
      return ArrayInstructionInterface::KeyedAccessIndexRequirement(
          OperandAt(1)->representation());
    }
    return Representation::None();
  }

  virtual Representation observed_input_representation(int index) V8_OVERRIDE {
    return RequiredInputRepresentation(index);
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  bool UsesMustHandleHole() const;
  bool AllUsesCanTreatHoleAsNaN() const;
  bool RequiresHoleCheck() const;

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyed)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    if (!other->IsLoadKeyed()) return false;
    HLoadKeyed* other_load = HLoadKeyed::cast(other);

    if (IsDehoisted() && index_offset() != other_load->index_offset())
      return false;
    return elements_kind() == other_load->elements_kind();
  }

 private:
  HLoadKeyed(HValue* obj,
             HValue* key,
             HValue* dependency,
             ElementsKind elements_kind,
             LoadKeyedHoleMode mode = NEVER_RETURN_HOLE)
      : bit_field_(0) {
    bit_field_ = ElementsKindField::encode(elements_kind) |
        HoleModeField::encode(mode);

    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, dependency != NULL ? dependency : obj);

    if (!is_external()) {
      // I can detect the case between storing double (holey and fast) and
      // smi/object by looking at elements_kind_.
      ASSERT(IsFastSmiOrObjectElementsKind(elements_kind) ||
             IsFastDoubleElementsKind(elements_kind));

      if (IsFastSmiOrObjectElementsKind(elements_kind)) {
        if (IsFastSmiElementsKind(elements_kind) &&
            (!IsHoleyElementsKind(elements_kind) ||
             mode == NEVER_RETURN_HOLE)) {
          set_type(HType::Smi());
          set_representation(Representation::Smi());
        } else {
          set_representation(Representation::Tagged());
        }

        SetGVNFlag(kDependsOnArrayElements);
      } else {
        set_representation(Representation::Double());
        SetGVNFlag(kDependsOnDoubleArrayElements);
      }
    } else {
      if (elements_kind == EXTERNAL_FLOAT_ELEMENTS ||
          elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
        set_representation(Representation::Double());
      } else {
        set_representation(Representation::Integer32());
      }

      SetGVNFlag(kDependsOnExternalMemory);
      // Native code could change the specialized array.
      SetGVNFlag(kDependsOnCalls);
    }

    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE {
    return !RequiresHoleCheck();
  }

  // Establish some checks around our packed fields
  enum LoadKeyedBits {
    kBitsForElementsKind = 5,
    kBitsForHoleMode = 1,
    kBitsForIndexOffset = 25,
    kBitsForIsDehoisted = 1,

    kStartElementsKind = 0,
    kStartHoleMode = kStartElementsKind + kBitsForElementsKind,
    kStartIndexOffset = kStartHoleMode + kBitsForHoleMode,
    kStartIsDehoisted = kStartIndexOffset + kBitsForIndexOffset
  };

  STATIC_ASSERT((kBitsForElementsKind + kBitsForIndexOffset +
                 kBitsForIsDehoisted) <= sizeof(uint32_t)*8);
  STATIC_ASSERT(kElementsKindCount <= (1 << kBitsForElementsKind));
  class ElementsKindField:
    public BitField<ElementsKind, kStartElementsKind, kBitsForElementsKind>
    {};  // NOLINT
  class HoleModeField:
    public BitField<LoadKeyedHoleMode, kStartHoleMode, kBitsForHoleMode>
    {};  // NOLINT
  class IndexOffsetField:
    public BitField<uint32_t, kStartIndexOffset, kBitsForIndexOffset>
    {};  // NOLINT
  class IsDehoistedField:
    public BitField<bool, kStartIsDehoisted, kBitsForIsDehoisted>
    {};  // NOLINT
  uint32_t bit_field_;
};


class HLoadKeyedGeneric V8_FINAL : public HTemplateInstruction<3> {
 public:
  HLoadKeyedGeneric(HValue* context, HValue* obj, HValue* key) {
    set_representation(Representation::Tagged());
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, context);
    SetAllSideEffects();
  }

  HValue* object() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }
  HValue* context() { return OperandAt(2); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    // tagged[tagged]
    return Representation::Tagged();
  }

  virtual HValue* Canonicalize() V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedGeneric)
};


class HStoreNamedField V8_FINAL : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HStoreNamedField, HValue*,
                                 HObjectAccess, HValue*);

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField)

  virtual bool HasEscapingOperandAt(int index) V8_OVERRIDE {
    return index == 1;
  }
  virtual bool HasOutOfBoundsAccess(int size) V8_OVERRIDE {
    return !access().IsInobject() || access().offset() >= size;
  }
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    if (index == 0 && access().IsExternalMemory()) {
      // object must be external in case of external memory access
      return Representation::External();
    } else if (index == 1 &&
        (field_representation().IsDouble() ||
         field_representation().IsSmi() ||
         field_representation().IsInteger32())) {
      return field_representation();
    }
    return Representation::Tagged();
  }
  virtual void HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) V8_OVERRIDE {
    ASSERT(side_effect == kChangesNewSpacePromotion);
    new_space_dominator_ = dominator;
  }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  void SkipWriteBarrier() { write_barrier_mode_ = SKIP_WRITE_BARRIER; }
  bool IsSkipWriteBarrier() const {
    return write_barrier_mode_ == SKIP_WRITE_BARRIER;
  }

  HValue* object() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }
  HValue* transition() const { return OperandAt(2); }

  HObjectAccess access() const { return access_; }
  HValue* new_space_dominator() const { return new_space_dominator_; }
  bool has_transition() const { return has_transition_; }

  Handle<Map> transition_map() const {
    if (has_transition()) {
      return Handle<Map>::cast(
          HConstant::cast(transition())->handle(Isolate::Current()));
    } else {
      return Handle<Map>();
    }
  }

  void SetTransition(HConstant* map_constant, CompilationInfo* info) {
    ASSERT(!has_transition());  // Only set once.
    Handle<Map> map = Handle<Map>::cast(map_constant->handle(info->isolate()));
    if (map->CanBeDeprecated()) {
      map->AddDependentCompilationInfo(DependentCode::kTransitionGroup, info);
    }
    SetOperandAt(2, map_constant);
    has_transition_ = true;
  }

  bool NeedsWriteBarrier() {
    ASSERT(!(FLAG_track_double_fields && field_representation().IsDouble()) ||
           !has_transition());
    if (IsSkipWriteBarrier()) return false;
    if (field_representation().IsDouble()) return false;
    if (field_representation().IsSmi()) return false;
    if (field_representation().IsInteger32()) return false;
    if (field_representation().IsExternal()) return false;
    return StoringValueNeedsWriteBarrier(value()) &&
        ReceiverObjectNeedsWriteBarrier(object(), new_space_dominator());
  }

  bool NeedsWriteBarrierForMap() {
    if (IsSkipWriteBarrier()) return false;
    return ReceiverObjectNeedsWriteBarrier(object(), new_space_dominator());
  }

  Representation field_representation() const {
    return access_.representation();
  }

  void UpdateValue(HValue* value) {
    SetOperandAt(1, value);
  }

 private:
  HStoreNamedField(HValue* obj,
                   HObjectAccess access,
                   HValue* val)
      : access_(access),
        new_space_dominator_(NULL),
        write_barrier_mode_(UPDATE_WRITE_BARRIER),
        has_transition_(false) {
    SetOperandAt(0, obj);
    SetOperandAt(1, val);
    SetOperandAt(2, obj);
    access.SetGVNFlags(this, true);
  }

  HObjectAccess access_;
  HValue* new_space_dominator_;
  WriteBarrierMode write_barrier_mode_ : 1;
  bool has_transition_ : 1;
};


class HStoreNamedGeneric V8_FINAL : public HTemplateInstruction<3> {
 public:
  HStoreNamedGeneric(HValue* context,
                     HValue* object,
                     Handle<String> name,
                     HValue* value,
                     StrictModeFlag strict_mode_flag)
      : name_(name),
        strict_mode_flag_(strict_mode_flag) {
    SetOperandAt(0, object);
    SetOperandAt(1, value);
    SetOperandAt(2, context);
    SetAllSideEffects();
  }

  HValue* object() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }
  HValue* context() { return OperandAt(2); }
  Handle<String> name() { return name_; }
  StrictModeFlag strict_mode_flag() { return strict_mode_flag_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedGeneric)

 private:
  Handle<String> name_;
  StrictModeFlag strict_mode_flag_;
};


class HStoreKeyed V8_FINAL
    : public HTemplateInstruction<3>, public ArrayInstructionInterface {
 public:
  DECLARE_INSTRUCTION_FACTORY_P4(HStoreKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    // kind_fast:       tagged[int32] = tagged
    // kind_double:     tagged[int32] = double
    // kind_smi   :     tagged[int32] = smi
    // kind_external: external[int32] = (double | int32)
    if (index == 0) {
      return is_external() ? Representation::External()
                           : Representation::Tagged();
    } else if (index == 1) {
      return ArrayInstructionInterface::KeyedAccessIndexRequirement(
          OperandAt(1)->representation());
    }

    ASSERT_EQ(index, 2);
    if (IsDoubleOrFloatElementsKind(elements_kind())) {
      return Representation::Double();
    }

    if (IsFastSmiElementsKind(elements_kind())) {
      return Representation::Smi();
    }

    return is_external() ? Representation::Integer32()
                         : Representation::Tagged();
  }

  bool is_external() const {
    return IsExternalArrayElementsKind(elements_kind());
  }

  virtual Representation observed_input_representation(int index) V8_OVERRIDE {
    if (index < 2) return RequiredInputRepresentation(index);
    if (IsUninitialized()) {
      return Representation::None();
    }
    if (IsFastSmiElementsKind(elements_kind())) {
      return Representation::Smi();
    }
    if (IsDoubleOrFloatElementsKind(elements_kind())) {
      return Representation::Double();
    }
    if (is_external()) {
      return Representation::Integer32();
    }
    // For fast object elements kinds, don't assume anything.
    return Representation::None();
  }

  HValue* elements() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }
  HValue* value() { return OperandAt(2); }
  bool value_is_smi() const {
    return IsFastSmiElementsKind(elements_kind_);
  }
  ElementsKind elements_kind() const { return elements_kind_; }
  uint32_t index_offset() { return index_offset_; }
  void SetIndexOffset(uint32_t index_offset) { index_offset_ = index_offset; }
  HValue* GetKey() { return key(); }
  void SetKey(HValue* key) { SetOperandAt(1, key); }
  bool IsDehoisted() { return is_dehoisted_; }
  void SetDehoisted(bool is_dehoisted) { is_dehoisted_ = is_dehoisted; }
  bool IsUninitialized() { return is_uninitialized_; }
  void SetUninitialized(bool is_uninitialized) {
    is_uninitialized_ = is_uninitialized;
  }

  bool IsConstantHoleStore() {
    return value()->IsConstant() && HConstant::cast(value())->IsTheHole();
  }

  virtual void HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) V8_OVERRIDE {
    ASSERT(side_effect == kChangesNewSpacePromotion);
    new_space_dominator_ = dominator;
  }

  HValue* new_space_dominator() const { return new_space_dominator_; }

  bool NeedsWriteBarrier() {
    if (value_is_smi()) {
      return false;
    } else {
      return StoringValueNeedsWriteBarrier(value()) &&
          ReceiverObjectNeedsWriteBarrier(elements(), new_space_dominator());
    }
  }

  bool NeedsCanonicalization();

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyed)

 private:
  HStoreKeyed(HValue* obj, HValue* key, HValue* val,
              ElementsKind elements_kind)
      : elements_kind_(elements_kind),
      index_offset_(0),
      is_dehoisted_(false),
      is_uninitialized_(false),
      new_space_dominator_(NULL) {
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, val);

    if (IsFastObjectElementsKind(elements_kind)) {
      SetFlag(kTrackSideEffectDominators);
      SetGVNFlag(kDependsOnNewSpacePromotion);
    }
    if (is_external()) {
      SetGVNFlag(kChangesExternalMemory);
      SetFlag(kAllowUndefinedAsNaN);
    } else if (IsFastDoubleElementsKind(elements_kind)) {
      SetGVNFlag(kChangesDoubleArrayElements);
    } else if (IsFastSmiElementsKind(elements_kind)) {
      SetGVNFlag(kChangesArrayElements);
    } else {
      SetGVNFlag(kChangesArrayElements);
    }

    // EXTERNAL_{UNSIGNED_,}{BYTE,SHORT,INT}_ELEMENTS are truncating.
    if (elements_kind >= EXTERNAL_BYTE_ELEMENTS &&
        elements_kind <= EXTERNAL_UNSIGNED_INT_ELEMENTS) {
      SetFlag(kTruncatingToInt32);
    }
  }

  ElementsKind elements_kind_;
  uint32_t index_offset_;
  bool is_dehoisted_ : 1;
  bool is_uninitialized_ : 1;
  HValue* new_space_dominator_;
};


class HStoreKeyedGeneric V8_FINAL : public HTemplateInstruction<4> {
 public:
  HStoreKeyedGeneric(HValue* context,
                     HValue* object,
                     HValue* key,
                     HValue* value,
                     StrictModeFlag strict_mode_flag)
      : strict_mode_flag_(strict_mode_flag) {
    SetOperandAt(0, object);
    SetOperandAt(1, key);
    SetOperandAt(2, value);
    SetOperandAt(3, context);
    SetAllSideEffects();
  }

  HValue* object() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }
  HValue* value() { return OperandAt(2); }
  HValue* context() { return OperandAt(3); }
  StrictModeFlag strict_mode_flag() { return strict_mode_flag_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    // tagged[tagged] = tagged
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedGeneric)

 private:
  StrictModeFlag strict_mode_flag_;
};


class HTransitionElementsKind V8_FINAL : public HTemplateInstruction<2> {
 public:
  inline static HTransitionElementsKind* New(Zone* zone,
                                             HValue* context,
                                             HValue* object,
                                             Handle<Map> original_map,
                                             Handle<Map> transitioned_map) {
    return new(zone) HTransitionElementsKind(context, object,
                                             original_map, transitioned_map);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* object() { return OperandAt(0); }
  HValue* context() { return OperandAt(1); }
  Handle<Map> original_map() { return original_map_; }
  Handle<Map> transitioned_map() { return transitioned_map_; }
  ElementsKind from_kind() { return from_kind_; }
  ElementsKind to_kind() { return to_kind_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual void FinalizeUniqueValueId() V8_OVERRIDE {
    original_map_unique_id_ = UniqueValueId(original_map_);
    transitioned_map_unique_id_ = UniqueValueId(transitioned_map_);
  }

  DECLARE_CONCRETE_INSTRUCTION(TransitionElementsKind)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HTransitionElementsKind* instr = HTransitionElementsKind::cast(other);
    return original_map_unique_id_ == instr->original_map_unique_id_ &&
           transitioned_map_unique_id_ == instr->transitioned_map_unique_id_;
  }

 private:
  HTransitionElementsKind(HValue* context,
                          HValue* object,
                          Handle<Map> original_map,
                          Handle<Map> transitioned_map)
      : original_map_(original_map),
        transitioned_map_(transitioned_map),
        original_map_unique_id_(),
        transitioned_map_unique_id_(),
        from_kind_(original_map->elements_kind()),
        to_kind_(transitioned_map->elements_kind()) {
    SetOperandAt(0, object);
    SetOperandAt(1, context);
    SetFlag(kUseGVN);
    SetGVNFlag(kChangesElementsKind);
    if (!IsSimpleMapChangeTransition(from_kind_, to_kind_)) {
      SetGVNFlag(kChangesElementsPointer);
      SetGVNFlag(kChangesNewSpacePromotion);
    }
    set_representation(Representation::Tagged());
  }

  Handle<Map> original_map_;
  Handle<Map> transitioned_map_;
  UniqueValueId original_map_unique_id_;
  UniqueValueId transitioned_map_unique_id_;
  ElementsKind from_kind_;
  ElementsKind to_kind_;
};


class HStringAdd V8_FINAL : public HBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right,
                           StringAddFlags flags = STRING_ADD_CHECK_NONE);

  StringAddFlags flags() const { return flags_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StringAdd)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  HStringAdd(HValue* context, HValue* left, HValue* right, StringAddFlags flags)
      : HBinaryOperation(context, left, right, HType::String()), flags_(flags) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  // No side-effects except possible allocation.
  // NOTE: this instruction _does not_ call ToString() on its inputs.
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  const StringAddFlags flags_;
};


class HStringCharCodeAt V8_FINAL : public HTemplateInstruction<3> {
 public:
  static HStringCharCodeAt* New(Zone* zone,
                                HValue* context,
                                HValue* string,
                                HValue* index) {
    return new(zone) HStringCharCodeAt(context, string, index);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    // The index is supposed to be Integer32.
    return index == 2
        ? Representation::Integer32()
        : Representation::Tagged();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* string() const { return OperandAt(1); }
  HValue* index() const { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(StringCharCodeAt)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE {
    return new(zone) Range(0, String::kMaxUtf16CodeUnit);
  }

 private:
  HStringCharCodeAt(HValue* context, HValue* string, HValue* index) {
    SetOperandAt(0, context);
    SetOperandAt(1, string);
    SetOperandAt(2, index);
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  // No side effects: runtime function assumes string + number inputs.
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HStringCharFromCode V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* char_code);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }

  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

  DECLARE_CONCRETE_INSTRUCTION(StringCharFromCode)

 private:
  HStringCharFromCode(HValue* context, HValue* char_code)
      : HTemplateInstruction<2>(HType::String()) {
    SetOperandAt(0, context);
    SetOperandAt(1, char_code);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  virtual bool IsDeletable() const V8_OVERRIDE {
    return !value()->ToNumberCanBeObserved();
  }
};


template <int V>
class HMaterializedLiteral : public HTemplateInstruction<V> {
 public:
  HMaterializedLiteral<V>(int index, int depth, AllocationSiteMode mode)
      : literal_index_(index), depth_(depth), allocation_site_mode_(mode) {
    this->set_representation(Representation::Tagged());
  }

  HMaterializedLiteral<V>(int index, int depth)
      : literal_index_(index), depth_(depth),
        allocation_site_mode_(DONT_TRACK_ALLOCATION_SITE) {
    this->set_representation(Representation::Tagged());
  }

  int literal_index() const { return literal_index_; }
  int depth() const { return depth_; }
  AllocationSiteMode allocation_site_mode() const {
    return allocation_site_mode_;
  }

 private:
  virtual bool IsDeletable() const V8_FINAL V8_OVERRIDE { return true; }

  int literal_index_;
  int depth_;
  AllocationSiteMode allocation_site_mode_;
};


class HRegExpLiteral V8_FINAL : public HMaterializedLiteral<1> {
 public:
  HRegExpLiteral(HValue* context,
                 Handle<FixedArray> literals,
                 Handle<String> pattern,
                 Handle<String> flags,
                 int literal_index)
      : HMaterializedLiteral<1>(literal_index, 0),
        literals_(literals),
        pattern_(pattern),
        flags_(flags) {
    SetOperandAt(0, context);
    SetAllSideEffects();
    set_type(HType::JSObject());
  }

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> literals() { return literals_; }
  Handle<String> pattern() { return pattern_; }
  Handle<String> flags() { return flags_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(RegExpLiteral)

 private:
  Handle<FixedArray> literals_;
  Handle<String> pattern_;
  Handle<String> flags_;
};


class HFunctionLiteral V8_FINAL : public HTemplateInstruction<1> {
 public:
  HFunctionLiteral(HValue* context,
                   Handle<SharedFunctionInfo> shared,
                   bool pretenure)
      : HTemplateInstruction<1>(HType::JSObject()),
        shared_info_(shared),
        pretenure_(pretenure),
        has_no_literals_(shared->num_literals() == 0),
        is_generator_(shared->is_generator()),
        language_mode_(shared->language_mode()) {
    SetOperandAt(0, context);
    set_representation(Representation::Tagged());
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  HValue* context() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(FunctionLiteral)

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  bool pretenure() const { return pretenure_; }
  bool has_no_literals() const { return has_no_literals_; }
  bool is_generator() const { return is_generator_; }
  LanguageMode language_mode() const { return language_mode_; }

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  Handle<SharedFunctionInfo> shared_info_;
  bool pretenure_ : 1;
  bool has_no_literals_ : 1;
  bool is_generator_ : 1;
  LanguageMode language_mode_;
};


class HTypeof V8_FINAL : public HTemplateInstruction<2> {
 public:
  explicit HTypeof(HValue* context, HValue* value) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    set_representation(Representation::Tagged());
  }

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Typeof)

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HTrapAllocationMemento V8_FINAL : public HTemplateInstruction<1> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HTrapAllocationMemento, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* object() { return OperandAt(0); }

  DECLARE_CONCRETE_INSTRUCTION(TrapAllocationMemento)

 private:
  explicit HTrapAllocationMemento(HValue* obj) {
    SetOperandAt(0, obj);
  }
};


class HToFastProperties V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HToFastProperties, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ToFastProperties)

 private:
  explicit HToFastProperties(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetGVNFlag(kChangesNewSpacePromotion);

    // This instruction is not marked as kChangesMaps, but does
    // change the map of the input operand. Use it only when creating
    // object literals via a runtime call.
    ASSERT(value->IsCallRuntime());
#ifdef DEBUG
    const Runtime::Function* function = HCallRuntime::cast(value)->function();
    ASSERT(function->function_id == Runtime::kCreateObjectLiteral ||
           function->function_id == Runtime::kCreateObjectLiteralShallow);
#endif
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HValueOf V8_FINAL : public HUnaryOperation {
 public:
  explicit HValueOf(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ValueOf)

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HDateField V8_FINAL : public HUnaryOperation {
 public:
  HDateField(HValue* date, Smi* index)
      : HUnaryOperation(date), index_(index) {
    set_representation(Representation::Tagged());
  }

  Smi* index() const { return index_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(DateField)

 private:
  Smi* index_;
};


class HSeqStringSetChar V8_FINAL : public HTemplateInstruction<3> {
 public:
  HSeqStringSetChar(String::Encoding encoding,
                    HValue* string,
                    HValue* index,
                    HValue* value) : encoding_(encoding) {
    SetOperandAt(0, string);
    SetOperandAt(1, index);
    SetOperandAt(2, value);
    set_representation(Representation::Tagged());
  }

  String::Encoding encoding() { return encoding_; }
  HValue* string() { return OperandAt(0); }
  HValue* index() { return OperandAt(1); }
  HValue* value() { return OperandAt(2); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return (index == 0) ? Representation::Tagged()
                        : Representation::Integer32();
  }

  DECLARE_CONCRETE_INSTRUCTION(SeqStringSetChar)

 private:
  String::Encoding encoding_;
};


class HCheckMapValue V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HCheckMapValue, HValue*, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual HType CalculateInferredType() V8_OVERRIDE {
    return HType::Tagged();
  }

  HValue* value() { return OperandAt(0); }
  HValue* map() { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(CheckMapValue)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    return true;
  }

 private:
  HCheckMapValue(HValue* value,
                 HValue* map) {
    SetOperandAt(0, value);
    SetOperandAt(1, map);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
    SetGVNFlag(kDependsOnElementsKind);
  }
};


class HForInPrepareMap V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HForInPrepareMap* New(Zone* zone,
                               HValue* context,
                               HValue* object) {
    return new(zone) HForInPrepareMap(context, object);
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* context() { return OperandAt(0); }
  HValue* enumerable() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual HType CalculateInferredType() V8_OVERRIDE {
    return HType::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ForInPrepareMap);

 private:
  HForInPrepareMap(HValue* context,
                   HValue* object) {
    SetOperandAt(0, context);
    SetOperandAt(1, object);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }
};


class HForInCacheArray V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HForInCacheArray, HValue*, HValue*, int);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* enumerable() { return OperandAt(0); }
  HValue* map() { return OperandAt(1); }
  int idx() { return idx_; }

  HForInCacheArray* index_cache() {
    return index_cache_;
  }

  void set_index_cache(HForInCacheArray* index_cache) {
    index_cache_ = index_cache;
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual HType CalculateInferredType() V8_OVERRIDE {
    return HType::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ForInCacheArray);

 private:
  HForInCacheArray(HValue* enumerable,
                   HValue* keys,
                   int idx) : idx_(idx) {
    SetOperandAt(0, enumerable);
    SetOperandAt(1, keys);
    set_representation(Representation::Tagged());
  }

  int idx_;
  HForInCacheArray* index_cache_;
};


class HLoadFieldByIndex V8_FINAL : public HTemplateInstruction<2> {
 public:
  HLoadFieldByIndex(HValue* object,
                    HValue* index) {
    SetOperandAt(0, object);
    SetOperandAt(1, index);
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* object() { return OperandAt(0); }
  HValue* index() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual HType CalculateInferredType() V8_OVERRIDE {
    return HType::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFieldByIndex);

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


#undef DECLARE_INSTRUCTION
#undef DECLARE_CONCRETE_INSTRUCTION

} }  // namespace v8::internal

#endif  // V8_HYDROGEN_INSTRUCTIONS_H_
