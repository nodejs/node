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
#include "unique.h"
#include "v8conversions.h"
#include "v8utils.h"
#include "zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class HBasicBlock;
class HDiv;
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
  V(AbnormalExit)                              \
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
  V(CallWithDescriptor)                        \
  V(CallJSFunction)                            \
  V(CallFunction)                              \
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
  V(CompareMinusZeroAndBranch)                 \
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
  V(EnterInlined)                              \
  V(EnvironmentMarker)                         \
  V(ForceRepresentation)                       \
  V(ForInCacheArray)                           \
  V(ForInPrepareMap)                           \
  V(FunctionLiteral)                           \
  V(GetCachedArrayIndex)                       \
  V(Goto)                                      \
  V(HasCachedArrayIndexAndBranch)              \
  V(HasInstanceTypeAndBranch)                  \
  V(InnerAllocatedObject)                      \
  V(InstanceOf)                                \
  V(InstanceOfKnownGlobal)                     \
  V(InvokeFunction)                            \
  V(IsConstructCallAndBranch)                  \
  V(IsObjectAndBranch)                         \
  V(IsStringAndBranch)                         \
  V(IsSmiAndBranch)                            \
  V(IsUndetectableAndBranch)                   \
  V(LeaveInlined)                              \
  V(LoadContextSlot)                           \
  V(LoadFieldByIndex)                          \
  V(LoadFunctionPrototype)                     \
  V(LoadGlobalCell)                            \
  V(LoadGlobalGeneric)                         \
  V(LoadKeyed)                                 \
  V(LoadKeyedGeneric)                          \
  V(LoadNamedField)                            \
  V(LoadNamedGeneric)                          \
  V(LoadRoot)                                  \
  V(MapEnumLength)                             \
  V(MathFloorOfDiv)                            \
  V(MathMinMax)                                \
  V(Mod)                                       \
  V(Mul)                                       \
  V(OsrEntry)                                  \
  V(Parameter)                                 \
  V(Power)                                     \
  V(PushArgument)                              \
  V(RegExpLiteral)                             \
  V(Return)                                    \
  V(Ror)                                       \
  V(Sar)                                       \
  V(SeqStringGetChar)                          \
  V(SeqStringSetChar)                          \
  V(Shl)                                       \
  V(Shr)                                       \
  V(Simulate)                                  \
  V(StackCheck)                                \
  V(StoreCodeEntry)                            \
  V(StoreContextSlot)                          \
  V(StoreGlobalCell)                           \
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
  V(ToFastProperties)                          \
  V(TransitionElementsKind)                    \
  V(TrapAllocationMemento)                     \
  V(Typeof)                                    \
  V(TypeofIsAndBranch)                         \
  V(UnaryMathOperation)                        \
  V(UnknownOSRValue)                           \
  V(UseConst)                                  \
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
  V(ExternalMemory)                            \
  V(StringChars)                               \
  V(TypedArrayElements)


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


enum PropertyAccessType { LOAD, STORE };


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


// All tracked flags should appear before untracked ones.
enum GVNFlag {
  // Declare global value numbering flags.
#define DECLARE_FLAG(Type) k##Type,
  GVN_TRACKED_FLAG_LIST(DECLARE_FLAG)
  GVN_UNTRACKED_FLAG_LIST(DECLARE_FLAG)
#undef DECLARE_FLAG
#define COUNT_FLAG(Type) + 1
  kNumberOfTrackedSideEffects = 0 GVN_TRACKED_FLAG_LIST(COUNT_FLAG),
  kNumberOfUntrackedSideEffects = 0 GVN_UNTRACKED_FLAG_LIST(COUNT_FLAG),
#undef COUNT_FLAG
  kNumberOfFlags = kNumberOfTrackedSideEffects + kNumberOfUntrackedSideEffects
};


static inline GVNFlag GVNFlagFromInt(int i) {
  ASSERT(i >= 0);
  ASSERT(i < kNumberOfFlags);
  return static_cast<GVNFlag>(i);
}


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


typedef EnumSet<GVNFlag, int32_t> GVNFlagSet;


// This class encapsulates encoding and decoding of sources positions from
// which hydrogen values originated.
// When FLAG_track_hydrogen_positions is set this object encodes the
// identifier of the inlining and absolute offset from the start of the
// inlined function.
// When the flag is not set we simply track absolute offset from the
// script start.
class HSourcePosition {
 public:
  HSourcePosition(const HSourcePosition& other) : value_(other.value_) { }

  static HSourcePosition Unknown() {
    return HSourcePosition(RelocInfo::kNoPosition);
  }

  bool IsUnknown() const { return value_ == RelocInfo::kNoPosition; }

  int position() const { return PositionField::decode(value_); }
  void set_position(int position) {
    if (FLAG_hydrogen_track_positions) {
      value_ = static_cast<int>(PositionField::update(value_, position));
    } else {
      value_ = position;
    }
  }

  int inlining_id() const { return InliningIdField::decode(value_); }
  void set_inlining_id(int inlining_id) {
    if (FLAG_hydrogen_track_positions) {
      value_ = static_cast<int>(InliningIdField::update(value_, inlining_id));
    }
  }

  int raw() const { return value_; }

  void PrintTo(FILE* f);

 private:
  typedef BitField<int, 0, 9> InliningIdField;

  // Offset from the start of the inlined function.
  typedef BitField<int, 9, 22> PositionField;

  // On HPositionInfo can use this constructor.
  explicit HSourcePosition(int value) : value_(value) { }

  friend class HPositionInfo;

  // If FLAG_hydrogen_track_positions is set contains bitfields InliningIdField
  // and PositionField.
  // Otherwise contains absolute offset from the script start.
  int value_;
};


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
    // EXTERNAL_UINT32_ELEMENTS array is not marked with this flag
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

  virtual HSourcePosition position() const {
    return HSourcePosition::Unknown();
  }
  virtual HSourcePosition operand_position(int index) const {
    return position();
  }

  HBasicBlock* block() const { return block_; }
  void SetBlock(HBasicBlock* block);

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

  bool CanReplaceWithDummyUses();

  virtual int argument_delta() const { return 0; }

  // A purely informative definition is an idef that will not emit code and
  // should therefore be removed from the graph in the RestoreActualValues
  // phase (so that live ranges will be shorter).
  virtual bool IsPurelyInformativeDefinition() { return false; }

  // This method must always return the original HValue SSA definition,
  // regardless of any chain of iDefs of this value.
  HValue* ActualValue() {
    HValue* value = this;
    int index;
    while ((index = value->RedefinedOperandIndex()) != kNoRedefinedOperand) {
      value = value->OperandAt(index);
    }
    return value;
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
  void CopyFlag(Flag f, HValue* other) {
    if (other->CheckFlag(f)) SetFlag(f);
  }

  // Returns true if the flag specified is set for all uses, false otherwise.
  bool CheckUsesForFlag(Flag f) const;
  // Same as before and the first one without the flag is returned in value.
  bool CheckUsesForFlag(Flag f, HValue** value) const;
  // Returns true if the flag specified is set for all uses, and this set
  // of uses is non-empty.
  bool HasAtLeastOneUseWithFlagAndNoneWithout(Flag f) const;

  GVNFlagSet ChangesFlags() const { return changes_flags_; }
  GVNFlagSet DependsOnFlags() const { return depends_on_flags_; }
  void SetChangesFlag(GVNFlag f) { changes_flags_.Add(f); }
  void SetDependsOnFlag(GVNFlag f) { depends_on_flags_.Add(f); }
  void ClearChangesFlag(GVNFlag f) { changes_flags_.Remove(f); }
  void ClearDependsOnFlag(GVNFlag f) { depends_on_flags_.Remove(f); }
  bool CheckChangesFlag(GVNFlag f) const {
    return changes_flags_.Contains(f);
  }
  bool CheckDependsOnFlag(GVNFlag f) const {
    return depends_on_flags_.Contains(f);
  }
  void SetAllSideEffects() { changes_flags_.Add(AllSideEffectsFlagSet()); }
  void ClearAllSideEffects() {
    changes_flags_.Remove(AllSideEffectsFlagSet());
  }
  bool HasSideEffects() const {
    return changes_flags_.ContainsAnyOf(AllSideEffectsFlagSet());
  }
  bool HasObservableSideEffects() const {
    return !CheckFlag(kHasNoObservableSideEffects) &&
        changes_flags_.ContainsAnyOf(AllObservableSideEffectsFlagSet());
  }

  GVNFlagSet SideEffectFlags() const {
    GVNFlagSet result = ChangesFlags();
    result.Intersect(AllSideEffectsFlagSet());
    return result;
  }

  GVNFlagSet ObservableChangesFlags() const {
    GVNFlagSet result = ChangesFlags();
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
  virtual void FinalizeUniqueness() { }

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
  // It returns true if it removed an instruction which had side effects.
  virtual bool HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) {
    UNREACHABLE();
    return false;
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

  static GVNFlagSet AllFlagSet() {
    GVNFlagSet result;
#define ADD_FLAG(Type) result.Add(k##Type);
  GVN_TRACKED_FLAG_LIST(ADD_FLAG)
  GVN_UNTRACKED_FLAG_LIST(ADD_FLAG)
#undef ADD_FLAG
    return result;
  }

  // A flag mask to mark an instruction as having arbitrary side effects.
  static GVNFlagSet AllSideEffectsFlagSet() {
    GVNFlagSet result = AllFlagSet();
    result.Remove(kOsrEntries);
    return result;
  }

  // A flag mask of all side effects that can make observable changes in
  // an executing program (i.e. are not safe to repeat, move or remove);
  static GVNFlagSet AllObservableSideEffectsFlagSet() {
    GVNFlagSet result = AllFlagSet();
    result.Remove(kNewSpacePromotion);
    result.Remove(kElementsKind);
    result.Remove(kElementsPointer);
    result.Remove(kMaps);
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
  GVNFlagSet changes_flags_;
  GVNFlagSet depends_on_flags_;

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

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P0(I)                         \
  static I* New(Zone* zone, HValue* context) {                                 \
    return new(zone) I(context);                                               \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(I, P1)                     \
  static I* New(Zone* zone, HValue* context, P1 p1) {                          \
    return new(zone) I(context, p1);                                           \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(I, P1, P2)                 \
  static I* New(Zone* zone, HValue* context, P1 p1, P2 p2) {                   \
    return new(zone) I(context, p1, p2);                                       \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(I, P1, P2, P3)             \
  static I* New(Zone* zone, HValue* context, P1 p1, P2 p2, P3 p3) {            \
    return new(zone) I(context, p1, p2, p3);                                   \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(I, P1, P2, P3, P4)         \
  static I* New(Zone* zone,                                                    \
                HValue* context,                                               \
                P1 p1,                                                         \
                P2 p2,                                                         \
                P3 p3,                                                         \
                P4 p4) {                                                       \
    return new(zone) I(context, p1, p2, p3, p4);                               \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P5(I, P1, P2, P3, P4, P5)     \
  static I* New(Zone* zone,                                                    \
                HValue* context,                                               \
                P1 p1,                                                         \
                P2 p2,                                                         \
                P3 p3,                                                         \
                P4 p4,                                                         \
                P5 p5) {                                                       \
    return new(zone) I(context, p1, p2, p3, p4, p5);                           \
  }


// A helper class to represent per-operand position information attached to
// the HInstruction in the compact form. Uses tagging to distinguish between
// case when only instruction's position is available and case when operands'
// positions are also available.
// In the first case it contains intruction's position as a tagged value.
// In the second case it points to an array which contains instruction's
// position and operands' positions.
class HPositionInfo {
 public:
  explicit HPositionInfo(int pos) : data_(TagPosition(pos)) { }

  HSourcePosition position() const {
    if (has_operand_positions()) {
      return operand_positions()[kInstructionPosIndex];
    }
    return HSourcePosition(static_cast<int>(UntagPosition(data_)));
  }

  void set_position(HSourcePosition pos) {
    if (has_operand_positions()) {
      operand_positions()[kInstructionPosIndex] = pos;
    } else {
      data_ = TagPosition(pos.raw());
    }
  }

  void ensure_storage_for_operand_positions(Zone* zone, int operand_count) {
    if (has_operand_positions()) {
      return;
    }

    const int length = kFirstOperandPosIndex + operand_count;
    HSourcePosition* positions =
        zone->NewArray<HSourcePosition>(length);
    for (int i = 0; i < length; i++) {
      positions[i] = HSourcePosition::Unknown();
    }

    const HSourcePosition pos = position();
    data_ = reinterpret_cast<intptr_t>(positions);
    set_position(pos);

    ASSERT(has_operand_positions());
  }

  HSourcePosition operand_position(int idx) const {
    if (!has_operand_positions()) {
      return position();
    }
    return *operand_position_slot(idx);
  }

  void set_operand_position(int idx, HSourcePosition pos) {
    *operand_position_slot(idx) = pos;
  }

 private:
  static const intptr_t kInstructionPosIndex = 0;
  static const intptr_t kFirstOperandPosIndex = 1;

  HSourcePosition* operand_position_slot(int idx) const {
    ASSERT(has_operand_positions());
    return &(operand_positions()[kFirstOperandPosIndex + idx]);
  }

  bool has_operand_positions() const {
    return !IsTaggedPosition(data_);
  }

  HSourcePosition* operand_positions() const {
    ASSERT(has_operand_positions());
    return reinterpret_cast<HSourcePosition*>(data_);
  }

  static const intptr_t kPositionTag = 1;
  static const intptr_t kPositionShift = 1;
  static bool IsTaggedPosition(intptr_t val) {
    return (val & kPositionTag) != 0;
  }
  static intptr_t UntagPosition(intptr_t val) {
    ASSERT(IsTaggedPosition(val));
    return val >> kPositionShift;
  }
  static intptr_t TagPosition(intptr_t val) {
    const intptr_t result = (val << kPositionShift) | kPositionTag;
    ASSERT(UntagPosition(result) == val);
    return result;
  }

  intptr_t data_;
};


class HInstruction : public HValue {
 public:
  HInstruction* next() const { return next_; }
  HInstruction* previous() const { return previous_; }

  virtual void PrintTo(StringStream* stream) V8_OVERRIDE;
  virtual void PrintDataTo(StringStream* stream);

  bool IsLinked() const { return block() != NULL; }
  void Unlink();

  void InsertBefore(HInstruction* next);

  template<class T> T* Prepend(T* instr) {
    instr->InsertBefore(this);
    return instr;
  }

  void InsertAfter(HInstruction* previous);

  template<class T> T* Append(T* instr) {
    instr->InsertAfter(this);
    return instr;
  }

  // The position is a write-once variable.
  virtual HSourcePosition position() const V8_OVERRIDE {
    return HSourcePosition(position_.position());
  }
  bool has_position() const {
    return !position().IsUnknown();
  }
  void set_position(HSourcePosition position) {
    ASSERT(!has_position());
    ASSERT(!position.IsUnknown());
    position_.set_position(position);
  }

  virtual HSourcePosition operand_position(int index) const V8_OVERRIDE {
    const HSourcePosition pos = position_.operand_position(index);
    return pos.IsUnknown() ? position() : pos;
  }
  void set_operand_position(Zone* zone, int index, HSourcePosition pos) {
    ASSERT(0 <= index && index < OperandCount());
    position_.ensure_storage_for_operand_positions(zone, OperandCount());
    position_.set_operand_position(index, pos);
  }

  bool CanTruncateToInt32() const { return CheckFlag(kTruncatingToInt32); }

  virtual LInstruction* CompileToLithium(LChunkBuilder* builder) = 0;

#ifdef DEBUG
  virtual void Verify() V8_OVERRIDE;
#endif

  virtual bool HasStackCheck() { return false; }

  DECLARE_ABSTRACT_INSTRUCTION(Instruction)

 protected:
  HInstruction(HType type = HType::Tagged())
      : HValue(type),
        next_(NULL),
        previous_(NULL),
        position_(RelocInfo::kNoPosition) {
    SetDependsOnFlag(kOsrEntries);
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
  HPositionInfo position_;

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

  virtual bool KnownSuccessorBlock(HBasicBlock** block) {
    *block = NULL;
    return false;
  }

  HBasicBlock* FirstSuccessor() {
    return SuccessorCount() > 0 ? SuccessorAt(0) : NULL;
  }
  HBasicBlock* SecondSuccessor() {
    return SuccessorCount() > 1 ? SuccessorAt(1) : NULL;
  }

  void Not() {
    HBasicBlock* swap = SuccessorAt(0);
    SetSuccessorAt(0, SuccessorAt(1));
    SetSuccessorAt(1, swap);
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


// Inserts an int3/stop break instruction for debugging purposes.
class HDebugBreak V8_FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HDebugBreak);

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

  virtual bool KnownSuccessorBlock(HBasicBlock** block) V8_OVERRIDE {
    *block = FirstSuccessor();
    return true;
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(Goto)
};


class HDeoptimize V8_FINAL : public HTemplateControlInstruction<1, 0> {
 public:
  static HDeoptimize* New(Zone* zone,
                          HValue* context,
                          const char* reason,
                          Deoptimizer::BailoutType type,
                          HBasicBlock* unreachable_continuation) {
    return new(zone) HDeoptimize(reason, type, unreachable_continuation);
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) V8_OVERRIDE {
    *block = NULL;
    return true;
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  const char* reason() const { return reason_; }
  Deoptimizer::BailoutType type() { return type_; }

  DECLARE_CONCRETE_INSTRUCTION(Deoptimize)

 private:
  explicit HDeoptimize(const char* reason,
                       Deoptimizer::BailoutType type,
                       HBasicBlock* unreachable_continuation)
      : reason_(reason), type_(type) {
    SetSuccessorAt(0, unreachable_continuation);
  }

  const char* reason_;
  Deoptimizer::BailoutType type_;
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
  DECLARE_INSTRUCTION_FACTORY_P1(HBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P2(HBranch, HValue*,
                                 ToBooleanStub::Types);
  DECLARE_INSTRUCTION_FACTORY_P4(HBranch, HValue*,
                                 ToBooleanStub::Types,
                                 HBasicBlock*, HBasicBlock*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }
  virtual Representation observed_input_representation(int index) V8_OVERRIDE;

  virtual bool KnownSuccessorBlock(HBasicBlock** block) V8_OVERRIDE;

  ToBooleanStub::Types expected_input_types() const {
    return expected_input_types_;
  }

  DECLARE_CONCRETE_INSTRUCTION(Branch)

 private:
  HBranch(HValue* value,
          ToBooleanStub::Types expected_input_types = ToBooleanStub::Types(),
          HBasicBlock* true_target = NULL,
          HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target),
        expected_input_types_(expected_input_types) {
    SetFlag(kAllowUndefinedAsNaN);
  }

  ToBooleanStub::Types expected_input_types_;
};


class HCompareMap V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HCompareMap, HValue*, Handle<Map>,
                                 CompilationInfo*);
  DECLARE_INSTRUCTION_FACTORY_P5(HCompareMap, HValue*, Handle<Map>,
                                 CompilationInfo*,
                                 HBasicBlock*, HBasicBlock*);

  virtual bool KnownSuccessorBlock(HBasicBlock** block) V8_OVERRIDE {
    if (known_successor_index() != kNoKnownSuccessorIndex) {
      *block = SuccessorAt(known_successor_index());
      return true;
    }
    *block = NULL;
    return false;
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  static const int kNoKnownSuccessorIndex = -1;
  int known_successor_index() const { return known_successor_index_; }
  void set_known_successor_index(int known_successor_index) {
    known_successor_index_ = known_successor_index;
  }

  Unique<Map> map() const { return map_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  bool is_stable() const {
    return is_stable_;
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareMap)

 protected:
  virtual int RedefinedOperandIndex() { return 0; }

 private:
  HCompareMap(HValue* value,
              Handle<Map> map,
              CompilationInfo* info,
              HBasicBlock* true_target = NULL,
              HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target),
        known_successor_index_(kNoKnownSuccessorIndex), map_(Unique<Map>(map)) {
    ASSERT(!map.is_null());
    is_stable_ = map->is_stable();

    if (is_stable_) {
      map->AddDependentCompilationInfo(
          DependentCode::kPrototypeCheckGroup, info);
    }
  }

  int known_successor_index_;
  bool is_stable_;
  Unique<Map> map_;
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
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HReturn, HValue*, HValue*);
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HReturn, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    // TODO(titzer): require an Int32 input for faster returns.
    if (index == 2) return Representation::Smi();
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* value() { return OperandAt(0); }
  HValue* context() { return OperandAt(1); }
  HValue* parameter_count() { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(Return)

 private:
  HReturn(HValue* context, HValue* value, HValue* parameter_count = 0) {
    SetOperandAt(0, value);
    SetOperandAt(1, context);
    SetOperandAt(2, parameter_count);
  }
};


class HAbnormalExit V8_FINAL : public HTemplateControlInstruction<0, 0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HAbnormalExit);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(AbnormalExit)
 private:
  HAbnormalExit() {}
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
  static HInstruction* New(Zone* zone, HValue* context, HValue* value,
                           Representation required_representation);

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
      if (to.IsTagged()) SetChangesFlag(kNewSpacePromotion);
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

  DECLARE_INSTRUCTION_FACTORY_P2(HEnvironmentMarker, Kind, int);

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
  HEnvironmentMarker(Kind kind, int index)
      : kind_(kind), index_(index), next_simulate_(NULL) { }

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

  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HStackCheck, Type);

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
    SetChangesFlag(kNewSpacePromotion);
  }

  Type type_;
};


enum InliningKind {
  NORMAL_RETURN,          // Drop the function from the environment on return.
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
                            HArgumentsObject* arguments_object) {
    return new(zone) HEnterInlined(closure, arguments_count, function,
                                   inlining_kind, arguments_var,
                                   arguments_object, zone);
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
                Zone* zone)
      : closure_(closure),
        arguments_count_(arguments_count),
        arguments_pushed_(false),
        function_(function),
        inlining_kind_(inlining_kind),
        arguments_var_(arguments_var),
        arguments_object_(arguments_object),
        return_targets_(2, zone) {
  }

  Handle<JSFunction> closure_;
  int arguments_count_;
  bool arguments_pushed_;
  FunctionLiteral* function_;
  InliningKind inlining_kind_;
  Variable* arguments_var_;
  HArgumentsObject* arguments_object_;
  ZoneList<HBasicBlock*> return_targets_;
};


class HLeaveInlined V8_FINAL : public HTemplateInstruction<0> {
 public:
  HLeaveInlined(HEnterInlined* entry,
                int drop_count)
      : entry_(entry),
        drop_count_(drop_count) { }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  virtual int argument_delta() const V8_OVERRIDE {
    return entry_->arguments_pushed() ? -drop_count_ : 0;
  }

  DECLARE_CONCRETE_INSTRUCTION(LeaveInlined)

 private:
  HEnterInlined* entry_;
  int drop_count_;
};


class HPushArgument V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HPushArgument, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual int argument_delta() const V8_OVERRIDE { return 1; }
  HValue* argument() { return OperandAt(0); }

  DECLARE_CONCRETE_INSTRUCTION(PushArgument)

 private:
  explicit HPushArgument(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
  }
};


class HThisFunction V8_FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HThisFunction);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ThisFunction)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  HThisFunction() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HDeclareGlobals V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HDeclareGlobals,
                                              Handle<FixedArray>,
                                              int);

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> pairs() const { return pairs_; }
  int flags() const { return flags_; }

  DECLARE_CONCRETE_INSTRUCTION(DeclareGlobals)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

 private:
  HDeclareGlobals(HValue* context,
                  Handle<FixedArray> pairs,
                  int flags)
      : HUnaryOperation(context),
        pairs_(pairs),
        flags_(flags) {
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  Handle<FixedArray> pairs_;
  int flags_;
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

  virtual int argument_count() const {
    return argument_count_;
  }

  virtual int argument_delta() const V8_OVERRIDE {
    return -argument_count();
  }

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


class HCallJSFunction V8_FINAL : public HCall<1> {
 public:
  static HCallJSFunction* New(Zone* zone,
                              HValue* context,
                              HValue* function,
                              int argument_count,
                              bool pass_argument_count);

  HValue* function() { return OperandAt(0); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(
      int index) V8_FINAL V8_OVERRIDE {
    ASSERT(index == 0);
    return Representation::Tagged();
  }

  bool pass_argument_count() const { return pass_argument_count_; }

  virtual bool HasStackCheck() V8_FINAL V8_OVERRIDE {
    return has_stack_check_;
  }

  DECLARE_CONCRETE_INSTRUCTION(CallJSFunction)

 private:
  // The argument count includes the receiver.
  HCallJSFunction(HValue* function,
                  int argument_count,
                  bool pass_argument_count,
                  bool has_stack_check)
      : HCall<1>(argument_count),
        pass_argument_count_(pass_argument_count),
        has_stack_check_(has_stack_check) {
      SetOperandAt(0, function);
  }

  bool pass_argument_count_;
  bool has_stack_check_;
};


class HCallWithDescriptor V8_FINAL : public HInstruction {
 public:
  static HCallWithDescriptor* New(Zone* zone, HValue* context,
      HValue* target,
      int argument_count,
      const CallInterfaceDescriptor* descriptor,
      Vector<HValue*>& operands) {
    ASSERT(operands.length() == descriptor->environment_length());
    HCallWithDescriptor* res =
        new(zone) HCallWithDescriptor(target, argument_count,
                                      descriptor, operands, zone);
    return res;
  }

  virtual int OperandCount() V8_FINAL V8_OVERRIDE { return values_.length(); }
  virtual HValue* OperandAt(int index) const V8_FINAL V8_OVERRIDE {
    return values_[index];
  }

  virtual Representation RequiredInputRepresentation(
      int index) V8_FINAL V8_OVERRIDE {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      int par_index = index - 1;
      ASSERT(par_index < descriptor_->environment_length());
      return descriptor_->GetParameterRepresentation(par_index);
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(CallWithDescriptor)

  virtual HType CalculateInferredType() V8_FINAL V8_OVERRIDE {
    return HType::Tagged();
  }

  virtual int argument_count() const {
    return argument_count_;
  }

  virtual int argument_delta() const V8_OVERRIDE {
    return -argument_count_;
  }

  const CallInterfaceDescriptor* descriptor() const {
    return descriptor_;
  }

  HValue* target() {
    return OperandAt(0);
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

 private:
  // The argument count includes the receiver.
  HCallWithDescriptor(HValue* target,
                      int argument_count,
                      const CallInterfaceDescriptor* descriptor,
                      Vector<HValue*>& operands,
                      Zone* zone)
    : descriptor_(descriptor),
      values_(descriptor->environment_length() + 1, zone) {
    argument_count_ = argument_count;
    AddOperand(target, zone);
    for (int i = 0; i < operands.length(); i++) {
      AddOperand(operands[i], zone);
    }
    this->set_representation(Representation::Tagged());
    this->SetAllSideEffects();
  }

  void AddOperand(HValue* v, Zone* zone) {
    values_.Add(NULL, zone);
    SetOperandAt(values_.length() - 1, v);
  }

  void InternalSetOperandAt(int index,
                            HValue* value) V8_FINAL V8_OVERRIDE {
    values_[index] = value;
  }

  const CallInterfaceDescriptor* descriptor_;
  ZoneList<HValue*> values_;
  int argument_count_;
};


class HInvokeFunction V8_FINAL : public HBinaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HInvokeFunction, HValue*, int);

  HInvokeFunction(HValue* context,
                  HValue* function,
                  Handle<JSFunction> known_function,
                  int argument_count)
      : HBinaryCall(context, function, argument_count),
        known_function_(known_function) {
    formal_parameter_count_ = known_function.is_null()
        ? 0 : known_function->shared()->formal_parameter_count();
    has_stack_check_ = !known_function.is_null() &&
        (known_function->code()->kind() == Code::FUNCTION ||
         known_function->code()->kind() == Code::OPTIMIZED_FUNCTION);
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

  virtual bool HasStackCheck() V8_FINAL V8_OVERRIDE {
    return has_stack_check_;
  }

  DECLARE_CONCRETE_INSTRUCTION(InvokeFunction)

 private:
  HInvokeFunction(HValue* context, HValue* function, int argument_count)
      : HBinaryCall(context, function, argument_count),
        has_stack_check_(false) {
  }

  Handle<JSFunction> known_function_;
  int formal_parameter_count_;
  bool has_stack_check_;
};


class HCallFunction V8_FINAL : public HBinaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HCallFunction, HValue*, int);
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(
      HCallFunction, HValue*, int, CallFunctionFlags);

  HValue* context() { return first(); }
  HValue* function() { return second(); }
  CallFunctionFlags function_flags() const { return function_flags_; }

  DECLARE_CONCRETE_INSTRUCTION(CallFunction)

  virtual int argument_delta() const V8_OVERRIDE { return -argument_count(); }

 private:
  HCallFunction(HValue* context,
                HValue* function,
                int argument_count,
                CallFunctionFlags flags = NO_CALL_FUNCTION_FLAGS)
      : HBinaryCall(context, function, argument_count), function_flags_(flags) {
  }
  CallFunctionFlags function_flags_;
};


class HCallNew V8_FINAL : public HBinaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HCallNew, HValue*, int);

  HValue* context() { return first(); }
  HValue* constructor() { return second(); }

  DECLARE_CONCRETE_INSTRUCTION(CallNew)

 private:
  HCallNew(HValue* context, HValue* constructor, int argument_count)
      : HBinaryCall(context, constructor, argument_count) {}
};


class HCallNewArray V8_FINAL : public HBinaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HCallNewArray,
                                              HValue*,
                                              int,
                                              ElementsKind);

  HValue* context() { return first(); }
  HValue* constructor() { return second(); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  ElementsKind elements_kind() const { return elements_kind_; }

  DECLARE_CONCRETE_INSTRUCTION(CallNewArray)

 private:
  HCallNewArray(HValue* context, HValue* constructor, int argument_count,
                ElementsKind elements_kind)
      : HBinaryCall(context, constructor, argument_count),
        elements_kind_(elements_kind) {}

  ElementsKind elements_kind_;
};


class HCallRuntime V8_FINAL : public HCall<1> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HCallRuntime,
                                              Handle<String>,
                                              const Runtime::Function*,
                                              int);

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* context() { return OperandAt(0); }
  const Runtime::Function* function() const { return c_function_; }
  Handle<String> name() const { return name_; }
  SaveFPRegsMode save_doubles() const { return save_doubles_; }
  void set_save_doubles(SaveFPRegsMode save_doubles) {
    save_doubles_ = save_doubles;
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallRuntime)

 private:
  HCallRuntime(HValue* context,
               Handle<String> name,
               const Runtime::Function* c_function,
               int argument_count)
      : HCall<1>(argument_count), c_function_(c_function), name_(name),
        save_doubles_(kDontSaveFPRegs) {
    SetOperandAt(0, context);
  }

  const Runtime::Function* c_function_;
  Handle<String> name_;
  SaveFPRegsMode save_doubles_;
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
    SetDependsOnFlag(kMaps);
  }

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
        SetChangesFlag(kNewSpacePromotion);
        break;
      case kMathLog:
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

  HValue* SimplifiedDividendForMathFloorOfDiv(HDiv* hdiv);
  HValue* SimplifiedDivisorForMathFloorOfDiv(HDiv* hdiv);

  BuiltinFunctionId op_;
};


class HLoadRoot V8_FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HLoadRoot, Heap::RootListIndex);
  DECLARE_INSTRUCTION_FACTORY_P2(HLoadRoot, Heap::RootListIndex, HType);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  Heap::RootListIndex index() const { return index_; }

  DECLARE_CONCRETE_INSTRUCTION(LoadRoot)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HLoadRoot* b = HLoadRoot::cast(other);
    return index_ == b->index_;
  }

 private:
  HLoadRoot(Heap::RootListIndex index, HType type = HType::Tagged())
      : HTemplateInstruction<0>(type), index_(index) {
    SetFlag(kUseGVN);
    // TODO(bmeurer): We'll need kDependsOnRoots once we add the
    // corresponding HStoreRoot instruction.
    SetDependsOnFlag(kCalls);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  const Heap::RootListIndex index_;
};


class HCheckMaps V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HCheckMaps* New(Zone* zone, HValue* context, HValue* value,
                         Handle<Map> map, CompilationInfo* info,
                         HValue* typecheck = NULL);
  static HCheckMaps* New(Zone* zone, HValue* context,
                         HValue* value, SmallMapList* maps,
                         CompilationInfo* info,
                         HValue* typecheck = NULL) {
    HCheckMaps* check_map = new(zone) HCheckMaps(value, zone, typecheck);
    for (int i = 0; i < maps->length(); i++) {
      check_map->Add(maps->at(i), info, zone);
    }
    return check_map;
  }

  bool CanOmitMapChecks() { return omit_; }

  virtual bool HasEscapingOperandAt(int index) V8_OVERRIDE { return false; }
  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }
  virtual bool HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) V8_OVERRIDE;
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  HValue* value() { return OperandAt(0); }
  HValue* typecheck() { return OperandAt(1); }

  Unique<Map> first_map() const { return map_set_.at(0); }
  UniqueSet<Map> map_set() const { return map_set_; }

  void set_map_set(UniqueSet<Map>* maps, Zone *zone) {
    map_set_.Clear();
    for (int i = 0; i < maps->size(); i++) {
      map_set_.Add(maps->at(i), zone);
    }
  }

  bool has_migration_target() const {
    return has_migration_target_;
  }

  bool is_stable() const {
    return is_stable_;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckMaps)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    return this->map_set_.Equals(&HCheckMaps::cast(other)->map_set_);
  }

  virtual int RedefinedOperandIndex() { return 0; }

 private:
  void Add(Handle<Map> map, CompilationInfo* info, Zone* zone) {
    map_set_.Add(Unique<Map>(map), zone);
    is_stable_ = is_stable_ && map->is_stable();
    if (is_stable_) {
      map->AddDependentCompilationInfo(
          DependentCode::kPrototypeCheckGroup, info);
    } else {
      SetDependsOnFlag(kMaps);
      SetDependsOnFlag(kElementsKind);
    }

    if (!has_migration_target_ && map->is_migration_target()) {
      has_migration_target_ = true;
      SetChangesFlag(kNewSpacePromotion);
    }
  }

  // Clients should use one of the static New* methods above.
  HCheckMaps(HValue* value, Zone *zone, HValue* typecheck)
      : HTemplateInstruction<2>(value->type()),
        omit_(false), has_migration_target_(false), is_stable_(true) {
    SetOperandAt(0, value);
    // Use the object value for the dependency if NULL is passed.
    SetOperandAt(1, typecheck != NULL ? typecheck : value);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kTrackSideEffectDominators);
  }

  bool omit_;
  bool has_migration_target_;
  bool is_stable_;
  UniqueSet<Map> map_set_;
};


class HCheckValue V8_FINAL : public HUnaryOperation {
 public:
  static HCheckValue* New(Zone* zone, HValue* context,
                          HValue* value, Handle<JSFunction> func) {
    bool in_new_space = zone->isolate()->heap()->InNewSpace(*func);
    // NOTE: We create an uninitialized Unique and initialize it later.
    // This is because a JSFunction can move due to GC during graph creation.
    // TODO(titzer): This is a migration crutch. Replace with some kind of
    // Uniqueness scope later.
    Unique<JSFunction> target = Unique<JSFunction>::CreateUninitialized(func);
    HCheckValue* check = new(zone) HCheckValue(value, target, in_new_space);
    return check;
  }
  static HCheckValue* New(Zone* zone, HValue* context,
                          HValue* value, Unique<HeapObject> target,
                          bool object_in_new_space) {
    return new(zone) HCheckValue(value, target, object_in_new_space);
  }

  virtual void FinalizeUniqueness() V8_OVERRIDE {
    object_ = Unique<HeapObject>(object_.handle());
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual HValue* Canonicalize() V8_OVERRIDE;

#ifdef DEBUG
  virtual void Verify() V8_OVERRIDE;
#endif

  Unique<HeapObject> object() const { return object_; }
  bool object_in_new_space() const { return object_in_new_space_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckValue)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HCheckValue* b = HCheckValue::cast(other);
    return object_ == b->object_;
  }

 private:
  HCheckValue(HValue* value, Unique<HeapObject> object,
               bool object_in_new_space)
      : HUnaryOperation(value, value->type()),
        object_(object),
        object_in_new_space_(object_in_new_space) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  Unique<HeapObject> object_;
  bool object_in_new_space_;
};


class HCheckInstanceType V8_FINAL : public HUnaryOperation {
 public:
  enum Check {
    IS_SPEC_OBJECT,
    IS_JS_ARRAY,
    IS_STRING,
    IS_INTERNALIZED_STRING,
    LAST_INTERVAL_CHECK = IS_JS_ARRAY
  };

  DECLARE_INSTRUCTION_FACTORY_P2(HCheckInstanceType, HValue*, Check);

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

  virtual HSourcePosition position() const V8_OVERRIDE;

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

  virtual bool IsDeletable() const V8_FINAL V8_OVERRIDE { return true; }
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
    changes_flags_.Add(store->ChangesFlags());
  }

  // Replay effects of this instruction on the given environment.
  void ReplayEnvironment(HEnvironment* env);

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CapturedObject)

 private:
  int capture_id_;

  // Note that we cannot DCE captured objects as they are used to replay
  // the environment. This method is here as an explicit reminder.
  // TODO(mstarzinger): Turn HSimulates into full snapshots maybe?
  virtual bool IsDeletable() const V8_FINAL V8_OVERRIDE { return false; }
};


class HConstant V8_FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, int32_t);
  DECLARE_INSTRUCTION_FACTORY_P2(HConstant, int32_t, Representation);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, double);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, Handle<Object>);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, ExternalReference);

  static HConstant* CreateAndInsertAfter(Zone* zone,
                                         HValue* context,
                                         int32_t value,
                                         Representation representation,
                                         HInstruction* instruction) {
    return instruction->Append(HConstant::New(
        zone, context, value, representation));
  }

  static HConstant* CreateAndInsertBefore(Zone* zone,
                                          HValue* context,
                                          int32_t value,
                                          Representation representation,
                                          HInstruction* instruction) {
    return instruction->Prepend(HConstant::New(
        zone, context, value, representation));
  }

  static HConstant* CreateAndInsertBefore(Zone* zone,
                                          Unique<Object> unique,
                                          bool is_not_in_new_space,
                                          HInstruction* instruction) {
    return instruction->Prepend(new(zone) HConstant(
        unique, Representation::Tagged(), HType::Tagged(), false,
        is_not_in_new_space, false, false));
  }

  Handle<Object> handle(Isolate* isolate) {
    if (object_.handle().is_null()) {
      // Default arguments to is_not_in_new_space depend on this heap number
      // to be tenured so that it's guaranteed not to be located in new space.
      object_ = Unique<Object>::CreateUninitialized(
          isolate->factory()->NewNumber(double_value_, TENURED));
    }
    AllowDeferredHandleDereference smi_check;
    ASSERT(has_int32_value_ || !object_.handle()->IsSmi());
    return object_.handle();
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

  bool ImmortalImmovable() const;

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
    return object_.IsKnownGlobal(isolate()->heap()->the_hole_value());
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
    ASSERT(!object_.handle().is_null());
    return type_.IsString();
  }
  Handle<String> StringValue() const {
    ASSERT(HasStringValue());
    return Handle<String>::cast(object_.handle());
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
      ASSERT(!object_.handle().is_null());
      return object_.Hashcode();
    }
  }

  virtual void FinalizeUniqueness() V8_OVERRIDE {
    if (!has_double_value_ && !has_external_reference_value_) {
      ASSERT(!object_.handle().is_null());
      object_ = Unique<Object>(object_.handle());
    }
  }

  Unique<Object> GetUnique() const {
    return object_;
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
      if (other_constant->has_int32_value_ ||
          other_constant->has_double_value_ ||
          other_constant->has_external_reference_value_) {
        return false;
      }
      ASSERT(!object_.handle().is_null());
      return other_constant->object_ == object_;
    }
  }

 private:
  friend class HGraph;
  HConstant(Handle<Object> handle, Representation r = Representation::None());
  HConstant(int32_t value,
            Representation r = Representation::None(),
            bool is_not_in_new_space = true,
            Unique<Object> optional = Unique<Object>(Handle<Object>::null()));
  HConstant(double value,
            Representation r = Representation::None(),
            bool is_not_in_new_space = true,
            Unique<Object> optional = Unique<Object>(Handle<Object>::null()));
  HConstant(Unique<Object> unique,
            Representation r,
            HType type,
            bool is_internalized_string,
            bool is_not_in_new_space,
            bool is_cell,
            bool boolean_value);

  explicit HConstant(ExternalReference reference);

  void Initialize(Representation r);

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  // If this is a numerical constant, object_ either points to the
  // HeapObject the constant originated from or is null.  If the
  // constant is non-numeric, object_ always points to a valid
  // constant HeapObject.
  Unique<Object> object_;

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

  void SetOperandPositions(Zone* zone,
                           HSourcePosition left_pos,
                           HSourcePosition right_pos) {
    set_operand_position(zone, 1, left_pos);
    set_operand_position(zone, 2, right_pos);
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

  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  HValue* receiver() { return OperandAt(0); }
  HValue* function() { return OperandAt(1); }

  virtual HValue* Canonicalize() V8_OVERRIDE;

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
  bool known_function() const { return known_function_; }

  DECLARE_CONCRETE_INSTRUCTION(WrapReceiver)

 private:
  HWrapReceiver(HValue* receiver, HValue* function) {
    known_function_ = function->IsConstant() &&
        HConstant::cast(function)->handle(function->isolate())->IsJSFunction();
    set_representation(Representation::Tagged());
    SetOperandAt(0, receiver);
    SetOperandAt(1, function);
    SetFlag(kUseGVN);
  }

  bool known_function_;
};


class HApplyArguments V8_FINAL : public HTemplateInstruction<4> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P4(HApplyArguments, HValue*, HValue*, HValue*,
                                 HValue*);

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

 private:
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
  DECLARE_INSTRUCTION_FACTORY_P3(HAccessArgumentsAt, HValue*, HValue*, HValue*);

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

 private:
  HAccessArgumentsAt(HValue* arguments, HValue* length, HValue* index) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetOperandAt(0, arguments);
    SetOperandAt(1, length);
    SetOperandAt(2, index);
  }

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

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE;

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
    if (to.IsTagged()) SetChangesFlag(kNewSpacePromotion);
    if (to.IsTagged() &&
        (left()->ToNumberCanBeObserved() || right()->ToNumberCanBeObserved())) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
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
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HMathFloorOfDiv,
                                              HValue*,
                                              HValue*);

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
    if (to.IsTagged()) SetChangesFlag(kNewSpacePromotion);
    if (to.IsTagged() &&
        (left()->ToNumberCanBeObserved() || right()->ToNumberCanBeObserved())) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
  }

  bool RightIsPowerOf2() {
    if (!right()->IsInteger32Constant()) return false;
    int32_t value = right()->GetInteger32Constant();
    return value != 0 && (IsPowerOf2(value) || IsPowerOf2(-value));
  }

  DECLARE_ABSTRACT_INSTRUCTION(ArithmeticBinaryOperation)

 private:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HCompareGeneric V8_FINAL : public HBinaryOperation {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HCompareGeneric, HValue*,
                                              HValue*, Token::Value);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return index == 0
        ? Representation::Tagged()
        : representation();
  }

  Token::Value token() const { return token_; }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CompareGeneric)

 private:
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

  Token::Value token_;
};


class HCompareNumericAndBranch : public HTemplateControlInstruction<2, 2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HCompareNumericAndBranch,
                                 HValue*, HValue*, Token::Value);
  DECLARE_INSTRUCTION_FACTORY_P5(HCompareNumericAndBranch,
                                 HValue*, HValue*, Token::Value,
                                 HBasicBlock*, HBasicBlock*);

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

  void SetOperandPositions(Zone* zone,
                           HSourcePosition left_pos,
                           HSourcePosition right_pos) {
    set_operand_position(zone, 0, left_pos);
    set_operand_position(zone, 1, right_pos);
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareNumericAndBranch)

 private:
  HCompareNumericAndBranch(HValue* left,
                           HValue* right,
                           Token::Value token,
                           HBasicBlock* true_target = NULL,
                           HBasicBlock* false_target = NULL)
      : token_(token) {
    SetFlag(kFlexibleRepresentation);
    ASSERT(Token::IsCompareOp(token));
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    SetSuccessorAt(0, true_target);
    SetSuccessorAt(1, false_target);
  }

  Representation observed_input_representation_[2];
  Token::Value token_;
};


class HCompareHoleAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCompareHoleAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HCompareHoleAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return representation();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareHoleAndBranch)

 private:
  HCompareHoleAndBranch(HValue* value,
                        HBasicBlock* true_target = NULL,
                        HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target) {
    SetFlag(kFlexibleRepresentation);
    SetFlag(kAllowUndefinedAsNaN);
  }
};


class HCompareMinusZeroAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCompareMinusZeroAndBranch, HValue*);

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return representation();
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CompareMinusZeroAndBranch)

 private:
  explicit HCompareMinusZeroAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) {
  }
};


class HCompareObjectEqAndBranch : public HTemplateControlInstruction<2, 2> {
 public:
  HCompareObjectEqAndBranch(HValue* left,
                            HValue* right,
                            HBasicBlock* true_target = NULL,
                            HBasicBlock* false_target = NULL) {
    // TODO(danno): make this private when the IfBuilder properly constructs
    // control flow instructions.
    ASSERT(!left->IsConstant() ||
           (!HConstant::cast(left)->HasInteger32Value() ||
            HConstant::cast(left)->HasSmiValue()));
    ASSERT(!right->IsConstant() ||
           (!HConstant::cast(right)->HasInteger32Value() ||
            HConstant::cast(right)->HasSmiValue()));
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    SetSuccessorAt(0, true_target);
    SetSuccessorAt(1, false_target);
  }

  DECLARE_INSTRUCTION_FACTORY_P2(HCompareObjectEqAndBranch, HValue*, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P4(HCompareObjectEqAndBranch, HValue*, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual bool KnownSuccessorBlock(HBasicBlock** block) V8_OVERRIDE;

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
  DECLARE_INSTRUCTION_FACTORY_P1(HIsObjectAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsObjectAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsObjectAndBranch)

 private:
  HIsObjectAndBranch(HValue* value,
                     HBasicBlock* true_target = NULL,
                     HBasicBlock* false_target = NULL)
    : HUnaryControlInstruction(value, true_target, false_target) {}
};


class HIsStringAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsStringAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsStringAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsStringAndBranch)

 protected:
  virtual int RedefinedOperandIndex() { return 0; }

 private:
  HIsStringAndBranch(HValue* value,
                     HBasicBlock* true_target = NULL,
                     HBasicBlock* false_target = NULL)
    : HUnaryControlInstruction(value, true_target, false_target) {}
};


class HIsSmiAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsSmiAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsSmiAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }
  virtual int RedefinedOperandIndex() { return 0; }

 private:
  HIsSmiAndBranch(HValue* value,
                  HBasicBlock* true_target = NULL,
                  HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target) {}
};


class HIsUndetectableAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsUndetectableAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsUndetectableAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsUndetectableAndBranch)

 private:
  HIsUndetectableAndBranch(HValue* value,
                           HBasicBlock* true_target = NULL,
                           HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target) {}
};


class HStringCompareAndBranch : public HTemplateControlInstruction<2, 3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HStringCompareAndBranch,
                                              HValue*,
                                              HValue*,
                                              Token::Value);

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
    SetChangesFlag(kNewSpacePromotion);
  }

  Token::Value token_;
};


class HIsConstructCallAndBranch : public HTemplateControlInstruction<2, 0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HIsConstructCallAndBranch);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsConstructCallAndBranch)
 private:
  HIsConstructCallAndBranch() {}
};


class HHasInstanceTypeAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(
      HHasInstanceTypeAndBranch, HValue*, InstanceType);
  DECLARE_INSTRUCTION_FACTORY_P3(
      HHasInstanceTypeAndBranch, HValue*, InstanceType, InstanceType);

  InstanceType from() { return from_; }
  InstanceType to() { return to_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceTypeAndBranch)

 private:
  HHasInstanceTypeAndBranch(HValue* value, InstanceType type)
      : HUnaryControlInstruction(value, NULL, NULL), from_(type), to_(type) { }
  HHasInstanceTypeAndBranch(HValue* value, InstanceType from, InstanceType to)
      : HUnaryControlInstruction(value, NULL, NULL), from_(from), to_(to) {
    ASSERT(to == LAST_TYPE);  // Others not implemented yet in backend.
  }

  InstanceType from_;
  InstanceType to_;  // Inclusive range, not all combinations work.
};


class HHasCachedArrayIndexAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HHasCachedArrayIndexAndBranch, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndexAndBranch)
 private:
  explicit HHasCachedArrayIndexAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }
};


class HGetCachedArrayIndex V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HGetCachedArrayIndex, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(GetCachedArrayIndex)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HGetCachedArrayIndex(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HClassOfTestAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HClassOfTestAndBranch, HValue*,
                                 Handle<String>);

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<String> class_name() const { return class_name_; }

 private:
  HClassOfTestAndBranch(HValue* value, Handle<String> class_name)
      : HUnaryControlInstruction(value, NULL, NULL),
        class_name_(class_name) { }

  Handle<String> class_name_;
};


class HTypeofIsAndBranch V8_FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HTypeofIsAndBranch, HValue*, Handle<String>);

  Handle<String> type_literal() { return type_literal_; }
  bool compares_number_type() { return compares_number_type_; }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(TypeofIsAndBranch)

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) V8_OVERRIDE;

 private:
  HTypeofIsAndBranch(HValue* value, Handle<String> type_literal)
      : HUnaryControlInstruction(value, NULL, NULL),
        type_literal_(type_literal) {
    Heap* heap = type_literal->GetHeap();
    compares_number_type_ = type_literal->Equals(heap->number_string());
  }

  Handle<String> type_literal_;
  bool compares_number_type_ : 1;
};


class HInstanceOf V8_FINAL : public HBinaryOperation {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HInstanceOf, HValue*, HValue*);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(InstanceOf)

 private:
  HInstanceOf(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right, HType::Boolean()) {
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }
};


class HInstanceOfKnownGlobal V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HInstanceOfKnownGlobal,
                                              HValue*,
                                              Handle<JSFunction>);

  HValue* context() { return OperandAt(0); }
  HValue* left() { return OperandAt(1); }
  Handle<JSFunction> function() { return function_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfKnownGlobal)

 private:
  HInstanceOfKnownGlobal(HValue* context,
                         HValue* left,
                         Handle<JSFunction> right)
      : HTemplateInstruction<2>(HType::Boolean()), function_(right) {
    SetOperandAt(0, context);
    SetOperandAt(1, left);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  Handle<JSFunction> function_;
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
    SetChangesFlag(kNewSpacePromotion);
  }

  virtual bool IsDeletable() const V8_OVERRIDE {
    return !right()->representation().IsTagged();
  }
};


class HAdd V8_FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  // Add is only commutative if two integer values are added and not if two
  // tagged values are added (because it might be a String concatenation).
  // We also do not commute (pointer + offset).
  virtual bool IsCommutative() const V8_OVERRIDE {
    return !representation().IsTagged() && !representation().IsExternal();
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
    if (to.IsTagged()) {
      SetChangesFlag(kNewSpacePromotion);
      ClearFlag(kAllowUndefinedAsNaN);
    }
    if (to.IsTagged() &&
        (left()->ToNumberCanBeObserved() || right()->ToNumberCanBeObserved() ||
         left()->ToStringCanBeObserved() || right()->ToStringCanBeObserved())) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
  }

  virtual Representation RepresentationFromInputs() V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE;

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
    HInstruction* instr = HMul::New(zone, context, left, right);
    if (!instr->IsMul()) return instr;
    HMul* mul = HMul::cast(instr);
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

  bool MulMinusOne();

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
                           HValue* right);

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
       HValue* right) : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
  }
};


class HDiv V8_FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

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
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right) {
    return new(zone) HRor(context, left, right);
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

 private:
  HRor(HValue* context, HValue* left, HValue* right)
       : HBitwiseBinaryOperation(context, left, right) {
    ChangeRepresentation(Representation::Integer32());
  }
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
    SetChangesFlag(kOsrEntries);
    SetChangesFlag(kNewSpacePromotion);
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
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HCallStub, CodeStub::Major, int);
  CodeStub::Major major_key() { return major_key_; }

  HValue* context() { return value(); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CallStub)

 private:
  HCallStub(HValue* context, CodeStub::Major major_key, int argument_count)
      : HUnaryCall(context, argument_count),
        major_key_(major_key) {
  }

  CodeStub::Major major_key_;
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
  DECLARE_INSTRUCTION_FACTORY_P2(HLoadGlobalCell, Handle<Cell>,
                                 PropertyDetails);

  Unique<Cell> cell() const { return cell_; }
  bool RequiresHoleCheck() const;

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual intptr_t Hashcode() V8_OVERRIDE {
    return cell_.Hashcode();
  }

  virtual void FinalizeUniqueness() V8_OVERRIDE {
    cell_ = Unique<Cell>(cell_.handle());
  }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalCell)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    return cell_ == HLoadGlobalCell::cast(other)->cell_;
  }

 private:
  HLoadGlobalCell(Handle<Cell> cell, PropertyDetails details)
    : cell_(Unique<Cell>::CreateUninitialized(cell)), details_(details) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kGlobalVars);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return !RequiresHoleCheck(); }

  Unique<Cell> cell_;
  PropertyDetails details_;
};


class HLoadGlobalGeneric V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HLoadGlobalGeneric, HValue*,
                                              Handle<Object>, bool);

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

  Handle<Object> name_;
  bool for_typeof_;
};


class HAllocate V8_FINAL : public HTemplateInstruction<2> {
 public:
  static bool CompatibleInstanceTypes(InstanceType type1,
                                      InstanceType type2) {
    return ComputeFlags(TENURED, type1) == ComputeFlags(TENURED, type2) &&
        ComputeFlags(NOT_TENURED, type1) == ComputeFlags(NOT_TENURED, type2);
  }

  static HAllocate* New(Zone* zone,
                        HValue* context,
                        HValue* size,
                        HType type,
                        PretenureFlag pretenure_flag,
                        InstanceType instance_type,
                        Handle<AllocationSite> allocation_site =
                            Handle<AllocationSite>::null()) {
    return new(zone) HAllocate(context, size, type, pretenure_flag,
        instance_type, allocation_site);
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

  bool MustClearNextMapWord() const {
    return (flags_ & CLEAR_NEXT_MAP_WORD) != 0;
  }

  void MakeDoubleAligned() {
    flags_ = static_cast<HAllocate::Flags>(flags_ | ALLOCATE_DOUBLE_ALIGNED);
  }

  virtual bool HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) V8_OVERRIDE;

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(Allocate)

 private:
  enum Flags {
    ALLOCATE_IN_NEW_SPACE = 1 << 0,
    ALLOCATE_IN_OLD_DATA_SPACE = 1 << 1,
    ALLOCATE_IN_OLD_POINTER_SPACE = 1 << 2,
    ALLOCATE_DOUBLE_ALIGNED = 1 << 3,
    PREFILL_WITH_FILLER = 1 << 4,
    CLEAR_NEXT_MAP_WORD = 1 << 5
  };

  HAllocate(HValue* context,
            HValue* size,
            HType type,
            PretenureFlag pretenure_flag,
            InstanceType instance_type,
            Handle<AllocationSite> allocation_site =
                Handle<AllocationSite>::null())
      : HTemplateInstruction<2>(type),
        flags_(ComputeFlags(pretenure_flag, instance_type)),
        dominating_allocate_(NULL),
        filler_free_space_size_(NULL) {
    SetOperandAt(0, context);
    SetOperandAt(1, size);
    set_representation(Representation::Tagged());
    SetFlag(kTrackSideEffectDominators);
    SetChangesFlag(kNewSpacePromotion);
    SetDependsOnFlag(kNewSpacePromotion);

    if (FLAG_trace_pretenuring) {
      PrintF("HAllocate with AllocationSite %p %s\n",
             allocation_site.is_null()
                 ? static_cast<void*>(NULL)
                 : static_cast<void*>(*allocation_site),
             pretenure_flag == TENURED ? "tenured" : "not tenured");
    }
  }

  static Flags ComputeFlags(PretenureFlag pretenure_flag,
                            InstanceType instance_type) {
    Flags flags = pretenure_flag == TENURED
        ? (Heap::TargetSpaceId(instance_type) == OLD_POINTER_SPACE
            ? ALLOCATE_IN_OLD_POINTER_SPACE : ALLOCATE_IN_OLD_DATA_SPACE)
        : ALLOCATE_IN_NEW_SPACE;
    if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) {
      flags = static_cast<Flags>(flags | ALLOCATE_DOUBLE_ALIGNED);
    }
    // We have to fill the allocated object with one word fillers if we do
    // not use allocation folding since some allocations may depend on each
    // other, i.e., have a pointer to each other. A GC in between these
    // allocations may leave such objects behind in a not completely initialized
    // state.
    if (!FLAG_use_gvn || !FLAG_use_allocation_folding) {
      flags = static_cast<Flags>(flags | PREFILL_WITH_FILLER);
    }
    if (pretenure_flag == NOT_TENURED &&
        AllocationSite::CanTrack(instance_type)) {
      flags = static_cast<Flags>(flags | CLEAR_NEXT_MAP_WORD);
    }
    return flags;
  }

  void UpdateClearNextMapWord(bool clear_next_map_word) {
    flags_ = static_cast<Flags>(clear_next_map_word
                                ? flags_ | CLEAR_NEXT_MAP_WORD
                                : flags_ & ~CLEAR_NEXT_MAP_WORD);
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


class HInnerAllocatedObject V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HInnerAllocatedObject* New(Zone* zone,
                                    HValue* context,
                                    HValue* value,
                                    HValue* offset,
                                    HType type = HType::Tagged()) {
    return new(zone) HInnerAllocatedObject(value, offset, type);
  }

  HValue* base_object() { return OperandAt(0); }
  HValue* offset() { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return index == 0 ? Representation::Tagged() : Representation::Integer32();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(InnerAllocatedObject)

 private:
  HInnerAllocatedObject(HValue* value,
                        HValue* offset,
                        HType type = HType::Tagged())
      : HTemplateInstruction<2>(type) {
    ASSERT(value->IsAllocate());
    SetOperandAt(0, value);
    SetOperandAt(1, offset);
    set_type(type);
    set_representation(Representation::Tagged());
  }
};


inline bool StoringValueNeedsWriteBarrier(HValue* value) {
  return !value->type().IsBoolean()
      && !value->type().IsSmi()
      && !(value->IsConstant() && HConstant::cast(value)->ImmortalImmovable());
}


inline bool ReceiverObjectNeedsWriteBarrier(HValue* object,
                                            HValue* value,
                                            HValue* new_space_dominator) {
  while (object->IsInnerAllocatedObject()) {
    object = HInnerAllocatedObject::cast(object)->base_object();
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
    // Stores to new space allocations require no write barriers if the object
    // is the new space dominator.
    if (HAllocate::cast(object)->IsNewSpaceAllocation()) {
      return false;
    }
    // Likewise we don't need a write barrier if we store a value that
    // originates from the same allocation (via allocation folding).
    while (value->IsInnerAllocatedObject()) {
      value = HInnerAllocatedObject::cast(value)->base_object();
    }
    return object != value;
  }
  return true;
}


class HStoreGlobalCell V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HStoreGlobalCell, HValue*,
                                 Handle<PropertyCell>, PropertyDetails);

  Unique<PropertyCell> cell() const { return cell_; }
  bool RequiresHoleCheck() {
    return !details_.IsDontDelete() || details_.IsReadOnly();
  }
  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

  virtual void FinalizeUniqueness() V8_OVERRIDE {
    cell_ = Unique<PropertyCell>(cell_.handle());
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
        cell_(Unique<PropertyCell>::CreateUninitialized(cell)),
        details_(details) {
    SetChangesFlag(kGlobalVars);
  }

  Unique<PropertyCell> cell_;
  PropertyDetails details_;
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
    SetDependsOnFlag(kContextSlots);
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
    SetChangesFlag(kContextSlots);
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

  inline bool immutable() const {
    return ImmutableField::decode(value_);
  }

  // Returns true if access is being made to an in-object property that
  // was already added to the object.
  inline bool existing_inobject_property() const {
    return ExistingInobjectPropertyField::decode(value_);
  }

  inline HObjectAccess WithRepresentation(Representation representation) {
    return HObjectAccess(portion(), offset(), representation, name(),
                         immutable(), existing_inobject_property());
  }

  static HObjectAccess ForHeapNumberValue() {
    return HObjectAccess(
        kDouble, HeapNumber::kValueOffset, Representation::Double());
  }

  static HObjectAccess ForHeapNumberValueLowestBits() {
    return HObjectAccess(kDouble,
                         HeapNumber::kValueOffset,
                         Representation::Integer32());
  }

  static HObjectAccess ForHeapNumberValueHighestBits() {
    return HObjectAccess(kDouble,
                         HeapNumber::kValueOffset + kIntSize,
                         Representation::Integer32());
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

  static HObjectAccess ForAllocationSiteOffset(int offset);

  static HObjectAccess ForAllocationSiteList() {
    return HObjectAccess(kExternalMemory, 0, Representation::Tagged(),
                         Handle<String>::null(), false, false);
  }

  static HObjectAccess ForFixedArrayLength() {
    return HObjectAccess(
        kArrayLengths,
        FixedArray::kLengthOffset,
        FLAG_track_fields ? Representation::Smi() : Representation::Tagged());
  }

  static HObjectAccess ForStringHashField() {
    return HObjectAccess(kInobject,
                         String::kHashFieldOffset,
                         Representation::Integer32());
  }

  static HObjectAccess ForStringLength() {
    STATIC_ASSERT(String::kMaxLength <= Smi::kMaxValue);
    return HObjectAccess(
        kStringLengths,
        String::kLengthOffset,
        FLAG_track_fields ? Representation::Smi() : Representation::Tagged());
  }

  static HObjectAccess ForConsStringFirst() {
    return HObjectAccess(kInobject, ConsString::kFirstOffset);
  }

  static HObjectAccess ForConsStringSecond() {
    return HObjectAccess(kInobject, ConsString::kSecondOffset);
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

  static HObjectAccess ForFirstOsrAstIdSlot() {
    return HObjectAccess(kInobject, SharedFunctionInfo::kFirstOsrAstIdSlot);
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

  static HObjectAccess ForMapInstanceSize() {
    return HObjectAccess(kInobject,
                         Map::kInstanceSizeOffset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForMapInstanceType() {
    return HObjectAccess(kInobject,
                         Map::kInstanceTypeOffset,
                         Representation::UInteger8());
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
    return HObjectAccess(kExternalMemory, 0, Representation::Integer32(),
                         Handle<String>::null(), false, false);
  }

  // Create an access to an offset in a fixed array header.
  static HObjectAccess ForFixedArrayHeader(int offset);

  // Create an access to an in-object property in a JSObject.
  // This kind of access must be used when the object |map| is known and
  // in-object properties are being accessed. Accesses of the in-object
  // properties can have different semantics depending on whether corresponding
  // property was added to the map or not.
  static HObjectAccess ForMapAndOffset(Handle<Map> map, int offset,
      Representation representation = Representation::Tagged());

  // Create an access to an in-object property in a JSObject.
  // This kind of access can be used for accessing object header fields or
  // in-object properties if the map of the object is not known.
  static HObjectAccess ForObservableJSObjectOffset(int offset,
      Representation representation = Representation::Tagged()) {
    return ForMapAndOffset(Handle<Map>::null(), offset, representation);
  }

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

  static HObjectAccess ForJSTypedArrayLength() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSTypedArray::kLengthOffset);
  }

  static HObjectAccess ForJSArrayBufferBackingStore() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBuffer::kBackingStoreOffset, Representation::External());
  }

  static HObjectAccess ForExternalArrayExternalPointer() {
    return HObjectAccess::ForObservableJSObjectOffset(
        ExternalArray::kExternalPointerOffset, Representation::External());
  }

  static HObjectAccess ForJSArrayBufferViewWeakNext() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBufferView::kWeakNextOffset);
  }

  static HObjectAccess ForJSArrayBufferWeakFirstView() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBuffer::kWeakFirstViewOffset);
  }

  static HObjectAccess ForJSArrayBufferViewBuffer() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBufferView::kBufferOffset);
  }

  static HObjectAccess ForJSArrayBufferViewByteOffset() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBufferView::kByteOffsetOffset);
  }

  static HObjectAccess ForJSArrayBufferViewByteLength() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBufferView::kByteLengthOffset);
  }

  static HObjectAccess ForGlobalObjectNativeContext() {
    return HObjectAccess(kInobject, GlobalObject::kNativeContextOffset);
  }

  void PrintTo(StringStream* stream) const;

  inline bool Equals(HObjectAccess that) const {
    return value_ == that.value_;  // portion and offset must match
  }

 protected:
  void SetGVNFlags(HValue *instr, PropertyAccessType access_type);

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

  HObjectAccess() : value_(0) {}

  HObjectAccess(Portion portion, int offset,
                Representation representation = Representation::Tagged(),
                Handle<String> name = Handle<String>::null(),
                bool immutable = false,
                bool existing_inobject_property = true)
    : value_(PortionField::encode(portion) |
             RepresentationField::encode(representation.kind()) |
             ImmutableField::encode(immutable ? 1 : 0) |
             ExistingInobjectPropertyField::encode(
                 existing_inobject_property ? 1 : 0) |
             OffsetField::encode(offset)),
      name_(name) {
    // assert that the fields decode correctly
    ASSERT(this->offset() == offset);
    ASSERT(this->portion() == portion);
    ASSERT(this->immutable() == immutable);
    ASSERT(this->existing_inobject_property() == existing_inobject_property);
    ASSERT(RepresentationField::decode(value_) == representation.kind());
    ASSERT(!this->existing_inobject_property() || IsInobject());
  }

  class PortionField : public BitField<Portion, 0, 3> {};
  class RepresentationField : public BitField<Representation::Kind, 3, 4> {};
  class ImmutableField : public BitField<bool, 7, 1> {};
  class ExistingInobjectPropertyField : public BitField<bool, 8, 1> {};
  class OffsetField : public BitField<int, 9, 23> {};

  uint32_t value_;  // encodes portion, representation, immutable, and offset
  Handle<String> name_;

  friend class HLoadNamedField;
  friend class HStoreNamedField;
  friend class SideEffectsTracker;

  inline Portion portion() const {
    return PortionField::decode(value_);
  }
};


class HLoadNamedField V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HLoadNamedField, HValue*, HValue*,
                                 HObjectAccess);

  HValue* object() { return OperandAt(0); }
  HValue* dependency() {
    ASSERT(HasDependency());
    return OperandAt(1);
  }
  bool HasDependency() const { return OperandAt(0) != OperandAt(1); }
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
  HLoadNamedField(HValue* object,
                  HValue* dependency,
                  HObjectAccess access) : access_(access) {
    ASSERT(object != NULL);
    SetOperandAt(0, object);
    SetOperandAt(1, dependency != NULL ? dependency : object);

    Representation representation = access.representation();
    if (representation.IsInteger8() ||
        representation.IsUInteger8() ||
        representation.IsInteger16() ||
        representation.IsUInteger16()) {
      set_representation(Representation::Integer32());
    } else if (representation.IsSmi()) {
      set_type(HType::Smi());
      if (SmiValuesAre32Bits()) {
        set_representation(Representation::Integer32());
      } else {
        set_representation(representation);
      }
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
    access.SetGVNFlags(this, LOAD);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  HObjectAccess access_;
};


class HLoadNamedGeneric V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HLoadNamedGeneric, HValue*,
                                              Handle<Object>);

  HValue* context() { return OperandAt(0); }
  HValue* object() { return OperandAt(1); }
  Handle<Object> name() const { return name_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedGeneric)

 private:
  HLoadNamedGeneric(HValue* context, HValue* object, Handle<Object> name)
      : name_(name) {
    SetOperandAt(0, context);
    SetOperandAt(1, object);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  Handle<Object> name_;
};


class HLoadFunctionPrototype V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HLoadFunctionPrototype, HValue*);

  HValue* function() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE { return true; }

 private:
  explicit HLoadFunctionPrototype(HValue* function)
      : HUnaryOperation(function) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kCalls);
  }
};

class ArrayInstructionInterface {
 public:
  virtual HValue* GetKey() = 0;
  virtual void SetKey(HValue* key) = 0;
  virtual void SetIndexOffset(uint32_t index_offset) = 0;
  virtual int MaxIndexOffsetBits() = 0;
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
  bool is_fixed_typed_array() const {
    return IsFixedTypedArrayElementsKind(elements_kind());
  }
  bool is_typed_elements() const {
    return is_external() || is_fixed_typed_array();
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
  virtual int MaxIndexOffsetBits() {
    return kBitsForIndexOffset;
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
    // kind_fast:                 tagged[int32] (none)
    // kind_double:               tagged[int32] (none)
    // kind_fixed_typed_array:    tagged[int32] (none)
    // kind_external:             external[int32] (none)
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

    if (!is_typed_elements()) {
      // I can detect the case between storing double (holey and fast) and
      // smi/object by looking at elements_kind_.
      ASSERT(IsFastSmiOrObjectElementsKind(elements_kind) ||
             IsFastDoubleElementsKind(elements_kind));

      if (IsFastSmiOrObjectElementsKind(elements_kind)) {
        if (IsFastSmiElementsKind(elements_kind) &&
            (!IsHoleyElementsKind(elements_kind) ||
             mode == NEVER_RETURN_HOLE)) {
          set_type(HType::Smi());
          if (SmiValuesAre32Bits() && !RequiresHoleCheck()) {
            set_representation(Representation::Integer32());
          } else {
            set_representation(Representation::Smi());
          }
        } else {
          set_representation(Representation::Tagged());
        }

        SetDependsOnFlag(kArrayElements);
      } else {
        set_representation(Representation::Double());
        SetDependsOnFlag(kDoubleArrayElements);
      }
    } else {
      if (elements_kind == EXTERNAL_FLOAT32_ELEMENTS ||
          elements_kind == EXTERNAL_FLOAT64_ELEMENTS ||
          elements_kind == FLOAT32_ELEMENTS ||
          elements_kind == FLOAT64_ELEMENTS) {
        set_representation(Representation::Double());
      } else {
        set_representation(Representation::Integer32());
      }

      if (is_external()) {
        SetDependsOnFlag(kExternalMemory);
      } else if (is_fixed_typed_array()) {
        SetDependsOnFlag(kTypedArrayElements);
      } else {
        UNREACHABLE();
      }
      // Native code could change the specialized array.
      SetDependsOnFlag(kCalls);
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
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HLoadKeyedGeneric, HValue*,
                                              HValue*);
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

 private:
  HLoadKeyedGeneric(HValue* context, HValue* obj, HValue* key) {
    set_representation(Representation::Tagged());
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, context);
    SetAllSideEffects();
  }
};


// Indicates whether the store is a store to an entry that was previously
// initialized or not.
enum StoreFieldOrKeyedMode {
  // The entry could be either previously initialized or not.
  INITIALIZING_STORE,
  // At the time of this store it is guaranteed that the entry is already
  // initialized.
  STORE_TO_INITIALIZED_ENTRY
};


class HStoreNamedField V8_FINAL : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HStoreNamedField, HValue*,
                                 HObjectAccess, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P4(HStoreNamedField, HValue*,
                                 HObjectAccess, HValue*, StoreFieldOrKeyedMode);

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
    } else if (index == 1) {
      if (field_representation().IsInteger8() ||
          field_representation().IsUInteger8() ||
          field_representation().IsInteger16() ||
          field_representation().IsUInteger16() ||
          field_representation().IsInteger32()) {
        return Representation::Integer32();
      } else if (field_representation().IsDouble()) {
        return field_representation();
      } else if (field_representation().IsSmi()) {
        if (SmiValuesAre32Bits() && store_mode_ == STORE_TO_INITIALIZED_ENTRY) {
          return Representation::Integer32();
        }
        return field_representation();
      } else if (field_representation().IsExternal()) {
        return Representation::External();
      }
    }
    return Representation::Tagged();
  }
  virtual bool HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) V8_OVERRIDE {
    ASSERT(side_effect == kNewSpacePromotion);
    if (!FLAG_use_write_barrier_elimination) return false;
    new_space_dominator_ = dominator;
    return false;
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
  StoreFieldOrKeyedMode store_mode() const { return store_mode_; }

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
    is_stable_ = map->is_stable();

    if (is_stable_) {
      map->AddDependentCompilationInfo(
          DependentCode::kPrototypeCheckGroup, info);
    }
  }

  bool is_stable() const {
    return is_stable_;
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
        ReceiverObjectNeedsWriteBarrier(object(), value(),
                                        new_space_dominator());
  }

  bool NeedsWriteBarrierForMap() {
    if (IsSkipWriteBarrier()) return false;
    return ReceiverObjectNeedsWriteBarrier(object(), transition(),
                                           new_space_dominator());
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
                   HValue* val,
                   StoreFieldOrKeyedMode store_mode = INITIALIZING_STORE)
      : access_(access),
        new_space_dominator_(NULL),
        write_barrier_mode_(UPDATE_WRITE_BARRIER),
        has_transition_(false),
        is_stable_(false),
        store_mode_(store_mode) {
    // Stores to a non existing in-object property are allowed only to the
    // newly allocated objects (via HAllocate or HInnerAllocatedObject).
    ASSERT(!access.IsInobject() || access.existing_inobject_property() ||
           obj->IsAllocate() || obj->IsInnerAllocatedObject());
    SetOperandAt(0, obj);
    SetOperandAt(1, val);
    SetOperandAt(2, obj);
    access.SetGVNFlags(this, STORE);
  }

  HObjectAccess access_;
  HValue* new_space_dominator_;
  WriteBarrierMode write_barrier_mode_ : 1;
  bool has_transition_ : 1;
  bool is_stable_ : 1;
  StoreFieldOrKeyedMode store_mode_ : 1;
};


class HStoreNamedGeneric V8_FINAL : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(HStoreNamedGeneric, HValue*,
                                              Handle<String>, HValue*,
                                              StrictModeFlag);
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

  Handle<String> name_;
  StrictModeFlag strict_mode_flag_;
};


class HStoreKeyed V8_FINAL
    : public HTemplateInstruction<3>, public ArrayInstructionInterface {
 public:
  DECLARE_INSTRUCTION_FACTORY_P4(HStoreKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind);
  DECLARE_INSTRUCTION_FACTORY_P5(HStoreKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind, StoreFieldOrKeyedMode);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    // kind_fast:               tagged[int32] = tagged
    // kind_double:             tagged[int32] = double
    // kind_smi   :             tagged[int32] = smi
    // kind_fixed_typed_array:  tagged[int32] = (double | int32)
    // kind_external:           external[int32] = (double | int32)
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
    if (SmiValuesAre32Bits() && store_mode_ == STORE_TO_INITIALIZED_ENTRY) {
      return Representation::Integer32();
    }
    if (IsFastSmiElementsKind(elements_kind())) {
      return Representation::Smi();
    }

    return is_external() || is_fixed_typed_array()
        ? Representation::Integer32()
        : Representation::Tagged();
  }

  bool is_external() const {
    return IsExternalArrayElementsKind(elements_kind());
  }

  bool is_fixed_typed_array() const {
    return IsFixedTypedArrayElementsKind(elements_kind());
  }

  bool is_typed_elements() const {
    return is_external() || is_fixed_typed_array();
  }

  virtual Representation observed_input_representation(int index) V8_OVERRIDE {
    if (index < 2) return RequiredInputRepresentation(index);
    if (IsUninitialized()) {
      return Representation::None();
    }
    if (IsDoubleOrFloatElementsKind(elements_kind())) {
      return Representation::Double();
    }
    if (SmiValuesAre32Bits() && store_mode_ == STORE_TO_INITIALIZED_ENTRY) {
      return Representation::Integer32();
    }
    if (IsFastSmiElementsKind(elements_kind())) {
      return Representation::Smi();
    }
    if (is_typed_elements()) {
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
  StoreFieldOrKeyedMode store_mode() const { return store_mode_; }
  ElementsKind elements_kind() const { return elements_kind_; }
  uint32_t index_offset() { return index_offset_; }
  void SetIndexOffset(uint32_t index_offset) { index_offset_ = index_offset; }
  virtual int MaxIndexOffsetBits() {
    return 31 - ElementsKindToShiftSize(elements_kind_);
  }
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

  virtual bool HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) V8_OVERRIDE {
    ASSERT(side_effect == kNewSpacePromotion);
    new_space_dominator_ = dominator;
    return false;
  }

  HValue* new_space_dominator() const { return new_space_dominator_; }

  bool NeedsWriteBarrier() {
    if (value_is_smi()) {
      return false;
    } else {
      return StoringValueNeedsWriteBarrier(value()) &&
          ReceiverObjectNeedsWriteBarrier(elements(), value(),
                                          new_space_dominator());
    }
  }

  bool NeedsCanonicalization();

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyed)

 private:
  HStoreKeyed(HValue* obj, HValue* key, HValue* val,
              ElementsKind elements_kind,
              StoreFieldOrKeyedMode store_mode = INITIALIZING_STORE)
      : elements_kind_(elements_kind),
      index_offset_(0),
      is_dehoisted_(false),
      is_uninitialized_(false),
      store_mode_(store_mode),
      new_space_dominator_(NULL) {
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, val);

    ASSERT(store_mode != STORE_TO_INITIALIZED_ENTRY ||
           elements_kind == FAST_SMI_ELEMENTS);

    if (IsFastObjectElementsKind(elements_kind)) {
      SetFlag(kTrackSideEffectDominators);
      SetDependsOnFlag(kNewSpacePromotion);
    }
    if (is_external()) {
      SetChangesFlag(kExternalMemory);
      SetFlag(kAllowUndefinedAsNaN);
    } else if (IsFastDoubleElementsKind(elements_kind)) {
      SetChangesFlag(kDoubleArrayElements);
    } else if (IsFastSmiElementsKind(elements_kind)) {
      SetChangesFlag(kArrayElements);
    } else if (is_fixed_typed_array()) {
      SetChangesFlag(kTypedArrayElements);
      SetFlag(kAllowUndefinedAsNaN);
    } else {
      SetChangesFlag(kArrayElements);
    }

    // EXTERNAL_{UNSIGNED_,}{BYTE,SHORT,INT}_ELEMENTS are truncating.
    if ((elements_kind >= EXTERNAL_INT8_ELEMENTS &&
        elements_kind <= EXTERNAL_UINT32_ELEMENTS) ||
        (elements_kind >= UINT8_ELEMENTS &&
        elements_kind <= INT32_ELEMENTS)) {
      SetFlag(kTruncatingToInt32);
    }
  }

  ElementsKind elements_kind_;
  uint32_t index_offset_;
  bool is_dehoisted_ : 1;
  bool is_uninitialized_ : 1;
  StoreFieldOrKeyedMode store_mode_: 1;
  HValue* new_space_dominator_;
};


class HStoreKeyedGeneric V8_FINAL : public HTemplateInstruction<4> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(HStoreKeyedGeneric, HValue*,
                                              HValue*, HValue*, StrictModeFlag);

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
  Unique<Map> original_map() { return original_map_; }
  Unique<Map> transitioned_map() { return transitioned_map_; }
  ElementsKind from_kind() { return from_kind_; }
  ElementsKind to_kind() { return to_kind_; }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(TransitionElementsKind)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    HTransitionElementsKind* instr = HTransitionElementsKind::cast(other);
    return original_map_ == instr->original_map_ &&
           transitioned_map_ == instr->transitioned_map_;
  }

 private:
  HTransitionElementsKind(HValue* context,
                          HValue* object,
                          Handle<Map> original_map,
                          Handle<Map> transitioned_map)
      : original_map_(Unique<Map>(original_map)),
        transitioned_map_(Unique<Map>(transitioned_map)),
        from_kind_(original_map->elements_kind()),
        to_kind_(transitioned_map->elements_kind()) {
    SetOperandAt(0, object);
    SetOperandAt(1, context);
    SetFlag(kUseGVN);
    SetChangesFlag(kElementsKind);
    if (!IsSimpleMapChangeTransition(from_kind_, to_kind_)) {
      SetChangesFlag(kElementsPointer);
      SetChangesFlag(kNewSpacePromotion);
    }
    set_representation(Representation::Tagged());
  }

  Unique<Map> original_map_;
  Unique<Map> transitioned_map_;
  ElementsKind from_kind_;
  ElementsKind to_kind_;
};


class HStringAdd V8_FINAL : public HBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right,
                           PretenureFlag pretenure_flag = NOT_TENURED,
                           StringAddFlags flags = STRING_ADD_CHECK_BOTH,
                           Handle<AllocationSite> allocation_site =
                               Handle<AllocationSite>::null());

  StringAddFlags flags() const { return flags_; }
  PretenureFlag pretenure_flag() const { return pretenure_flag_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(StringAdd)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    return flags_ == HStringAdd::cast(other)->flags_ &&
        pretenure_flag_ == HStringAdd::cast(other)->pretenure_flag_;
  }

 private:
  HStringAdd(HValue* context,
             HValue* left,
             HValue* right,
             PretenureFlag pretenure_flag,
             StringAddFlags flags,
             Handle<AllocationSite> allocation_site)
      : HBinaryOperation(context, left, right, HType::String()),
        flags_(flags), pretenure_flag_(pretenure_flag) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kMaps);
    SetChangesFlag(kNewSpacePromotion);
    if (FLAG_trace_pretenuring) {
      PrintF("HStringAdd with AllocationSite %p %s\n",
             allocation_site.is_null()
                 ? static_cast<void*>(NULL)
                 : static_cast<void*>(*allocation_site),
             pretenure_flag == TENURED ? "tenured" : "not tenured");
    }
  }

  // No side-effects except possible allocation:
  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  const StringAddFlags flags_;
  const PretenureFlag pretenure_flag_;
};


class HStringCharCodeAt V8_FINAL : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HStringCharCodeAt,
                                              HValue*,
                                              HValue*);

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
    SetDependsOnFlag(kMaps);
    SetDependsOnFlag(kStringChars);
    SetChangesFlag(kNewSpacePromotion);
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
    SetChangesFlag(kNewSpacePromotion);
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
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(HRegExpLiteral,
                                              Handle<FixedArray>,
                                              Handle<String>,
                                              Handle<String>,
                                              int);

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> literals() { return literals_; }
  Handle<String> pattern() { return pattern_; }
  Handle<String> flags() { return flags_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(RegExpLiteral)

 private:
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

  Handle<FixedArray> literals_;
  Handle<String> pattern_;
  Handle<String> flags_;
};


class HFunctionLiteral V8_FINAL : public HTemplateInstruction<1> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HFunctionLiteral,
                                              Handle<SharedFunctionInfo>,
                                              bool);
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
    SetChangesFlag(kNewSpacePromotion);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  Handle<SharedFunctionInfo> shared_info_;
  bool pretenure_ : 1;
  bool has_no_literals_ : 1;
  bool is_generator_ : 1;
  LanguageMode language_mode_;
};


class HTypeof V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HTypeof, HValue*);

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Typeof)

 private:
  explicit HTypeof(HValue* context, HValue* value) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    set_representation(Representation::Tagged());
  }

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
    SetChangesFlag(kNewSpacePromotion);

    // This instruction is not marked as kChangesMaps, but does
    // change the map of the input operand. Use it only when creating
    // object literals via a runtime call.
    ASSERT(value->IsCallRuntime());
#ifdef DEBUG
    const Runtime::Function* function = HCallRuntime::cast(value)->function();
    ASSERT(function->function_id == Runtime::kCreateObjectLiteral);
#endif
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }
};


class HDateField V8_FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HDateField, HValue*, Smi*);

  Smi* index() const { return index_; }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(DateField)

 private:
  HDateField(HValue* date, Smi* index)
      : HUnaryOperation(date), index_(index) {
    set_representation(Representation::Tagged());
  }

  Smi* index_;
};


class HSeqStringGetChar V8_FINAL : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           String::Encoding encoding,
                           HValue* string,
                           HValue* index);

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return (index == 0) ? Representation::Tagged()
                        : Representation::Integer32();
  }

  String::Encoding encoding() const { return encoding_; }
  HValue* string() const { return OperandAt(0); }
  HValue* index() const { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(SeqStringGetChar)

 protected:
  virtual bool DataEquals(HValue* other) V8_OVERRIDE {
    return encoding() == HSeqStringGetChar::cast(other)->encoding();
  }

  virtual Range* InferRange(Zone* zone) V8_OVERRIDE {
    if (encoding() == String::ONE_BYTE_ENCODING) {
      return new(zone) Range(0, String::kMaxOneByteCharCode);
    } else {
      ASSERT_EQ(String::TWO_BYTE_ENCODING, encoding());
      return  new(zone) Range(0, String::kMaxUtf16CodeUnit);
    }
  }

 private:
  HSeqStringGetChar(String::Encoding encoding,
                    HValue* string,
                    HValue* index) : encoding_(encoding) {
    SetOperandAt(0, string);
    SetOperandAt(1, index);
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kStringChars);
  }

  virtual bool IsDeletable() const V8_OVERRIDE { return true; }

  String::Encoding encoding_;
};


class HSeqStringSetChar V8_FINAL : public HTemplateInstruction<4> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(
      HSeqStringSetChar, String::Encoding,
      HValue*, HValue*, HValue*);

  String::Encoding encoding() { return encoding_; }
  HValue* context() { return OperandAt(0); }
  HValue* string() { return OperandAt(1); }
  HValue* index() { return OperandAt(2); }
  HValue* value() { return OperandAt(3); }

  virtual Representation RequiredInputRepresentation(int index) V8_OVERRIDE {
    return (index <= 1) ? Representation::Tagged()
                        : Representation::Integer32();
  }

  DECLARE_CONCRETE_INSTRUCTION(SeqStringSetChar)

 private:
  HSeqStringSetChar(HValue* context,
                    String::Encoding encoding,
                    HValue* string,
                    HValue* index,
                    HValue* value) : encoding_(encoding) {
    SetOperandAt(0, context);
    SetOperandAt(1, string);
    SetOperandAt(2, index);
    SetOperandAt(3, value);
    set_representation(Representation::Tagged());
    SetChangesFlag(kStringChars);
  }

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
  virtual int RedefinedOperandIndex() { return 0; }

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
    SetDependsOnFlag(kMaps);
    SetDependsOnFlag(kElementsKind);
  }
};


class HForInPrepareMap V8_FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HForInPrepareMap, HValue*);

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
