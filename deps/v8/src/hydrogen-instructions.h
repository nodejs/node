// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_INSTRUCTIONS_H_
#define V8_HYDROGEN_INSTRUCTIONS_H_

#include "src/v8.h"

#include "src/allocation.h"
#include "src/base/bits.h"
#include "src/code-stubs.h"
#include "src/conversions.h"
#include "src/data-flow.h"
#include "src/deoptimizer.h"
#include "src/feedback-slots.h"
#include "src/hydrogen-types.h"
#include "src/small-pointer-list.h"
#include "src/unique.h"
#include "src/utils.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
struct ChangesOf;
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
class OStream;

#define HYDROGEN_ABSTRACT_INSTRUCTION_LIST(V) \
  V(ArithmeticBinaryOperation)                \
  V(BinaryOperation)                          \
  V(BitwiseBinaryOperation)                   \
  V(ControlInstruction)                       \
  V(Instruction)


#define HYDROGEN_CONCRETE_INSTRUCTION_LIST(V) \
  V(AbnormalExit)                             \
  V(AccessArgumentsAt)                        \
  V(Add)                                      \
  V(AllocateBlockContext)                     \
  V(Allocate)                                 \
  V(ApplyArguments)                           \
  V(ArgumentsElements)                        \
  V(ArgumentsLength)                          \
  V(ArgumentsObject)                          \
  V(Bitwise)                                  \
  V(BlockEntry)                               \
  V(BoundsCheck)                              \
  V(BoundsCheckBaseIndexInformation)          \
  V(Branch)                                   \
  V(CallWithDescriptor)                       \
  V(CallJSFunction)                           \
  V(CallFunction)                             \
  V(CallNew)                                  \
  V(CallNewArray)                             \
  V(CallRuntime)                              \
  V(CallStub)                                 \
  V(CapturedObject)                           \
  V(Change)                                   \
  V(CheckHeapObject)                          \
  V(CheckInstanceType)                        \
  V(CheckMaps)                                \
  V(CheckMapValue)                            \
  V(CheckSmi)                                 \
  V(CheckValue)                               \
  V(ClampToUint8)                             \
  V(ClassOfTestAndBranch)                     \
  V(CompareNumericAndBranch)                  \
  V(CompareHoleAndBranch)                     \
  V(CompareGeneric)                           \
  V(CompareMinusZeroAndBranch)                \
  V(CompareObjectEqAndBranch)                 \
  V(CompareMap)                               \
  V(Constant)                                 \
  V(ConstructDouble)                          \
  V(Context)                                  \
  V(DateField)                                \
  V(DebugBreak)                               \
  V(DeclareGlobals)                           \
  V(Deoptimize)                               \
  V(Div)                                      \
  V(DoubleBits)                               \
  V(DummyUse)                                 \
  V(EnterInlined)                             \
  V(EnvironmentMarker)                        \
  V(ForceRepresentation)                      \
  V(ForInCacheArray)                          \
  V(ForInPrepareMap)                          \
  V(FunctionLiteral)                          \
  V(GetCachedArrayIndex)                      \
  V(Goto)                                     \
  V(HasCachedArrayIndexAndBranch)             \
  V(HasInstanceTypeAndBranch)                 \
  V(InnerAllocatedObject)                     \
  V(InstanceOf)                               \
  V(InstanceOfKnownGlobal)                    \
  V(InvokeFunction)                           \
  V(IsConstructCallAndBranch)                 \
  V(IsObjectAndBranch)                        \
  V(IsStringAndBranch)                        \
  V(IsSmiAndBranch)                           \
  V(IsUndetectableAndBranch)                  \
  V(LeaveInlined)                             \
  V(LoadContextSlot)                          \
  V(LoadFieldByIndex)                         \
  V(LoadFunctionPrototype)                    \
  V(LoadGlobalCell)                           \
  V(LoadGlobalGeneric)                        \
  V(LoadKeyed)                                \
  V(LoadKeyedGeneric)                         \
  V(LoadNamedField)                           \
  V(LoadNamedGeneric)                         \
  V(LoadRoot)                                 \
  V(MapEnumLength)                            \
  V(MathFloorOfDiv)                           \
  V(MathMinMax)                               \
  V(Mod)                                      \
  V(Mul)                                      \
  V(OsrEntry)                                 \
  V(Parameter)                                \
  V(Power)                                    \
  V(PushArguments)                            \
  V(RegExpLiteral)                            \
  V(Return)                                   \
  V(Ror)                                      \
  V(Sar)                                      \
  V(SeqStringGetChar)                         \
  V(SeqStringSetChar)                         \
  V(Shl)                                      \
  V(Shr)                                      \
  V(Simulate)                                 \
  V(StackCheck)                               \
  V(StoreCodeEntry)                           \
  V(StoreContextSlot)                         \
  V(StoreFrameContext)                        \
  V(StoreGlobalCell)                          \
  V(StoreKeyed)                               \
  V(StoreKeyedGeneric)                        \
  V(StoreNamedField)                          \
  V(StoreNamedGeneric)                        \
  V(StringAdd)                                \
  V(StringCharCodeAt)                         \
  V(StringCharFromCode)                       \
  V(StringCompareAndBranch)                   \
  V(Sub)                                      \
  V(TailCallThroughMegamorphicCache)          \
  V(ThisFunction)                             \
  V(ToFastProperties)                         \
  V(TransitionElementsKind)                   \
  V(TrapAllocationMemento)                    \
  V(Typeof)                                   \
  V(TypeofIsAndBranch)                        \
  V(UnaryMathOperation)                       \
  V(UnknownOSRValue)                          \
  V(UseConst)                                 \
  V(WrapReceiver)

#define GVN_TRACKED_FLAG_LIST(V)               \
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
  V(Maps)                                      \
  V(OsrEntries)                                \
  V(ExternalMemory)                            \
  V(StringChars)                               \
  V(TypedArrayElements)


#define DECLARE_ABSTRACT_INSTRUCTION(type)                              \
  virtual bool Is##type() const FINAL OVERRIDE { return true; }   \
  static H##type* cast(HValue* value) {                                 \
    DCHECK(value->Is##type());                                          \
    return reinterpret_cast<H##type*>(value);                           \
  }


#define DECLARE_CONCRETE_INSTRUCTION(type)              \
  virtual LInstruction* CompileToLithium(               \
     LChunkBuilder* builder) FINAL OVERRIDE;      \
  static H##type* cast(HValue* value) {                 \
    DCHECK(value->Is##type());                          \
    return reinterpret_cast<H##type*>(value);           \
  }                                                     \
  virtual Opcode opcode() const FINAL OVERRIDE {  \
    return HValue::k##type;                             \
  }


enum PropertyAccessType { LOAD, STORE };


class Range FINAL : public ZoneObject {
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
class HUseIterator FINAL BASE_EMBEDDED {
 public:
  bool Done() { return current_ == NULL; }
  void Advance();

  HValue* value() {
    DCHECK(!Done());
    return value_;
  }

  int index() {
    DCHECK(!Done());
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
  DCHECK(i >= 0);
  DCHECK(i < kNumberOfFlags);
  return static_cast<GVNFlag>(i);
}


class DecompositionResult FINAL BASE_EMBEDDED {
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

 private:
  typedef BitField<int, 0, 9> InliningIdField;

  // Offset from the start of the inlined function.
  typedef BitField<int, 9, 23> PositionField;

  explicit HSourcePosition(int value) : value_(value) { }

  friend class HPositionInfo;
  friend class LCodeGenBase;

  // If FLAG_hydrogen_track_positions is set contains bitfields InliningIdField
  // and PositionField.
  // Otherwise contains absolute offset from the script start.
  int value_;
};


OStream& operator<<(OStream& os, const HSourcePosition& p);


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
    kLeftCanBeMinInt,
    kLeftCanBeNegative,
    kLeftCanBePositive,
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
    // Indicates an instruction shouldn't be replaced by optimization, this flag
    // is useful to set in cases where recomputing a value is cheaper than
    // extending the value's live range and spilling it.
    kCantBeReplaced,
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

  bool IsBitwiseBinaryShift() {
    return IsShl() || IsShr() || IsSar();
  }

  explicit HValue(HType type = HType::Tagged())
      : block_(NULL),
        id_(kNoNumber),
        type_(type),
        use_list_(NULL),
        range_(NULL),
#ifdef DEBUG
        range_poisoned_(false),
#endif
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
    DCHECK(CheckFlag(kFlexibleRepresentation));
    DCHECK(!CheckFlag(kCannotBeTagged) || !r.IsTagged());
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
    DCHECK(new_type.IsSubtypeOf(type_));
    type_ = new_type;
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
  virtual int OperandCount() const = 0;
  virtual HValue* OperandAt(int index) const = 0;
  void SetOperandAt(int index, HValue* value);

  void DeleteAndReplaceWith(HValue* other);
  void ReplaceAllUsesWith(HValue* other);
  bool HasNoUses() const { return use_list_ == NULL; }
  bool HasOneUse() const {
    return use_list_ != NULL && use_list_->tail() == NULL;
  }
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

  Range* range() const {
    DCHECK(!range_poisoned_);
    return range_;
  }
  bool HasRange() const {
    DCHECK(!range_poisoned_);
    return range_ != NULL;
  }
#ifdef DEBUG
  void PoisonRange() { range_poisoned_ = true; }
#endif
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
  virtual OStream& PrintTo(OStream& os) const = 0;  // NOLINT

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
    return ToStringOrToNumberCanBeObserved();
  }

  // Returns true conservatively if the program might be able to observe a
  // ToNumber() operation on this value.
  bool ToNumberCanBeObserved() const {
    return ToStringOrToNumberCanBeObserved();
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

  bool ToStringOrToNumberCanBeObserved() const {
    if (type().IsTaggedPrimitive()) return false;
    if (type().IsJSObject()) return true;
    return !representation().IsSmiOrInteger32() && !representation().IsDouble();
  }

  virtual Representation RepresentationFromInputs() {
    return representation();
  }
  virtual Representation RepresentationFromUses();
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
    DCHECK(block_ != NULL);
    block_ = NULL;
  }

  void set_representation(Representation r) {
    DCHECK(representation_.IsNone() && !r.IsNone());
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
  friend OStream& operator<<(OStream& os, const ChangesOf& v);

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
#ifdef DEBUG
  bool range_poisoned_;
#endif
  int flags_;
  GVNFlagSet changes_flags_;
  GVNFlagSet depends_on_flags_;

 private:
  virtual bool IsDeletable() const { return false; }

  DISALLOW_COPY_AND_ASSIGN(HValue);
};

// Support for printing various aspects of an HValue.
struct NameOf {
  explicit NameOf(const HValue* const v) : value(v) {}
  const HValue* value;
};


struct TypeOf {
  explicit TypeOf(const HValue* const v) : value(v) {}
  const HValue* value;
};


struct ChangesOf {
  explicit ChangesOf(const HValue* const v) : value(v) {}
  const HValue* value;
};


OStream& operator<<(OStream& os, const HValue& v);
OStream& operator<<(OStream& os, const NameOf& v);
OStream& operator<<(OStream& os, const TypeOf& v);
OStream& operator<<(OStream& os, const ChangesOf& v);


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

#define DECLARE_INSTRUCTION_FACTORY_P6(I, P1, P2, P3, P4, P5, P6)              \
  static I* New(Zone* zone,                                                    \
                HValue* context,                                               \
                P1 p1,                                                         \
                P2 p2,                                                         \
                P3 p3,                                                         \
                P4 p4,                                                         \
                P5 p5,                                                         \
                P6 p6) {                                                       \
    return new(zone) I(p1, p2, p3, p4, p5, p6);                                \
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

    DCHECK(has_operand_positions());
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
    DCHECK(has_operand_positions());
    return &(operand_positions()[kFirstOperandPosIndex + idx]);
  }

  bool has_operand_positions() const {
    return !IsTaggedPosition(data_);
  }

  HSourcePosition* operand_positions() const {
    DCHECK(has_operand_positions());
    return reinterpret_cast<HSourcePosition*>(data_);
  }

  static const intptr_t kPositionTag = 1;
  static const intptr_t kPositionShift = 1;
  static bool IsTaggedPosition(intptr_t val) {
    return (val & kPositionTag) != 0;
  }
  static intptr_t UntagPosition(intptr_t val) {
    DCHECK(IsTaggedPosition(val));
    return val >> kPositionShift;
  }
  static intptr_t TagPosition(intptr_t val) {
    const intptr_t result = (val << kPositionShift) | kPositionTag;
    DCHECK(UntagPosition(result) == val);
    return result;
  }

  intptr_t data_;
};


class HInstruction : public HValue {
 public:
  HInstruction* next() const { return next_; }
  HInstruction* previous() const { return previous_; }

  virtual OStream& PrintTo(OStream& os) const OVERRIDE;  // NOLINT
  virtual OStream& PrintDataTo(OStream& os) const;          // NOLINT

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
  virtual HSourcePosition position() const OVERRIDE {
    return HSourcePosition(position_.position());
  }
  bool has_position() const {
    return !position().IsUnknown();
  }
  void set_position(HSourcePosition position) {
    DCHECK(!has_position());
    DCHECK(!position.IsUnknown());
    position_.set_position(position);
  }

  virtual HSourcePosition operand_position(int index) const OVERRIDE {
    const HSourcePosition pos = position_.operand_position(index);
    return pos.IsUnknown() ? position() : pos;
  }
  void set_operand_position(Zone* zone, int index, HSourcePosition pos) {
    DCHECK(0 <= index && index < OperandCount());
    position_.ensure_storage_for_operand_positions(zone, OperandCount());
    position_.set_operand_position(index, pos);
  }

  bool Dominates(HInstruction* other);
  bool CanTruncateToSmi() const { return CheckFlag(kTruncatingToSmi); }
  bool CanTruncateToInt32() const { return CheckFlag(kTruncatingToInt32); }

  virtual LInstruction* CompileToLithium(LChunkBuilder* builder) = 0;

#ifdef DEBUG
  virtual void Verify() OVERRIDE;
#endif

  bool CanDeoptimize();

  virtual bool HasStackCheck() { return false; }

  DECLARE_ABSTRACT_INSTRUCTION(Instruction)

 protected:
  explicit HInstruction(HType type = HType::Tagged())
      : HValue(type),
        next_(NULL),
        previous_(NULL),
        position_(RelocInfo::kNoPosition) {
    SetDependsOnFlag(kOsrEntries);
  }

  virtual void DeleteFromGraph() OVERRIDE { Unlink(); }

 private:
  void InitializeAsFirst(HBasicBlock* block) {
    DCHECK(!IsLinked());
    SetBlock(block);
  }

  HInstruction* next_;
  HInstruction* previous_;
  HPositionInfo position_;

  friend class HBasicBlock;
};


template<int V>
class HTemplateInstruction : public HInstruction {
 public:
  virtual int OperandCount() const FINAL OVERRIDE { return V; }
  virtual HValue* OperandAt(int i) const FINAL OVERRIDE {
    return inputs_[i];
  }

 protected:
  explicit HTemplateInstruction(HType type = HType::Tagged())
      : HInstruction(type) {}

  virtual void InternalSetOperandAt(int i, HValue* value) FINAL OVERRIDE {
    inputs_[i] = value;
  }

 private:
  EmbeddedContainer<HValue*, V> inputs_;
};


class HControlInstruction : public HInstruction {
 public:
  virtual HBasicBlock* SuccessorAt(int i) const = 0;
  virtual int SuccessorCount() const = 0;
  virtual void SetSuccessorAt(int i, HBasicBlock* block) = 0;

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

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


class HSuccessorIterator FINAL BASE_EMBEDDED {
 public:
  explicit HSuccessorIterator(const HControlInstruction* instr)
      : instr_(instr), current_(0) {}

  bool Done() { return current_ >= instr_->SuccessorCount(); }
  HBasicBlock* Current() { return instr_->SuccessorAt(current_); }
  void Advance() { current_++; }

 private:
  const HControlInstruction* instr_;
  int current_;
};


template<int S, int V>
class HTemplateControlInstruction : public HControlInstruction {
 public:
  int SuccessorCount() const OVERRIDE { return S; }
  HBasicBlock* SuccessorAt(int i) const OVERRIDE { return successors_[i]; }
  void SetSuccessorAt(int i, HBasicBlock* block) OVERRIDE {
    successors_[i] = block;
  }

  int OperandCount() const OVERRIDE { return V; }
  HValue* OperandAt(int i) const OVERRIDE { return inputs_[i]; }


 protected:
  void InternalSetOperandAt(int i, HValue* value) OVERRIDE {
    inputs_[i] = value;
  }

 private:
  EmbeddedContainer<HBasicBlock*, S> successors_;
  EmbeddedContainer<HValue*, V> inputs_;
};


class HBlockEntry FINAL : public HTemplateInstruction<0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(BlockEntry)
};


class HDummyUse FINAL : public HTemplateInstruction<1> {
 public:
  explicit HDummyUse(HValue* value)
      : HTemplateInstruction<1>(HType::Smi()) {
    SetOperandAt(0, value);
    // Pretend to be a Smi so that the HChange instructions inserted
    // before any use generate as little code as possible.
    set_representation(Representation::Tagged());
  }

  HValue* value() const { return OperandAt(0); }

  virtual bool HasEscapingOperandAt(int index) OVERRIDE { return false; }
  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(DummyUse);
};


// Inserts an int3/stop break instruction for debugging purposes.
class HDebugBreak FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HDebugBreak);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(DebugBreak)
};


class HGoto FINAL : public HTemplateControlInstruction<1, 0> {
 public:
  explicit HGoto(HBasicBlock* target) {
    SetSuccessorAt(0, target);
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE {
    *block = FirstSuccessor();
    return true;
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(Goto)
};


class HDeoptimize FINAL : public HTemplateControlInstruction<1, 0> {
 public:
  static HDeoptimize* New(Zone* zone,
                          HValue* context,
                          const char* reason,
                          Deoptimizer::BailoutType type,
                          HBasicBlock* unreachable_continuation) {
    return new(zone) HDeoptimize(reason, type, unreachable_continuation);
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE {
    *block = NULL;
    return true;
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  HValue* value() const { return OperandAt(0); }
};


class HBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P2(HBranch, HValue*,
                                 ToBooleanStub::Types);
  DECLARE_INSTRUCTION_FACTORY_P4(HBranch, HValue*,
                                 ToBooleanStub::Types,
                                 HBasicBlock*, HBasicBlock*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }
  virtual Representation observed_input_representation(int index) OVERRIDE;

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE;

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

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


class HCompareMap FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HCompareMap, HValue*, Handle<Map>);
  DECLARE_INSTRUCTION_FACTORY_P4(HCompareMap, HValue*, Handle<Map>,
                                 HBasicBlock*, HBasicBlock*);

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE {
    if (known_successor_index() != kNoKnownSuccessorIndex) {
      *block = SuccessorAt(known_successor_index());
      return true;
    }
    *block = NULL;
    return false;
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  static const int kNoKnownSuccessorIndex = -1;
  int known_successor_index() const { return known_successor_index_; }
  void set_known_successor_index(int known_successor_index) {
    known_successor_index_ = known_successor_index;
  }

  Unique<Map> map() const { return map_; }
  bool map_is_stable() const { return map_is_stable_; }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareMap)

 protected:
  virtual int RedefinedOperandIndex() { return 0; }

 private:
  HCompareMap(HValue* value,
              Handle<Map> map,
              HBasicBlock* true_target = NULL,
              HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target),
        known_successor_index_(kNoKnownSuccessorIndex),
        map_is_stable_(map->is_stable()),
        map_(Unique<Map>::CreateImmovable(map)) {
    set_representation(Representation::Tagged());
  }

  int known_successor_index_ : 31;
  bool map_is_stable_ : 1;
  Unique<Map> map_;
};


class HContext FINAL : public HTemplateInstruction<0> {
 public:
  static HContext* New(Zone* zone) {
    return new(zone) HContext();
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Context)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  HContext() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HReturn FINAL : public HTemplateControlInstruction<0, 3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HReturn, HValue*, HValue*);
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HReturn, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    // TODO(titzer): require an Int32 input for faster returns.
    if (index == 2) return Representation::Smi();
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  HValue* value() const { return OperandAt(0); }
  HValue* context() const { return OperandAt(1); }
  HValue* parameter_count() const { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(Return)

 private:
  HReturn(HValue* context, HValue* value, HValue* parameter_count = 0) {
    SetOperandAt(0, value);
    SetOperandAt(1, context);
    SetOperandAt(2, parameter_count);
  }
};


class HAbnormalExit FINAL : public HTemplateControlInstruction<0, 0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HAbnormalExit);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(AbnormalExit)
 private:
  HAbnormalExit() {}
};


class HUnaryOperation : public HTemplateInstruction<1> {
 public:
  explicit HUnaryOperation(HValue* value, HType type = HType::Tagged())
      : HTemplateInstruction<1>(type) {
    SetOperandAt(0, value);
  }

  static HUnaryOperation* cast(HValue* value) {
    return reinterpret_cast<HUnaryOperation*>(value);
  }

  HValue* value() const { return OperandAt(0); }
  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT
};


class HUseConst FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HUseConst, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(UseConst)

 private:
    explicit HUseConst(HValue* old_value) : HUnaryOperation(old_value) { }
};


class HForceRepresentation FINAL : public HTemplateInstruction<1> {
 public:
  static HInstruction* New(Zone* zone, HValue* context, HValue* value,
                           Representation required_representation);

  HValue* value() const { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return representation();  // Same as the output representation.
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(ForceRepresentation)

 private:
  HForceRepresentation(HValue* value, Representation required_representation) {
    SetOperandAt(0, value);
    set_representation(required_representation);
  }
};


class HChange FINAL : public HUnaryOperation {
 public:
  HChange(HValue* value,
          Representation to,
          bool is_truncating_to_smi,
          bool is_truncating_to_int32)
      : HUnaryOperation(value) {
    DCHECK(!value->representation().IsNone());
    DCHECK(!to.IsNone());
    DCHECK(!value->representation().Equals(to));
    set_representation(to);
    SetFlag(kUseGVN);
    SetFlag(kCanOverflow);
    if (is_truncating_to_smi && to.IsSmi()) {
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

  virtual HType CalculateInferredType() OVERRIDE;
  virtual HValue* Canonicalize() OVERRIDE;

  Representation from() const { return value()->representation(); }
  Representation to() const { return representation(); }
  bool deoptimize_on_minus_zero() const {
    return CheckFlag(kBailoutOnMinusZero);
  }
  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return from();
  }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(Change)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  virtual bool IsDeletable() const OVERRIDE {
    return !from().IsTagged() || value()->type().IsSmi();
  }
};


class HClampToUint8 FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HClampToUint8, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ClampToUint8)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  explicit HClampToUint8(HValue* value)
      : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kAllowUndefinedAsNaN);
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HDoubleBits FINAL : public HUnaryOperation {
 public:
  enum Bits { HIGH, LOW };
  DECLARE_INSTRUCTION_FACTORY_P2(HDoubleBits, HValue*, Bits);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Double();
  }

  DECLARE_CONCRETE_INSTRUCTION(DoubleBits)

  Bits bits() { return bits_; }

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    return other->IsDoubleBits() && HDoubleBits::cast(other)->bits() == bits();
  }

 private:
  HDoubleBits(HValue* value, Bits bits)
      : HUnaryOperation(value), bits_(bits) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }

  Bits bits_;
};


class HConstructDouble FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HConstructDouble, HValue*, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Integer32();
  }

  DECLARE_CONCRETE_INSTRUCTION(ConstructDouble)

  HValue* hi() { return OperandAt(0); }
  HValue* lo() { return OperandAt(1); }

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  explicit HConstructDouble(HValue* hi, HValue* lo) {
    set_representation(Representation::Double());
    SetFlag(kUseGVN);
    SetOperandAt(0, hi);
    SetOperandAt(1, lo);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


enum RemovableSimulate {
  REMOVABLE_SIMULATE,
  FIXED_SIMULATE
};


class HSimulate FINAL : public HInstruction {
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
        removable_(removable),
        done_with_replay_(false) {}
  ~HSimulate() {}

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  bool HasAstId() const { return !ast_id_.IsNone(); }
  BailoutId ast_id() const { return ast_id_; }
  void set_ast_id(BailoutId id) {
    DCHECK(!HasAstId());
    ast_id_ = id;
  }

  int pop_count() const { return pop_count_; }
  const ZoneList<HValue*>* values() const { return &values_; }
  int GetAssignedIndexAt(int index) const {
    DCHECK(HasAssignedIndexAt(index));
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
  virtual int OperandCount() const OVERRIDE { return values_.length(); }
  virtual HValue* OperandAt(int index) const OVERRIDE {
    return values_[index];
  }

  virtual bool HasEscapingOperandAt(int index) OVERRIDE { return false; }
  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  void MergeWith(ZoneList<HSimulate*>* list);
  bool is_candidate_for_removal() { return removable_ == REMOVABLE_SIMULATE; }

  // Replay effects of this instruction on the given environment.
  void ReplayEnvironment(HEnvironment* env);

  DECLARE_CONCRETE_INSTRUCTION(Simulate)

#ifdef DEBUG
  virtual void Verify() OVERRIDE;
  void set_closure(Handle<JSFunction> closure) { closure_ = closure; }
  Handle<JSFunction> closure() const { return closure_; }
#endif

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) OVERRIDE {
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
  RemovableSimulate removable_ : 2;
  bool done_with_replay_ : 1;

#ifdef DEBUG
  Handle<JSFunction> closure_;
#endif
};


class HEnvironmentMarker FINAL : public HTemplateInstruction<1> {
 public:
  enum Kind { BIND, LOOKUP };

  DECLARE_INSTRUCTION_FACTORY_P2(HEnvironmentMarker, Kind, int);

  Kind kind() const { return kind_; }
  int index() const { return index_; }
  HSimulate* next_simulate() { return next_simulate_; }
  void set_next_simulate(HSimulate* simulate) {
    next_simulate_ = simulate;
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

#ifdef DEBUG
  void set_closure(Handle<JSFunction> closure) {
    DCHECK(closure_.is_null());
    DCHECK(!closure.is_null());
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


class HStackCheck FINAL : public HTemplateInstruction<1> {
 public:
  enum Type {
    kFunctionEntry,
    kBackwardsBranch
  };

  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HStackCheck, Type);

  HValue* context() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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
class HConstant;


class HEnterInlined FINAL : public HTemplateInstruction<0> {
 public:
  static HEnterInlined* New(Zone* zone, HValue* context, BailoutId return_id,
                            Handle<JSFunction> closure,
                            HConstant* closure_context, int arguments_count,
                            FunctionLiteral* function,
                            InliningKind inlining_kind, Variable* arguments_var,
                            HArgumentsObject* arguments_object) {
    return new (zone) HEnterInlined(return_id, closure, closure_context,
                                    arguments_count, function, inlining_kind,
                                    arguments_var, arguments_object, zone);
  }

  void RegisterReturnTarget(HBasicBlock* return_target, Zone* zone);
  ZoneList<HBasicBlock*>* return_targets() { return &return_targets_; }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  Handle<JSFunction> closure() const { return closure_; }
  HConstant* closure_context() const { return closure_context_; }
  int arguments_count() const { return arguments_count_; }
  bool arguments_pushed() const { return arguments_pushed_; }
  void set_arguments_pushed() { arguments_pushed_ = true; }
  FunctionLiteral* function() const { return function_; }
  InliningKind inlining_kind() const { return inlining_kind_; }
  BailoutId ReturnId() const { return return_id_; }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  Variable* arguments_var() { return arguments_var_; }
  HArgumentsObject* arguments_object() { return arguments_object_; }

  DECLARE_CONCRETE_INSTRUCTION(EnterInlined)

 private:
  HEnterInlined(BailoutId return_id, Handle<JSFunction> closure,
                HConstant* closure_context, int arguments_count,
                FunctionLiteral* function, InliningKind inlining_kind,
                Variable* arguments_var, HArgumentsObject* arguments_object,
                Zone* zone)
      : return_id_(return_id),
        closure_(closure),
        closure_context_(closure_context),
        arguments_count_(arguments_count),
        arguments_pushed_(false),
        function_(function),
        inlining_kind_(inlining_kind),
        arguments_var_(arguments_var),
        arguments_object_(arguments_object),
        return_targets_(2, zone) {}

  BailoutId return_id_;
  Handle<JSFunction> closure_;
  HConstant* closure_context_;
  int arguments_count_;
  bool arguments_pushed_;
  FunctionLiteral* function_;
  InliningKind inlining_kind_;
  Variable* arguments_var_;
  HArgumentsObject* arguments_object_;
  ZoneList<HBasicBlock*> return_targets_;
};


class HLeaveInlined FINAL : public HTemplateInstruction<0> {
 public:
  HLeaveInlined(HEnterInlined* entry,
                int drop_count)
      : entry_(entry),
        drop_count_(drop_count) { }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  virtual int argument_delta() const OVERRIDE {
    return entry_->arguments_pushed() ? -drop_count_ : 0;
  }

  DECLARE_CONCRETE_INSTRUCTION(LeaveInlined)

 private:
  HEnterInlined* entry_;
  int drop_count_;
};


class HPushArguments FINAL : public HInstruction {
 public:
  static HPushArguments* New(Zone* zone, HValue* context) {
    return new(zone) HPushArguments(zone);
  }
  static HPushArguments* New(Zone* zone, HValue* context, HValue* arg1) {
    HPushArguments* instr = new(zone) HPushArguments(zone);
    instr->AddInput(arg1);
    return instr;
  }
  static HPushArguments* New(Zone* zone, HValue* context, HValue* arg1,
                             HValue* arg2) {
    HPushArguments* instr = new(zone) HPushArguments(zone);
    instr->AddInput(arg1);
    instr->AddInput(arg2);
    return instr;
  }
  static HPushArguments* New(Zone* zone, HValue* context, HValue* arg1,
                             HValue* arg2, HValue* arg3) {
    HPushArguments* instr = new(zone) HPushArguments(zone);
    instr->AddInput(arg1);
    instr->AddInput(arg2);
    instr->AddInput(arg3);
    return instr;
  }
  static HPushArguments* New(Zone* zone, HValue* context, HValue* arg1,
                             HValue* arg2, HValue* arg3, HValue* arg4) {
    HPushArguments* instr = new(zone) HPushArguments(zone);
    instr->AddInput(arg1);
    instr->AddInput(arg2);
    instr->AddInput(arg3);
    instr->AddInput(arg4);
    return instr;
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual int argument_delta() const OVERRIDE { return inputs_.length(); }
  HValue* argument(int i) { return OperandAt(i); }

  virtual int OperandCount() const FINAL OVERRIDE {
    return inputs_.length();
  }
  virtual HValue* OperandAt(int i) const FINAL OVERRIDE {
    return inputs_[i];
  }

  void AddInput(HValue* value);

  DECLARE_CONCRETE_INSTRUCTION(PushArguments)

 protected:
  virtual void InternalSetOperandAt(int i, HValue* value) FINAL OVERRIDE {
    inputs_[i] = value;
  }

 private:
  explicit HPushArguments(Zone* zone)
      : HInstruction(HType::Tagged()), inputs_(4, zone) {
    set_representation(Representation::Tagged());
  }

  ZoneList<HValue*> inputs_;
};


class HThisFunction FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HThisFunction);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ThisFunction)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  HThisFunction() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HDeclareGlobals FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HDeclareGlobals,
                                              Handle<FixedArray>,
                                              int);

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> pairs() const { return pairs_; }
  int flags() const { return flags_; }

  DECLARE_CONCRETE_INSTRUCTION(DeclareGlobals)

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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

  virtual HType CalculateInferredType() FINAL OVERRIDE {
    return HType::Tagged();
  }

  virtual int argument_count() const {
    return argument_count_;
  }

  virtual int argument_delta() const OVERRIDE {
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
      int index) FINAL OVERRIDE {
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  HValue* value() const { return OperandAt(0); }
};


class HBinaryCall : public HCall<2> {
 public:
  HBinaryCall(HValue* first, HValue* second, int argument_count)
      : HCall<2>(argument_count) {
    SetOperandAt(0, first);
    SetOperandAt(1, second);
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(
      int index) FINAL OVERRIDE {
    return Representation::Tagged();
  }

  HValue* first() const { return OperandAt(0); }
  HValue* second() const { return OperandAt(1); }
};


class HCallJSFunction FINAL : public HCall<1> {
 public:
  static HCallJSFunction* New(Zone* zone,
                              HValue* context,
                              HValue* function,
                              int argument_count,
                              bool pass_argument_count);

  HValue* function() const { return OperandAt(0); }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(
      int index) FINAL OVERRIDE {
    DCHECK(index == 0);
    return Representation::Tagged();
  }

  bool pass_argument_count() const { return pass_argument_count_; }

  virtual bool HasStackCheck() FINAL OVERRIDE {
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


class HCallWithDescriptor FINAL : public HInstruction {
 public:
  static HCallWithDescriptor* New(Zone* zone, HValue* context, HValue* target,
                                  int argument_count,
                                  CallInterfaceDescriptor descriptor,
                                  const Vector<HValue*>& operands) {
    DCHECK(operands.length() == descriptor.GetEnvironmentLength());
    HCallWithDescriptor* res = new (zone)
        HCallWithDescriptor(target, argument_count, descriptor, operands, zone);
    return res;
  }

  virtual int OperandCount() const FINAL OVERRIDE {
    return values_.length();
  }
  virtual HValue* OperandAt(int index) const FINAL OVERRIDE {
    return values_[index];
  }

  virtual Representation RequiredInputRepresentation(
      int index) FINAL OVERRIDE {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      int par_index = index - 1;
      DCHECK(par_index < descriptor_.GetEnvironmentLength());
      return descriptor_.GetParameterRepresentation(par_index);
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(CallWithDescriptor)

  virtual HType CalculateInferredType() FINAL OVERRIDE {
    return HType::Tagged();
  }

  virtual int argument_count() const {
    return argument_count_;
  }

  virtual int argument_delta() const OVERRIDE {
    return -argument_count_;
  }

  CallInterfaceDescriptor descriptor() const { return descriptor_; }

  HValue* target() {
    return OperandAt(0);
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

 private:
  // The argument count includes the receiver.
  HCallWithDescriptor(HValue* target, int argument_count,
                      CallInterfaceDescriptor descriptor,
                      const Vector<HValue*>& operands, Zone* zone)
      : descriptor_(descriptor),
        values_(descriptor.GetEnvironmentLength() + 1, zone) {
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
                            HValue* value) FINAL OVERRIDE {
    values_[index] = value;
  }

  CallInterfaceDescriptor descriptor_;
  ZoneList<HValue*> values_;
  int argument_count_;
};


class HInvokeFunction FINAL : public HBinaryCall {
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

  virtual bool HasStackCheck() FINAL OVERRIDE {
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


class HCallFunction FINAL : public HBinaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HCallFunction, HValue*, int);
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(
      HCallFunction, HValue*, int, CallFunctionFlags);

  HValue* context() { return first(); }
  HValue* function() { return second(); }
  CallFunctionFlags function_flags() const { return function_flags_; }

  DECLARE_CONCRETE_INSTRUCTION(CallFunction)

  virtual int argument_delta() const OVERRIDE { return -argument_count(); }

 private:
  HCallFunction(HValue* context,
                HValue* function,
                int argument_count,
                CallFunctionFlags flags = NO_CALL_FUNCTION_FLAGS)
      : HBinaryCall(context, function, argument_count), function_flags_(flags) {
  }
  CallFunctionFlags function_flags_;
};


class HCallNew FINAL : public HBinaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HCallNew, HValue*, int);

  HValue* context() { return first(); }
  HValue* constructor() { return second(); }

  DECLARE_CONCRETE_INSTRUCTION(CallNew)

 private:
  HCallNew(HValue* context, HValue* constructor, int argument_count)
      : HBinaryCall(context, constructor, argument_count) {}
};


class HCallNewArray FINAL : public HBinaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HCallNewArray,
                                              HValue*,
                                              int,
                                              ElementsKind);

  HValue* context() { return first(); }
  HValue* constructor() { return second(); }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  ElementsKind elements_kind() const { return elements_kind_; }

  DECLARE_CONCRETE_INSTRUCTION(CallNewArray)

 private:
  HCallNewArray(HValue* context, HValue* constructor, int argument_count,
                ElementsKind elements_kind)
      : HBinaryCall(context, constructor, argument_count),
        elements_kind_(elements_kind) {}

  ElementsKind elements_kind_;
};


class HCallRuntime FINAL : public HCall<1> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HCallRuntime,
                                              Handle<String>,
                                              const Runtime::Function*,
                                              int);

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  HValue* context() { return OperandAt(0); }
  const Runtime::Function* function() const { return c_function_; }
  Handle<String> name() const { return name_; }
  SaveFPRegsMode save_doubles() const { return save_doubles_; }
  void set_save_doubles(SaveFPRegsMode save_doubles) {
    save_doubles_ = save_doubles;
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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


class HMapEnumLength FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HMapEnumLength, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(MapEnumLength)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  explicit HMapEnumLength(HValue* value)
      : HUnaryOperation(value, HType::Smi()) {
    set_representation(Representation::Smi());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kMaps);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HUnaryMathOperation FINAL : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* value,
                           BuiltinFunctionId op);

  HValue* context() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      switch (op_) {
        case kMathFloor:
        case kMathRound:
        case kMathFround:
        case kMathSqrt:
        case kMathPowHalf:
        case kMathLog:
        case kMathExp:
          return Representation::Double();
        case kMathAbs:
          return representation();
        case kMathClz32:
          return Representation::Integer32();
        default:
          UNREACHABLE();
          return Representation::None();
      }
    }
  }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

  virtual HValue* Canonicalize() OVERRIDE;
  virtual Representation RepresentationFromUses() OVERRIDE;
  virtual Representation RepresentationFromInputs() OVERRIDE;

  BuiltinFunctionId op() const { return op_; }
  const char* OpName() const;

  DECLARE_CONCRETE_INSTRUCTION(UnaryMathOperation)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    HUnaryMathOperation* b = HUnaryMathOperation::cast(other);
    return op_ == b->op();
  }

 private:
  // Indicates if we support a double (and int32) output for Math.floor and
  // Math.round.
  bool SupportsFlexibleFloorAndRound() const {
#ifdef V8_TARGET_ARCH_ARM64
    return true;
#else
    return false;
#endif
  }
  HUnaryMathOperation(HValue* context, HValue* value, BuiltinFunctionId op)
      : HTemplateInstruction<2>(HType::TaggedNumber()), op_(op) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    switch (op) {
      case kMathFloor:
      case kMathRound:
        if (SupportsFlexibleFloorAndRound()) {
          SetFlag(kFlexibleRepresentation);
        } else {
          set_representation(Representation::Integer32());
        }
        break;
      case kMathClz32:
        set_representation(Representation::Integer32());
        break;
      case kMathAbs:
        // Not setting representation here: it is None intentionally.
        SetFlag(kFlexibleRepresentation);
        // TODO(svenpanne) This flag is actually only needed if representation()
        // is tagged, and not when it is an unboxed double or unboxed integer.
        SetChangesFlag(kNewSpacePromotion);
        break;
      case kMathFround:
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

  virtual bool IsDeletable() const OVERRIDE { return true; }

  HValue* SimplifiedDividendForMathFloorOfDiv(HDiv* hdiv);
  HValue* SimplifiedDivisorForMathFloorOfDiv(HDiv* hdiv);

  BuiltinFunctionId op_;
};


class HLoadRoot FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HLoadRoot, Heap::RootListIndex);
  DECLARE_INSTRUCTION_FACTORY_P2(HLoadRoot, Heap::RootListIndex, HType);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  Heap::RootListIndex index() const { return index_; }

  DECLARE_CONCRETE_INSTRUCTION(LoadRoot)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    HLoadRoot* b = HLoadRoot::cast(other);
    return index_ == b->index_;
  }

 private:
  explicit HLoadRoot(Heap::RootListIndex index, HType type = HType::Tagged())
      : HTemplateInstruction<0>(type), index_(index) {
    SetFlag(kUseGVN);
    // TODO(bmeurer): We'll need kDependsOnRoots once we add the
    // corresponding HStoreRoot instruction.
    SetDependsOnFlag(kCalls);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }

  const Heap::RootListIndex index_;
};


class HCheckMaps FINAL : public HTemplateInstruction<2> {
 public:
  static HCheckMaps* New(Zone* zone, HValue* context, HValue* value,
                         Handle<Map> map, HValue* typecheck = NULL) {
    return new(zone) HCheckMaps(value, new(zone) UniqueSet<Map>(
            Unique<Map>::CreateImmovable(map), zone), typecheck);
  }
  static HCheckMaps* New(Zone* zone, HValue* context,
                         HValue* value, SmallMapList* map_list,
                         HValue* typecheck = NULL) {
    UniqueSet<Map>* maps = new(zone) UniqueSet<Map>(map_list->length(), zone);
    for (int i = 0; i < map_list->length(); ++i) {
      maps->Add(Unique<Map>::CreateImmovable(map_list->at(i)), zone);
    }
    return new(zone) HCheckMaps(value, maps, typecheck);
  }

  bool IsStabilityCheck() const { return is_stability_check_; }
  void MarkAsStabilityCheck() {
    maps_are_stable_ = true;
    has_migration_target_ = false;
    is_stability_check_ = true;
    ClearChangesFlag(kNewSpacePromotion);
    ClearDependsOnFlag(kElementsKind);
    ClearDependsOnFlag(kMaps);
  }

  virtual bool HasEscapingOperandAt(int index) OVERRIDE { return false; }
  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() OVERRIDE {
    if (value()->type().IsHeapObject()) return value()->type();
    return HType::HeapObject();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  HValue* value() const { return OperandAt(0); }
  HValue* typecheck() const { return OperandAt(1); }

  const UniqueSet<Map>* maps() const { return maps_; }
  void set_maps(const UniqueSet<Map>* maps) { maps_ = maps; }

  bool maps_are_stable() const { return maps_are_stable_; }

  bool HasMigrationTarget() const { return has_migration_target_; }

  virtual HValue* Canonicalize() OVERRIDE;

  static HCheckMaps* CreateAndInsertAfter(Zone* zone,
                                          HValue* value,
                                          Unique<Map> map,
                                          bool map_is_stable,
                                          HInstruction* instr) {
    return instr->Append(new(zone) HCheckMaps(
            value, new(zone) UniqueSet<Map>(map, zone), map_is_stable));
  }

  static HCheckMaps* CreateAndInsertBefore(Zone* zone,
                                           HValue* value,
                                           const UniqueSet<Map>* maps,
                                           bool maps_are_stable,
                                           HInstruction* instr) {
    return instr->Prepend(new(zone) HCheckMaps(value, maps, maps_are_stable));
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckMaps)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    return this->maps()->Equals(HCheckMaps::cast(other)->maps());
  }

  virtual int RedefinedOperandIndex() { return 0; }

 private:
  HCheckMaps(HValue* value, const UniqueSet<Map>* maps, bool maps_are_stable)
      : HTemplateInstruction<2>(HType::HeapObject()), maps_(maps),
        has_migration_target_(false), is_stability_check_(false),
        maps_are_stable_(maps_are_stable) {
    DCHECK_NE(0, maps->size());
    SetOperandAt(0, value);
    // Use the object value for the dependency.
    SetOperandAt(1, value);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kMaps);
    SetDependsOnFlag(kElementsKind);
  }

  HCheckMaps(HValue* value, const UniqueSet<Map>* maps, HValue* typecheck)
      : HTemplateInstruction<2>(HType::HeapObject()), maps_(maps),
        has_migration_target_(false), is_stability_check_(false),
        maps_are_stable_(true) {
    DCHECK_NE(0, maps->size());
    SetOperandAt(0, value);
    // Use the object value for the dependency if NULL is passed.
    SetOperandAt(1, typecheck ? typecheck : value);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kMaps);
    SetDependsOnFlag(kElementsKind);
    for (int i = 0; i < maps->size(); ++i) {
      Handle<Map> map = maps->at(i).handle();
      if (map->is_migration_target()) has_migration_target_ = true;
      if (!map->is_stable()) maps_are_stable_ = false;
    }
    if (has_migration_target_) SetChangesFlag(kNewSpacePromotion);
  }

  const UniqueSet<Map>* maps_;
  bool has_migration_target_ : 1;
  bool is_stability_check_ : 1;
  bool maps_are_stable_ : 1;
};


class HCheckValue FINAL : public HUnaryOperation {
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

  virtual void FinalizeUniqueness() OVERRIDE {
    object_ = Unique<HeapObject>(object_.handle());
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }
  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual HValue* Canonicalize() OVERRIDE;

#ifdef DEBUG
  virtual void Verify() OVERRIDE;
#endif

  Unique<HeapObject> object() const { return object_; }
  bool object_in_new_space() const { return object_in_new_space_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckValue)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
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


class HCheckInstanceType FINAL : public HUnaryOperation {
 public:
  enum Check {
    IS_SPEC_OBJECT,
    IS_JS_ARRAY,
    IS_STRING,
    IS_INTERNALIZED_STRING,
    LAST_INTERVAL_CHECK = IS_JS_ARRAY
  };

  DECLARE_INSTRUCTION_FACTORY_P2(HCheckInstanceType, HValue*, Check);

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() OVERRIDE {
    switch (check_) {
      case IS_SPEC_OBJECT: return HType::JSObject();
      case IS_JS_ARRAY: return HType::JSArray();
      case IS_STRING: return HType::String();
      case IS_INTERNALIZED_STRING: return HType::String();
    }
    UNREACHABLE();
    return HType::Tagged();
  }

  virtual HValue* Canonicalize() OVERRIDE;

  bool is_interval_check() const { return check_ <= LAST_INTERVAL_CHECK; }
  void GetCheckInterval(InstanceType* first, InstanceType* last);
  void GetCheckMaskAndTag(uint8_t* mask, uint8_t* tag);

  Check check() const { return check_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckInstanceType)

 protected:
  // TODO(ager): It could be nice to allow the ommision of instance
  // type checks if we have already performed an instance type check
  // with a larger range.
  virtual bool DataEquals(HValue* other) OVERRIDE {
    HCheckInstanceType* b = HCheckInstanceType::cast(other);
    return check_ == b->check_;
  }

  virtual int RedefinedOperandIndex() { return 0; }

 private:
  const char* GetCheckName() const;

  HCheckInstanceType(HValue* value, Check check)
      : HUnaryOperation(value, HType::HeapObject()), check_(check) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  const Check check_;
};


class HCheckSmi FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCheckSmi, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual HValue* Canonicalize() OVERRIDE {
    HType value_type = value()->type();
    if (value_type.IsSmi()) {
      return NULL;
    }
    return this;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckSmi)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  explicit HCheckSmi(HValue* value) : HUnaryOperation(value, HType::Smi()) {
    set_representation(Representation::Smi());
    SetFlag(kUseGVN);
  }
};


class HCheckHeapObject FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCheckHeapObject, HValue*);

  virtual bool HasEscapingOperandAt(int index) OVERRIDE { return false; }
  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() OVERRIDE {
    if (value()->type().IsHeapObject()) return value()->type();
    return HType::HeapObject();
  }

#ifdef DEBUG
  virtual void Verify() OVERRIDE;
#endif

  virtual HValue* Canonicalize() OVERRIDE {
    return value()->type().IsHeapObject() ? NULL : this;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckHeapObject)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  explicit HCheckHeapObject(HValue* value) : HUnaryOperation(value) {
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
class HBitwise;


class InductionVariableData FINAL : public ZoneObject {
 public:
  class InductionVariableCheck : public ZoneObject {
   public:
    HBoundsCheck* check() { return check_; }
    InductionVariableCheck* next() { return next_; }
    bool HasUpperLimit() { return upper_limit_ >= 0; }
    int32_t upper_limit() {
      DCHECK(HasUpperLimit());
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


class HPhi FINAL : public HValue {
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
    DCHECK(merged_index >= 0 || merged_index == kInvalidMergedIndex);
    SetFlag(kFlexibleRepresentation);
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual Representation RepresentationFromInputs() OVERRIDE;

  virtual Range* InferRange(Zone* zone) OVERRIDE;
  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) OVERRIDE;
  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return representation();
  }
  virtual Representation KnownOptimalRepresentation() OVERRIDE {
    return representation();
  }
  virtual HType CalculateInferredType() OVERRIDE;
  virtual int OperandCount() const OVERRIDE { return inputs_.length(); }
  virtual HValue* OperandAt(int index) const OVERRIDE {
    return inputs_[index];
  }
  HValue* GetRedundantReplacement();
  void AddInput(HValue* value);
  bool HasRealUses();

  bool IsReceiver() const { return merged_index_ == 0; }
  bool HasMergedIndex() const { return merged_index_ != kInvalidMergedIndex; }

  virtual HSourcePosition position() const OVERRIDE;

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
    DCHECK(induction_variable_data_ == NULL);
    induction_variable_data_ = InductionVariableData::ExaminePhi(this);
  }

  virtual OStream& PrintTo(OStream& os) const OVERRIDE;  // NOLINT

#ifdef DEBUG
  virtual void Verify() OVERRIDE;
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
    DCHECK(value->IsPhi());
    return reinterpret_cast<HPhi*>(value);
  }
  virtual Opcode opcode() const OVERRIDE { return HValue::kPhi; }

  void SimplifyConstantInputs();

  // Marker value representing an invalid merge index.
  static const int kInvalidMergedIndex = -1;

 protected:
  virtual void DeleteFromGraph() OVERRIDE;
  virtual void InternalSetOperandAt(int index, HValue* value) OVERRIDE {
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
  virtual bool IsDeletable() const OVERRIDE { return !IsReceiver(); }
};


// Common base class for HArgumentsObject and HCapturedObject.
class HDematerializedObject : public HInstruction {
 public:
  HDematerializedObject(int count, Zone* zone) : values_(count, zone) {}

  virtual int OperandCount() const FINAL OVERRIDE {
    return values_.length();
  }
  virtual HValue* OperandAt(int index) const FINAL OVERRIDE {
    return values_[index];
  }

  virtual bool HasEscapingOperandAt(int index) FINAL OVERRIDE {
    return false;
  }
  virtual Representation RequiredInputRepresentation(
      int index) FINAL OVERRIDE {
    return Representation::None();
  }

 protected:
  virtual void InternalSetOperandAt(int index,
                                    HValue* value) FINAL OVERRIDE {
    values_[index] = value;
  }

  // List of values tracked by this marker.
  ZoneList<HValue*> values_;
};


class HArgumentsObject FINAL : public HDematerializedObject {
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


class HCapturedObject FINAL : public HDematerializedObject {
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
    DCHECK(store->HasObservableSideEffects());
    DCHECK(store->IsStoreNamedField());
    changes_flags_.Add(store->ChangesFlags());
  }

  // Replay effects of this instruction on the given environment.
  void ReplayEnvironment(HEnvironment* env);

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(CapturedObject)

 private:
  int capture_id_;

  // Note that we cannot DCE captured objects as they are used to replay
  // the environment. This method is here as an explicit reminder.
  // TODO(mstarzinger): Turn HSimulates into full snapshots maybe?
  virtual bool IsDeletable() const FINAL OVERRIDE { return false; }
};


class HConstant FINAL : public HTemplateInstruction<0> {
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

  virtual Handle<Map> GetMonomorphicJSObjectMap() OVERRIDE {
    Handle<Object> object = object_.handle();
    if (!object.is_null() && object->IsHeapObject()) {
      return v8::internal::handle(HeapObject::cast(*object)->map());
    }
    return Handle<Map>();
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
                                          Unique<Map> map,
                                          bool map_is_stable,
                                          HInstruction* instruction) {
    return instruction->Prepend(new(zone) HConstant(
        map, Unique<Map>(Handle<Map>::null()), map_is_stable,
        Representation::Tagged(), HType::HeapObject(), true,
        false, false, MAP_TYPE));
  }

  static HConstant* CreateAndInsertAfter(Zone* zone,
                                         Unique<Map> map,
                                         bool map_is_stable,
                                         HInstruction* instruction) {
    return instruction->Append(new(zone) HConstant(
            map, Unique<Map>(Handle<Map>::null()), map_is_stable,
            Representation::Tagged(), HType::HeapObject(), true,
            false, false, MAP_TYPE));
  }

  Handle<Object> handle(Isolate* isolate) {
    if (object_.handle().is_null()) {
      // Default arguments to is_not_in_new_space depend on this heap number
      // to be tenured so that it's guaranteed not to be located in new space.
      object_ = Unique<Object>::CreateUninitialized(
          isolate->factory()->NewNumber(double_value_, TENURED));
    }
    AllowDeferredHandleDereference smi_check;
    DCHECK(has_int32_value_ || !object_.handle()->IsSmi());
    return object_.handle();
  }

  bool IsSpecialDouble() const {
    return has_double_value_ &&
           (bit_cast<int64_t>(double_value_) == bit_cast<int64_t>(-0.0) ||
            FixedDoubleArray::is_the_hole_nan(double_value_) ||
            std::isnan(double_value_));
  }

  bool NotInNewSpace() const {
    return is_not_in_new_space_;
  }

  bool ImmortalImmovable() const;

  bool IsCell() const {
    return instance_type_ == CELL_TYPE || instance_type_ == PROPERTY_CELL_TYPE;
  }

  bool IsMap() const {
    return instance_type_ == MAP_TYPE;
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  virtual Representation KnownOptimalRepresentation() OVERRIDE {
    if (HasSmiValue() && SmiValuesAre31Bits()) return Representation::Smi();
    if (HasInteger32Value()) return Representation::Integer32();
    if (HasNumberValue()) return Representation::Double();
    if (HasExternalReferenceValue()) return Representation::External();
    return Representation::Tagged();
  }

  virtual bool EmitAtUses() OVERRIDE;
  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT
  HConstant* CopyToRepresentation(Representation r, Zone* zone) const;
  Maybe<HConstant*> CopyToTruncatedInt32(Zone* zone);
  Maybe<HConstant*> CopyToTruncatedNumber(Zone* zone);
  bool HasInteger32Value() const { return has_int32_value_; }
  int32_t Integer32Value() const {
    DCHECK(HasInteger32Value());
    return int32_value_;
  }
  bool HasSmiValue() const { return has_smi_value_; }
  bool HasDoubleValue() const { return has_double_value_; }
  double DoubleValue() const {
    DCHECK(HasDoubleValue());
    return double_value_;
  }
  bool IsTheHole() const {
    if (HasDoubleValue() && FixedDoubleArray::is_the_hole_nan(double_value_)) {
      return true;
    }
    return object_.IsInitialized() &&
           object_.IsKnownGlobal(isolate()->heap()->the_hole_value());
  }
  bool HasNumberValue() const { return has_double_value_; }
  int32_t NumberValueAsInteger32() const {
    DCHECK(HasNumberValue());
    // Irrespective of whether a numeric HConstant can be safely
    // represented as an int32, we store the (in some cases lossy)
    // representation of the number in int32_value_.
    return int32_value_;
  }
  bool HasStringValue() const {
    if (has_double_value_ || has_int32_value_) return false;
    DCHECK(!object_.handle().is_null());
    return instance_type_ < FIRST_NONSTRING_TYPE;
  }
  Handle<String> StringValue() const {
    DCHECK(HasStringValue());
    return Handle<String>::cast(object_.handle());
  }
  bool HasInternalizedStringValue() const {
    return HasStringValue() && StringShape(instance_type_).IsInternalized();
  }

  bool HasExternalReferenceValue() const {
    return has_external_reference_value_;
  }
  ExternalReference ExternalReferenceValue() const {
    return external_reference_value_;
  }

  bool HasBooleanValue() const { return type_.IsBoolean(); }
  bool BooleanValue() const { return boolean_value_; }
  bool IsUndetectable() const { return is_undetectable_; }
  InstanceType GetInstanceType() const { return instance_type_; }

  bool HasMapValue() const { return instance_type_ == MAP_TYPE; }
  Unique<Map> MapValue() const {
    DCHECK(HasMapValue());
    return Unique<Map>::cast(GetUnique());
  }
  bool HasStableMapValue() const {
    DCHECK(HasMapValue() || !has_stable_map_value_);
    return has_stable_map_value_;
  }

  bool HasObjectMap() const { return !object_map_.IsNull(); }
  Unique<Map> ObjectMap() const {
    DCHECK(HasObjectMap());
    return object_map_;
  }

  virtual intptr_t Hashcode() OVERRIDE {
    if (has_int32_value_) {
      return static_cast<intptr_t>(int32_value_);
    } else if (has_double_value_) {
      return static_cast<intptr_t>(bit_cast<int64_t>(double_value_));
    } else if (has_external_reference_value_) {
      return reinterpret_cast<intptr_t>(external_reference_value_.address());
    } else {
      DCHECK(!object_.handle().is_null());
      return object_.Hashcode();
    }
  }

  virtual void FinalizeUniqueness() OVERRIDE {
    if (!has_double_value_ && !has_external_reference_value_) {
      DCHECK(!object_.handle().is_null());
      object_ = Unique<Object>(object_.handle());
    }
  }

  Unique<Object> GetUnique() const {
    return object_;
  }

  bool EqualsUnique(Unique<Object> other) const {
    return object_.IsInitialized() && object_ == other;
  }

  virtual bool DataEquals(HValue* other) OVERRIDE {
    HConstant* other_constant = HConstant::cast(other);
    if (has_int32_value_) {
      return other_constant->has_int32_value_ &&
          int32_value_ == other_constant->int32_value_;
    } else if (has_double_value_) {
      return other_constant->has_double_value_ &&
             bit_cast<int64_t>(double_value_) ==
                 bit_cast<int64_t>(other_constant->double_value_);
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
      DCHECK(!object_.handle().is_null());
      return other_constant->object_ == object_;
    }
  }

#ifdef DEBUG
  virtual void Verify() OVERRIDE { }
#endif

  DECLARE_CONCRETE_INSTRUCTION(Constant)

 protected:
  virtual Range* InferRange(Zone* zone) OVERRIDE;

 private:
  friend class HGraph;
  explicit HConstant(Handle<Object> handle,
                     Representation r = Representation::None());
  HConstant(int32_t value,
            Representation r = Representation::None(),
            bool is_not_in_new_space = true,
            Unique<Object> optional = Unique<Object>(Handle<Object>::null()));
  HConstant(double value,
            Representation r = Representation::None(),
            bool is_not_in_new_space = true,
            Unique<Object> optional = Unique<Object>(Handle<Object>::null()));
  HConstant(Unique<Object> object,
            Unique<Map> object_map,
            bool has_stable_map_value,
            Representation r,
            HType type,
            bool is_not_in_new_space,
            bool boolean_value,
            bool is_undetectable,
            InstanceType instance_type);

  explicit HConstant(ExternalReference reference);

  void Initialize(Representation r);

  virtual bool IsDeletable() const OVERRIDE { return true; }

  // If this is a numerical constant, object_ either points to the
  // HeapObject the constant originated from or is null.  If the
  // constant is non-numeric, object_ always points to a valid
  // constant HeapObject.
  Unique<Object> object_;

  // If object_ is a heap object, this points to the stable map of the object.
  Unique<Map> object_map_;

  // If object_ is a map, this indicates whether the map is stable.
  bool has_stable_map_value_ : 1;

  // We store the HConstant in the most specific form safely possible.
  // The two flags, has_int32_value_ and has_double_value_ tell us if
  // int32_value_ and double_value_ hold valid, safe representations
  // of the constant.  has_int32_value_ implies has_double_value_ but
  // not the converse.
  bool has_smi_value_ : 1;
  bool has_int32_value_ : 1;
  bool has_double_value_ : 1;
  bool has_external_reference_value_ : 1;
  bool is_not_in_new_space_ : 1;
  bool boolean_value_ : 1;
  bool is_undetectable_: 1;
  int32_t int32_value_;
  double double_value_;
  ExternalReference external_reference_value_;

  static const InstanceType kUnknownInstanceType = FILLER_TYPE;
  InstanceType instance_type_;
};


class HBinaryOperation : public HTemplateInstruction<3> {
 public:
  HBinaryOperation(HValue* context, HValue* left, HValue* right,
                   HType type = HType::Tagged())
      : HTemplateInstruction<3>(type),
        observed_output_representation_(Representation::None()) {
    DCHECK(left != NULL && right != NULL);
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
    return right()->HasOneUse();
  }

  HValue* BetterLeftOperand() {
    return AreOperandsBetterSwitched() ? right() : left();
  }

  HValue* BetterRightOperand() {
    return AreOperandsBetterSwitched() ? left() : right();
  }

  void set_observed_input_representation(int index, Representation rep) {
    DCHECK(index >= 1 && index <= 2);
    observed_input_representation_[index - 1] = rep;
  }

  virtual void initialize_output_representation(Representation observed) {
    observed_output_representation_ = observed;
  }

  virtual Representation observed_input_representation(int index) OVERRIDE {
    if (index == 0) return Representation::Tagged();
    return observed_input_representation_[index - 1];
  }

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) OVERRIDE {
    Representation rep = !FLAG_smi_binop && new_rep.IsSmi()
        ? Representation::Integer32() : new_rep;
    HValue::UpdateRepresentation(rep, h_infer, reason);
  }

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) OVERRIDE;
  virtual Representation RepresentationFromInputs() OVERRIDE;
  Representation RepresentationFromOutput();
  virtual void AssumeRepresentation(Representation r) OVERRIDE;

  virtual bool IsCommutative() const { return false; }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    if (index == 0) return Representation::Tagged();
    return representation();
  }

  void SetOperandPositions(Zone* zone,
                           HSourcePosition left_pos,
                           HSourcePosition right_pos) {
    set_operand_position(zone, 1, left_pos);
    set_operand_position(zone, 2, right_pos);
  }

  bool RightIsPowerOf2() {
    if (!right()->IsInteger32Constant()) return false;
    int32_t value = right()->GetInteger32Constant();
    if (value < 0) {
      return base::bits::IsPowerOfTwo32(static_cast<uint32_t>(-value));
    }
    return base::bits::IsPowerOfTwo32(static_cast<uint32_t>(value));
  }

  DECLARE_ABSTRACT_INSTRUCTION(BinaryOperation)

 private:
  bool IgnoreObservedOutputRepresentation(Representation current_rep);

  Representation observed_input_representation_[2];
  Representation observed_output_representation_;
};


class HWrapReceiver FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HWrapReceiver, HValue*, HValue*);

  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  HValue* receiver() const { return OperandAt(0); }
  HValue* function() const { return OperandAt(1); }

  virtual HValue* Canonicalize() OVERRIDE;

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT
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


class HApplyArguments FINAL : public HTemplateInstruction<4> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P4(HApplyArguments, HValue*, HValue*, HValue*,
                                 HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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


class HArgumentsElements FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HArgumentsElements, bool);

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements)

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  bool from_inlined() const { return from_inlined_; }

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  explicit HArgumentsElements(bool from_inlined) : from_inlined_(from_inlined) {
    // The value produced by this instruction is a pointer into the stack
    // that looks as if it was a smi because of alignment.
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }

  bool from_inlined_;
};


class HArgumentsLength FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HArgumentsLength, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  explicit HArgumentsLength(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HAccessArgumentsAt FINAL : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HAccessArgumentsAt, HValue*, HValue*, HValue*);

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    // The arguments elements is considered tagged.
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }

  HValue* arguments() const { return OperandAt(0); }
  HValue* length() const { return OperandAt(1); }
  HValue* index() const { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(AccessArgumentsAt)

 private:
  HAccessArgumentsAt(HValue* arguments, HValue* length, HValue* index) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetOperandAt(0, arguments);
    SetOperandAt(1, length);
    SetOperandAt(2, index);
  }

  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }
};


class HBoundsCheckBaseIndexInformation;


class HBoundsCheck FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HBoundsCheck, HValue*, HValue*);

  bool skip_check() const { return skip_check_; }
  void set_skip_check() { skip_check_ = true; }

  HValue* base() const { return base_; }
  int offset() const { return offset_; }
  int scale() const { return scale_; }

  void ApplyIndexChange();
  bool DetectCompoundIndex() {
    DCHECK(base() == NULL);

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

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return representation();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT
  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) OVERRIDE;

  HValue* index() const { return OperandAt(0); }
  HValue* length() const { return OperandAt(1); }
  bool allow_equality() const { return allow_equality_; }
  void set_allow_equality(bool v) { allow_equality_ = v; }

  virtual int RedefinedOperandIndex() OVERRIDE { return 0; }
  virtual bool IsPurelyInformativeDefinition() OVERRIDE {
    return skip_check();
  }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheck)

 protected:
  friend class HBoundsCheckBaseIndexInformation;

  virtual Range* InferRange(Zone* zone) OVERRIDE;

  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }
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

  virtual bool IsDeletable() const OVERRIDE {
    return skip_check() && !FLAG_debug_code;
  }
};


class HBoundsCheckBaseIndexInformation FINAL
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

  HValue* base_index() const { return OperandAt(0); }
  HBoundsCheck* bounds_check() { return HBoundsCheck::cast(OperandAt(1)); }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheckBaseIndexInformation)

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return representation();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual int RedefinedOperandIndex() OVERRIDE { return 0; }
  virtual bool IsPurelyInformativeDefinition() OVERRIDE { return true; }
};


class HBitwiseBinaryOperation : public HBinaryOperation {
 public:
  HBitwiseBinaryOperation(HValue* context, HValue* left, HValue* right,
                          HType type = HType::TaggedNumber())
      : HBinaryOperation(context, left, right, type) {
    SetFlag(kFlexibleRepresentation);
    SetFlag(kTruncatingToInt32);
    SetFlag(kAllowUndefinedAsNaN);
    SetAllSideEffects();
  }

  virtual void RepresentationChanged(Representation to) OVERRIDE {
    if (to.IsTagged() &&
        (left()->ToNumberCanBeObserved() || right()->ToNumberCanBeObserved())) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
    if (to.IsTagged()) SetChangesFlag(kNewSpacePromotion);
  }

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) OVERRIDE {
    // We only generate either int32 or generic tagged bitwise operations.
    if (new_rep.IsDouble()) new_rep = Representation::Integer32();
    HBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  virtual Representation observed_input_representation(int index) OVERRIDE {
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
  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HMathFloorOfDiv FINAL : public HBinaryOperation {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HMathFloorOfDiv,
                                              HValue*,
                                              HValue*);

  DECLARE_CONCRETE_INSTRUCTION(MathFloorOfDiv)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  HMathFloorOfDiv(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetFlag(kCanOverflow);
    SetFlag(kCanBeDivByZero);
    SetFlag(kLeftCanBeMinInt);
    SetFlag(kLeftCanBeNegative);
    SetFlag(kLeftCanBePositive);
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HArithmeticBinaryOperation : public HBinaryOperation {
 public:
  HArithmeticBinaryOperation(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right, HType::TaggedNumber()) {
    SetAllSideEffects();
    SetFlag(kFlexibleRepresentation);
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual void RepresentationChanged(Representation to) OVERRIDE {
    if (to.IsTagged() &&
        (left()->ToNumberCanBeObserved() || right()->ToNumberCanBeObserved())) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
    if (to.IsTagged()) SetChangesFlag(kNewSpacePromotion);
  }

  DECLARE_ABSTRACT_INSTRUCTION(ArithmeticBinaryOperation)

 private:
  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HCompareGeneric FINAL : public HBinaryOperation {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HCompareGeneric, HValue*,
                                              HValue*, Token::Value);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return index == 0
        ? Representation::Tagged()
        : representation();
  }

  Token::Value token() const { return token_; }
  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(CompareGeneric)

 private:
  HCompareGeneric(HValue* context,
                  HValue* left,
                  HValue* right,
                  Token::Value token)
      : HBinaryOperation(context, left, right, HType::Boolean()),
        token_(token) {
    DCHECK(Token::IsCompareOp(token));
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

  HValue* left() const { return OperandAt(0); }
  HValue* right() const { return OperandAt(1); }
  Token::Value token() const { return token_; }

  void set_observed_input_representation(Representation left,
                                         Representation right) {
      observed_input_representation_[0] = left;
      observed_input_representation_[1] = right;
  }

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return representation();
  }
  virtual Representation observed_input_representation(int index) OVERRIDE {
    return observed_input_representation_[index];
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE;

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

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
    DCHECK(Token::IsCompareOp(token));
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    SetSuccessorAt(0, true_target);
    SetSuccessorAt(1, false_target);
  }

  Representation observed_input_representation_[2];
  Token::Value token_;
};


class HCompareHoleAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCompareHoleAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HCompareHoleAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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


class HCompareMinusZeroAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCompareMinusZeroAndBranch, HValue*);

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return representation();
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CompareMinusZeroAndBranch)

 private:
  explicit HCompareMinusZeroAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) {
  }
};


class HCompareObjectEqAndBranch : public HTemplateControlInstruction<2, 2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HCompareObjectEqAndBranch, HValue*, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P4(HCompareObjectEqAndBranch, HValue*, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE;

  static const int kNoKnownSuccessorIndex = -1;
  int known_successor_index() const { return known_successor_index_; }
  void set_known_successor_index(int known_successor_index) {
    known_successor_index_ = known_successor_index;
  }

  HValue* left() const { return OperandAt(0); }
  HValue* right() const { return OperandAt(1); }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual Representation observed_input_representation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareObjectEqAndBranch)

 private:
  HCompareObjectEqAndBranch(HValue* left,
                            HValue* right,
                            HBasicBlock* true_target = NULL,
                            HBasicBlock* false_target = NULL)
      : known_successor_index_(kNoKnownSuccessorIndex) {
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    SetSuccessorAt(0, true_target);
    SetSuccessorAt(1, false_target);
  }

  int known_successor_index_;
};


class HIsObjectAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsObjectAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsObjectAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(IsObjectAndBranch)

 private:
  HIsObjectAndBranch(HValue* value,
                     HBasicBlock* true_target = NULL,
                     HBasicBlock* false_target = NULL)
    : HUnaryControlInstruction(value, true_target, false_target) {}
};


class HIsStringAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsStringAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsStringAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE;

  static const int kNoKnownSuccessorIndex = -1;
  int known_successor_index() const { return known_successor_index_; }
  void set_known_successor_index(int known_successor_index) {
    known_successor_index_ = known_successor_index;
  }

  DECLARE_CONCRETE_INSTRUCTION(IsStringAndBranch)

 protected:
  virtual int RedefinedOperandIndex() { return 0; }

 private:
  HIsStringAndBranch(HValue* value,
                     HBasicBlock* true_target = NULL,
                     HBasicBlock* false_target = NULL)
    : HUnaryControlInstruction(value, true_target, false_target),
      known_successor_index_(kNoKnownSuccessorIndex) { }

  int known_successor_index_;
};


class HIsSmiAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsSmiAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsSmiAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch)

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }
  virtual int RedefinedOperandIndex() { return 0; }

 private:
  HIsSmiAndBranch(HValue* value,
                  HBasicBlock* true_target = NULL,
                  HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target) {
    set_representation(Representation::Tagged());
  }
};


class HIsUndetectableAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsUndetectableAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsUndetectableAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE;

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

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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
    DCHECK(Token::IsCompareOp(token));
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

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsConstructCallAndBranch)
 private:
  HIsConstructCallAndBranch() {}
};


class HHasInstanceTypeAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(
      HHasInstanceTypeAndBranch, HValue*, InstanceType);
  DECLARE_INSTRUCTION_FACTORY_P3(
      HHasInstanceTypeAndBranch, HValue*, InstanceType, InstanceType);

  InstanceType from() { return from_; }
  InstanceType to() { return to_; }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceTypeAndBranch)

 private:
  HHasInstanceTypeAndBranch(HValue* value, InstanceType type)
      : HUnaryControlInstruction(value, NULL, NULL), from_(type), to_(type) { }
  HHasInstanceTypeAndBranch(HValue* value, InstanceType from, InstanceType to)
      : HUnaryControlInstruction(value, NULL, NULL), from_(from), to_(to) {
    DCHECK(to == LAST_TYPE);  // Others not implemented yet in backend.
  }

  InstanceType from_;
  InstanceType to_;  // Inclusive range, not all combinations work.
};


class HHasCachedArrayIndexAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HHasCachedArrayIndexAndBranch, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndexAndBranch)
 private:
  explicit HHasCachedArrayIndexAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }
};


class HGetCachedArrayIndex FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HGetCachedArrayIndex, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(GetCachedArrayIndex)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  explicit HGetCachedArrayIndex(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HClassOfTestAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HClassOfTestAndBranch, HValue*,
                                 Handle<String>);

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch)

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  Handle<String> class_name() const { return class_name_; }

 private:
  HClassOfTestAndBranch(HValue* value, Handle<String> class_name)
      : HUnaryControlInstruction(value, NULL, NULL),
        class_name_(class_name) { }

  Handle<String> class_name_;
};


class HTypeofIsAndBranch FINAL : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HTypeofIsAndBranch, HValue*, Handle<String>);

  Handle<String> type_literal() const { return type_literal_.handle(); }
  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(TypeofIsAndBranch)

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  virtual bool KnownSuccessorBlock(HBasicBlock** block) OVERRIDE;

  virtual void FinalizeUniqueness() OVERRIDE {
    type_literal_ = Unique<String>(type_literal_.handle());
  }

 private:
  HTypeofIsAndBranch(HValue* value, Handle<String> type_literal)
      : HUnaryControlInstruction(value, NULL, NULL),
        type_literal_(Unique<String>::CreateUninitialized(type_literal)) { }

  Unique<String> type_literal_;
};


class HInstanceOf FINAL : public HBinaryOperation {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HInstanceOf, HValue*, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(InstanceOf)

 private:
  HInstanceOf(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right, HType::Boolean()) {
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }
};


class HInstanceOfKnownGlobal FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HInstanceOfKnownGlobal,
                                              HValue*,
                                              Handle<JSFunction>);

  HValue* context() { return OperandAt(0); }
  HValue* left() { return OperandAt(1); }
  Handle<JSFunction> function() { return function_; }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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


class HPower FINAL : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  HValue* left() { return OperandAt(0); }
  HValue* right() const { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return index == 0
      ? Representation::Double()
      : Representation::None();
  }
  virtual Representation observed_input_representation(int index) OVERRIDE {
    return RequiredInputRepresentation(index);
  }

  DECLARE_CONCRETE_INSTRUCTION(Power)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  HPower(HValue* left, HValue* right) {
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    set_representation(Representation::Double());
    SetFlag(kUseGVN);
    SetChangesFlag(kNewSpacePromotion);
  }

  virtual bool IsDeletable() const OVERRIDE {
    return !right()->representation().IsTagged();
  }
};


class HAdd FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  // Add is only commutative if two integer values are added and not if two
  // tagged values are added (because it might be a String concatenation).
  // We also do not commute (pointer + offset).
  virtual bool IsCommutative() const OVERRIDE {
    return !representation().IsTagged() && !representation().IsExternal();
  }

  virtual HValue* Canonicalize() OVERRIDE;

  virtual bool TryDecompose(DecompositionResult* decomposition) OVERRIDE {
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

  virtual void RepresentationChanged(Representation to) OVERRIDE {
    if (to.IsTagged() &&
        (left()->ToNumberCanBeObserved() || right()->ToNumberCanBeObserved() ||
         left()->ToStringCanBeObserved() || right()->ToStringCanBeObserved())) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
    if (to.IsTagged()) {
      SetChangesFlag(kNewSpacePromotion);
      ClearFlag(kAllowUndefinedAsNaN);
    }
  }

  virtual Representation RepresentationFromInputs() OVERRIDE;

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(Add)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

 private:
  HAdd(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HSub FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual HValue* Canonicalize() OVERRIDE;

  virtual bool TryDecompose(DecompositionResult* decomposition) OVERRIDE {
    if (right()->IsInteger32Constant()) {
      decomposition->Apply(left(), -right()->GetInteger32Constant());
      return true;
    } else {
      return false;
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(Sub)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

 private:
  HSub(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HMul FINAL : public HArithmeticBinaryOperation {
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

  virtual HValue* Canonicalize() OVERRIDE;

  // Only commutative if it is certain that not two objects are multiplicated.
  virtual bool IsCommutative() const OVERRIDE {
    return !representation().IsTagged();
  }

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) OVERRIDE {
    HArithmeticBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  bool MulMinusOne();

  DECLARE_CONCRETE_INSTRUCTION(Mul)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

 private:
  HMul(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HMod FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual HValue* Canonicalize() OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HArithmeticBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Mod)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

 private:
  HMod(HValue* context,
       HValue* left,
       HValue* right) : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
    SetFlag(kLeftCanBeNegative);
  }
};


class HDiv FINAL : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual HValue* Canonicalize() OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HArithmeticBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Div)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

 private:
  HDiv(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
  }
};


class HMathMinMax FINAL : public HArithmeticBinaryOperation {
 public:
  enum Operation { kMathMin, kMathMax };

  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right,
                           Operation op);

  virtual Representation observed_input_representation(int index) OVERRIDE {
    return RequiredInputRepresentation(index);
  }

  virtual void InferRepresentation(
      HInferRepresentationPhase* h_infer) OVERRIDE;

  virtual Representation RepresentationFromInputs() OVERRIDE {
    Representation left_rep = left()->representation();
    Representation right_rep = right()->representation();
    Representation result = Representation::Smi();
    result = result.generalize(left_rep);
    result = result.generalize(right_rep);
    if (result.IsTagged()) return Representation::Double();
    return result;
  }

  virtual bool IsCommutative() const OVERRIDE { return true; }

  Operation operation() { return operation_; }

  DECLARE_CONCRETE_INSTRUCTION(MathMinMax)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    return other->IsMathMinMax() &&
        HMathMinMax::cast(other)->operation_ == operation_;
  }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

 private:
  HMathMinMax(HValue* context, HValue* left, HValue* right, Operation op)
      : HArithmeticBinaryOperation(context, left, right),
        operation_(op) { }

  Operation operation_;
};


class HBitwise FINAL : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           Token::Value op,
                           HValue* left,
                           HValue* right);

  Token::Value op() const { return op_; }

  virtual bool IsCommutative() const OVERRIDE { return true; }

  virtual HValue* Canonicalize() OVERRIDE;

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(Bitwise)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    return op() == HBitwise::cast(other)->op();
  }

  virtual Range* InferRange(Zone* zone) OVERRIDE;

 private:
  HBitwise(HValue* context,
           Token::Value op,
           HValue* left,
           HValue* right)
      : HBitwiseBinaryOperation(context, left, right),
        op_(op) {
    DCHECK(op == Token::BIT_AND || op == Token::BIT_OR || op == Token::BIT_XOR);
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


class HShl FINAL : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual Range* InferRange(Zone* zone) OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) OVERRIDE {
    if (new_rep.IsSmi() &&
        !(right()->IsInteger32Constant() &&
          right()->GetInteger32Constant() >= 0)) {
      new_rep = Representation::Integer32();
    }
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Shl)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  HShl(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }
};


class HShr FINAL : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual bool TryDecompose(DecompositionResult* decomposition) OVERRIDE {
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

  virtual Range* InferRange(Zone* zone) OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Shr)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  HShr(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }
};


class HSar FINAL : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual bool TryDecompose(DecompositionResult* decomposition) OVERRIDE {
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

  virtual Range* InferRange(Zone* zone) OVERRIDE;

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Sar)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  HSar(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }
};


class HRor FINAL : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right) {
    return new(zone) HRor(context, left, right);
  }

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) OVERRIDE {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Ror)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

 private:
  HRor(HValue* context, HValue* left, HValue* right)
       : HBitwiseBinaryOperation(context, left, right) {
    ChangeRepresentation(Representation::Integer32());
  }
};


class HOsrEntry FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HOsrEntry, BailoutId);

  BailoutId ast_id() const { return ast_id_; }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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


class HParameter FINAL : public HTemplateInstruction<0> {
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

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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


class HCallStub FINAL : public HUnaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HCallStub, CodeStub::Major, int);
  CodeStub::Major major_key() { return major_key_; }

  HValue* context() { return value(); }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(CallStub)

 private:
  HCallStub(HValue* context, CodeStub::Major major_key, int argument_count)
      : HUnaryCall(context, argument_count),
        major_key_(major_key) {
  }

  CodeStub::Major major_key_;
};


class HTailCallThroughMegamorphicCache FINAL : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HTailCallThroughMegamorphicCache,
                                              HValue*, HValue*, Code::Flags);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* receiver() const { return OperandAt(1); }
  HValue* name() const { return OperandAt(2); }
  Code::Flags flags() const { return flags_; }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(TailCallThroughMegamorphicCache)

 private:
  HTailCallThroughMegamorphicCache(HValue* context, HValue* receiver,
                                   HValue* name, Code::Flags flags)
      : flags_(flags) {
    SetOperandAt(0, context);
    SetOperandAt(1, receiver);
    SetOperandAt(2, name);
  }

  Code::Flags flags_;
};


class HUnknownOSRValue FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HUnknownOSRValue, HEnvironment*, int);

  virtual OStream& PrintDataTo(OStream& os) const;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  void set_incoming_value(HPhi* value) { incoming_value_ = value; }
  HPhi* incoming_value() { return incoming_value_; }
  HEnvironment *environment() { return environment_; }
  int index() { return index_; }

  virtual Representation KnownOptimalRepresentation() OVERRIDE {
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


class HLoadGlobalCell FINAL : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HLoadGlobalCell, Handle<Cell>,
                                 PropertyDetails);

  Unique<Cell> cell() const { return cell_; }
  bool RequiresHoleCheck() const;

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual intptr_t Hashcode() OVERRIDE {
    return cell_.Hashcode();
  }

  virtual void FinalizeUniqueness() OVERRIDE {
    cell_ = Unique<Cell>(cell_.handle());
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalCell)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    return cell_ == HLoadGlobalCell::cast(other)->cell_;
  }

 private:
  HLoadGlobalCell(Handle<Cell> cell, PropertyDetails details)
    : cell_(Unique<Cell>::CreateUninitialized(cell)), details_(details) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kGlobalVars);
  }

  virtual bool IsDeletable() const OVERRIDE { return !RequiresHoleCheck(); }

  Unique<Cell> cell_;
  PropertyDetails details_;
};


class HLoadGlobalGeneric FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HLoadGlobalGeneric, HValue*,
                                              Handle<String>, bool);

  HValue* context() { return OperandAt(0); }
  HValue* global_object() { return OperandAt(1); }
  Handle<String> name() const { return name_; }
  bool for_typeof() const { return for_typeof_; }
  int slot() const {
    DCHECK(FLAG_vector_ics &&
           slot_ != FeedbackSlotInterface::kInvalidFeedbackSlot);
    return slot_;
  }
  Handle<FixedArray> feedback_vector() const { return feedback_vector_; }
  void SetVectorAndSlot(Handle<FixedArray> vector, int slot) {
    DCHECK(FLAG_vector_ics);
    feedback_vector_ = vector;
    slot_ = slot;
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalGeneric)

 private:
  HLoadGlobalGeneric(HValue* context, HValue* global_object,
                     Handle<String> name, bool for_typeof)
      : name_(name), for_typeof_(for_typeof),
        slot_(FeedbackSlotInterface::kInvalidFeedbackSlot) {
    SetOperandAt(0, context);
    SetOperandAt(1, global_object);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  Handle<String> name_;
  bool for_typeof_;
  Handle<FixedArray> feedback_vector_;
  int slot_;
};


class HAllocate FINAL : public HTemplateInstruction<2> {
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

  HValue* context() const { return OperandAt(0); }
  HValue* size() const { return OperandAt(1); }

  bool has_size_upper_bound() { return size_upper_bound_ != NULL; }
  HConstant* size_upper_bound() { return size_upper_bound_; }
  void set_size_upper_bound(HConstant* value) {
    DCHECK(size_upper_bound_ == NULL);
    size_upper_bound_ = value;
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      return Representation::Integer32();
    }
  }

  virtual Handle<Map> GetMonomorphicJSObjectMap() OVERRIDE {
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
                                         HValue* dominator) OVERRIDE;

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

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
        filler_free_space_size_(NULL),
        size_upper_bound_(NULL) {
    SetOperandAt(0, context);
    UpdateSize(size);
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
    if (size->IsInteger32Constant()) {
      size_upper_bound_ = HConstant::cast(size);
    } else {
      size_upper_bound_ = NULL;
    }
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
  HConstant* size_upper_bound_;
};


class HStoreCodeEntry FINAL: public HTemplateInstruction<2> {
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


class HInnerAllocatedObject FINAL : public HTemplateInstruction<2> {
 public:
  static HInnerAllocatedObject* New(Zone* zone,
                                    HValue* context,
                                    HValue* value,
                                    HValue* offset,
                                    HType type) {
    return new(zone) HInnerAllocatedObject(value, offset, type);
  }

  HValue* base_object() const { return OperandAt(0); }
  HValue* offset() const { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return index == 0 ? Representation::Tagged() : Representation::Integer32();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(InnerAllocatedObject)

 private:
  HInnerAllocatedObject(HValue* value,
                        HValue* offset,
                        HType type) : HTemplateInstruction<2>(type) {
    DCHECK(value->IsAllocate());
    DCHECK(type.IsHeapObject());
    SetOperandAt(0, value);
    SetOperandAt(1, offset);
    set_representation(Representation::Tagged());
  }
};


inline bool StoringValueNeedsWriteBarrier(HValue* value) {
  return !value->type().IsSmi()
      && !value->type().IsNull()
      && !value->type().IsBoolean()
      && !value->type().IsUndefined()
      && !(value->IsConstant() && HConstant::cast(value)->ImmortalImmovable());
}


inline bool ReceiverObjectNeedsWriteBarrier(HValue* object,
                                            HValue* value,
                                            HValue* dominator) {
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
  // We definitely need a write barrier unless the object is the allocation
  // dominator.
  if (object == dominator && object->IsAllocate()) {
    // Stores to new space allocations require no write barriers.
    if (HAllocate::cast(object)->IsNewSpaceAllocation()) {
      return false;
    }
    // Stores to old space allocations require no write barriers if the value is
    // a constant provably not in new space.
    if (value->IsConstant() && HConstant::cast(value)->NotInNewSpace()) {
      return false;
    }
    // Stores to old space allocations require no write barriers if the value is
    // an old space allocation.
    while (value->IsInnerAllocatedObject()) {
      value = HInnerAllocatedObject::cast(value)->base_object();
    }
    if (value->IsAllocate() &&
        !HAllocate::cast(value)->IsNewSpaceAllocation()) {
      return false;
    }
  }
  return true;
}


inline PointersToHereCheck PointersToHereCheckForObject(HValue* object,
                                                        HValue* dominator) {
  while (object->IsInnerAllocatedObject()) {
    object = HInnerAllocatedObject::cast(object)->base_object();
  }
  if (object == dominator &&
      object->IsAllocate() &&
      HAllocate::cast(object)->IsNewSpaceAllocation()) {
    return kPointersToHereAreAlwaysInteresting;
  }
  return kPointersToHereMaybeInteresting;
}


class HStoreGlobalCell FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HStoreGlobalCell, HValue*,
                                 Handle<PropertyCell>, PropertyDetails);

  Unique<PropertyCell> cell() const { return cell_; }
  bool RequiresHoleCheck() { return details_.IsConfigurable(); }
  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

  virtual void FinalizeUniqueness() OVERRIDE {
    cell_ = Unique<PropertyCell>(cell_.handle());
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }
  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

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


class HLoadContextSlot FINAL : public HUnaryOperation {
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

  HLoadContextSlot(HValue* context, int slot_index, Mode mode)
      : HUnaryOperation(context), slot_index_(slot_index), mode_(mode) {
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

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(LoadContextSlot)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    HLoadContextSlot* b = HLoadContextSlot::cast(other);
    return (slot_index() == b->slot_index());
  }

 private:
  virtual bool IsDeletable() const OVERRIDE { return !RequiresHoleCheck(); }

  int slot_index_;
  Mode mode_;
};


class HStoreContextSlot FINAL : public HTemplateInstruction<2> {
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

  HValue* context() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }
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

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

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
class HObjectAccess FINAL {
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

  inline bool IsMap() const {
    return portion() == kMaps;
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
        IsFastElementsKind(elements_kind)
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
        Representation::Smi());
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
        Representation::Smi());
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

  static HObjectAccess ForMapAsInteger32() {
    return HObjectAccess(kMaps, JSObject::kMapOffset,
                         Representation::Integer32());
  }

  static HObjectAccess ForMapInObjectProperties() {
    return HObjectAccess(kInobject,
                         Map::kInObjectPropertiesOffset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForMapInstanceType() {
    return HObjectAccess(kInobject,
                         Map::kInstanceTypeOffset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForMapInstanceSize() {
    return HObjectAccess(kInobject,
                         Map::kInstanceSizeOffset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForMapBitField() {
    return HObjectAccess(kInobject,
                         Map::kBitFieldOffset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForMapBitField2() {
    return HObjectAccess(kInobject,
                         Map::kBitField2Offset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForNameHashField() {
    return HObjectAccess(kInobject,
                         Name::kHashFieldOffset,
                         Representation::Integer32());
  }

  static HObjectAccess ForMapInstanceTypeAndBitField() {
    STATIC_ASSERT((Map::kInstanceTypeAndBitFieldOffset & 1) == 0);
    // Ensure the two fields share one 16-bit word, endian-independent.
    STATIC_ASSERT((Map::kBitFieldOffset & ~1) ==
                  (Map::kInstanceTypeOffset & ~1));
    return HObjectAccess(kInobject,
                         Map::kInstanceTypeAndBitFieldOffset,
                         Representation::UInteger16());
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

  static HObjectAccess ForExternalUInteger8() {
    return HObjectAccess(kExternalMemory, 0, Representation::UInteger8(),
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
  static HObjectAccess ForField(Handle<Map> map, int index,
                                Representation representation,
                                Handle<String> name);

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

  static HObjectAccess ForJSArrayBufferByteLength() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBuffer::kByteLengthOffset, Representation::Tagged());
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
    DCHECK(this->offset() == offset);
    DCHECK(this->portion() == portion);
    DCHECK(this->immutable() == immutable);
    DCHECK(this->existing_inobject_property() == existing_inobject_property);
    DCHECK(RepresentationField::decode(value_) == representation.kind());
    DCHECK(!this->existing_inobject_property() || IsInobject());
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
  friend OStream& operator<<(OStream& os, const HObjectAccess& access);

  inline Portion portion() const {
    return PortionField::decode(value_);
  }
};


OStream& operator<<(OStream& os, const HObjectAccess& access);


class HLoadNamedField FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HLoadNamedField, HValue*,
                                 HValue*, HObjectAccess);
  DECLARE_INSTRUCTION_FACTORY_P5(HLoadNamedField, HValue*, HValue*,
                                 HObjectAccess, const UniqueSet<Map>*, HType);

  HValue* object() const { return OperandAt(0); }
  HValue* dependency() const {
    DCHECK(HasDependency());
    return OperandAt(1);
  }
  bool HasDependency() const { return OperandAt(0) != OperandAt(1); }
  HObjectAccess access() const { return access_; }
  Representation field_representation() const {
      return access_.representation();
  }

  const UniqueSet<Map>* maps() const { return maps_; }

  virtual bool HasEscapingOperandAt(int index) OVERRIDE { return false; }
  virtual bool HasOutOfBoundsAccess(int size) OVERRIDE {
    return !access().IsInobject() || access().offset() >= size;
  }
  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    if (index == 0 && access().IsExternalMemory()) {
      // object must be external in case of external memory access
      return Representation::External();
    }
    return Representation::Tagged();
  }
  virtual Range* InferRange(Zone* zone) OVERRIDE;
  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  bool CanBeReplacedWith(HValue* other) const {
    if (!CheckFlag(HValue::kCantBeReplaced)) return false;
    if (!type().Equals(other->type())) return false;
    if (!representation().Equals(other->representation())) return false;
    if (!other->IsLoadNamedField()) return true;
    HLoadNamedField* that = HLoadNamedField::cast(other);
    if (this->maps_ == that->maps_) return true;
    if (this->maps_ == NULL || that->maps_ == NULL) return false;
    return this->maps_->IsSubset(that->maps_);
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    HLoadNamedField* that = HLoadNamedField::cast(other);
    if (!this->access_.Equals(that->access_)) return false;
    if (this->maps_ == that->maps_) return true;
    return (this->maps_ != NULL &&
            that->maps_ != NULL &&
            this->maps_->Equals(that->maps_));
  }

 private:
  HLoadNamedField(HValue* object,
                  HValue* dependency,
                  HObjectAccess access)
      : access_(access), maps_(NULL) {
    DCHECK_NOT_NULL(object);
    SetOperandAt(0, object);
    SetOperandAt(1, dependency ? dependency : object);

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
    } else if (representation.IsHeapObject()) {
      set_type(HType::HeapObject());
      set_representation(Representation::Tagged());
    } else {
      set_representation(Representation::Tagged());
    }
    access.SetGVNFlags(this, LOAD);
  }

  HLoadNamedField(HValue* object,
                  HValue* dependency,
                  HObjectAccess access,
                  const UniqueSet<Map>* maps,
                  HType type)
      : HTemplateInstruction<2>(type), access_(access), maps_(maps) {
    DCHECK_NOT_NULL(maps);
    DCHECK_NE(0, maps->size());

    DCHECK_NOT_NULL(object);
    SetOperandAt(0, object);
    SetOperandAt(1, dependency ? dependency : object);

    DCHECK(access.representation().IsHeapObject());
    DCHECK(type.IsHeapObject());
    set_representation(Representation::Tagged());

    access.SetGVNFlags(this, LOAD);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }

  HObjectAccess access_;
  const UniqueSet<Map>* maps_;
};


class HLoadNamedGeneric FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HLoadNamedGeneric, HValue*,
                                              Handle<Object>);

  HValue* context() const { return OperandAt(0); }
  HValue* object() const { return OperandAt(1); }
  Handle<Object> name() const { return name_; }

  int slot() const {
    DCHECK(FLAG_vector_ics &&
           slot_ != FeedbackSlotInterface::kInvalidFeedbackSlot);
    return slot_;
  }
  Handle<FixedArray> feedback_vector() const { return feedback_vector_; }
  void SetVectorAndSlot(Handle<FixedArray> vector, int slot) {
    DCHECK(FLAG_vector_ics);
    feedback_vector_ = vector;
    slot_ = slot;
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedGeneric)

 private:
  HLoadNamedGeneric(HValue* context, HValue* object, Handle<Object> name)
      : name_(name),
        slot_(FeedbackSlotInterface::kInvalidFeedbackSlot) {
    SetOperandAt(0, context);
    SetOperandAt(1, object);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  Handle<Object> name_;
  Handle<FixedArray> feedback_vector_;
  int slot_;
};


class HLoadFunctionPrototype FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HLoadFunctionPrototype, HValue*);

  HValue* function() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

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
  virtual ElementsKind elements_kind() const = 0;
  // TryIncreaseBaseOffset returns false if overflow would result.
  virtual bool TryIncreaseBaseOffset(uint32_t increase_by_value) = 0;
  virtual bool IsDehoisted() const = 0;
  virtual void SetDehoisted(bool is_dehoisted) = 0;
  virtual ~ArrayInstructionInterface() { }

  static Representation KeyedAccessIndexRequirement(Representation r) {
    return r.IsInteger32() || SmiValuesAre32Bits()
        ? Representation::Integer32() : Representation::Smi();
  }
};


static const int kDefaultKeyedHeaderOffsetSentinel = -1;

enum LoadKeyedHoleMode {
  NEVER_RETURN_HOLE,
  ALLOW_RETURN_HOLE
};


class HLoadKeyed FINAL
    : public HTemplateInstruction<3>, public ArrayInstructionInterface {
 public:
  DECLARE_INSTRUCTION_FACTORY_P4(HLoadKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind);
  DECLARE_INSTRUCTION_FACTORY_P5(HLoadKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind, LoadKeyedHoleMode);
  DECLARE_INSTRUCTION_FACTORY_P6(HLoadKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind, LoadKeyedHoleMode, int);

  bool is_external() const {
    return IsExternalArrayElementsKind(elements_kind());
  }
  bool is_fixed_typed_array() const {
    return IsFixedTypedArrayElementsKind(elements_kind());
  }
  bool is_typed_elements() const {
    return is_external() || is_fixed_typed_array();
  }
  HValue* elements() const { return OperandAt(0); }
  HValue* key() const { return OperandAt(1); }
  HValue* dependency() const {
    DCHECK(HasDependency());
    return OperandAt(2);
  }
  bool HasDependency() const { return OperandAt(0) != OperandAt(2); }
  uint32_t base_offset() const { return BaseOffsetField::decode(bit_field_); }
  bool TryIncreaseBaseOffset(uint32_t increase_by_value);
  HValue* GetKey() { return key(); }
  void SetKey(HValue* key) { SetOperandAt(1, key); }
  bool IsDehoisted() const { return IsDehoistedField::decode(bit_field_); }
  void SetDehoisted(bool is_dehoisted) {
    bit_field_ = IsDehoistedField::update(bit_field_, is_dehoisted);
  }
  virtual ElementsKind elements_kind() const OVERRIDE {
    return ElementsKindField::decode(bit_field_);
  }
  LoadKeyedHoleMode hole_mode() const {
    return HoleModeField::decode(bit_field_);
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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

  virtual Representation observed_input_representation(int index) OVERRIDE {
    return RequiredInputRepresentation(index);
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  bool UsesMustHandleHole() const;
  bool AllUsesCanTreatHoleAsNaN() const;
  bool RequiresHoleCheck() const;

  virtual Range* InferRange(Zone* zone) OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyed)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    if (!other->IsLoadKeyed()) return false;
    HLoadKeyed* other_load = HLoadKeyed::cast(other);

    if (IsDehoisted() && base_offset() != other_load->base_offset())
      return false;
    return elements_kind() == other_load->elements_kind();
  }

 private:
  HLoadKeyed(HValue* obj,
             HValue* key,
             HValue* dependency,
             ElementsKind elements_kind,
             LoadKeyedHoleMode mode = NEVER_RETURN_HOLE,
             int offset = kDefaultKeyedHeaderOffsetSentinel)
      : bit_field_(0) {
    offset = offset == kDefaultKeyedHeaderOffsetSentinel
        ? GetDefaultHeaderSizeForElementsKind(elements_kind)
        : offset;
    bit_field_ = ElementsKindField::encode(elements_kind) |
        HoleModeField::encode(mode) |
        BaseOffsetField::encode(offset);

    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, dependency != NULL ? dependency : obj);

    if (!is_typed_elements()) {
      // I can detect the case between storing double (holey and fast) and
      // smi/object by looking at elements_kind_.
      DCHECK(IsFastSmiOrObjectElementsKind(elements_kind) ||
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

  virtual bool IsDeletable() const OVERRIDE {
    return !RequiresHoleCheck();
  }

  // Establish some checks around our packed fields
  enum LoadKeyedBits {
    kBitsForElementsKind = 5,
    kBitsForHoleMode = 1,
    kBitsForBaseOffset = 25,
    kBitsForIsDehoisted = 1,

    kStartElementsKind = 0,
    kStartHoleMode = kStartElementsKind + kBitsForElementsKind,
    kStartBaseOffset = kStartHoleMode + kBitsForHoleMode,
    kStartIsDehoisted = kStartBaseOffset + kBitsForBaseOffset
  };

  STATIC_ASSERT((kBitsForElementsKind + kBitsForBaseOffset +
                 kBitsForIsDehoisted) <= sizeof(uint32_t)*8);
  STATIC_ASSERT(kElementsKindCount <= (1 << kBitsForElementsKind));
  class ElementsKindField:
    public BitField<ElementsKind, kStartElementsKind, kBitsForElementsKind>
    {};  // NOLINT
  class HoleModeField:
    public BitField<LoadKeyedHoleMode, kStartHoleMode, kBitsForHoleMode>
    {};  // NOLINT
  class BaseOffsetField:
    public BitField<uint32_t, kStartBaseOffset, kBitsForBaseOffset>
    {};  // NOLINT
  class IsDehoistedField:
    public BitField<bool, kStartIsDehoisted, kBitsForIsDehoisted>
    {};  // NOLINT
  uint32_t bit_field_;
};


class HLoadKeyedGeneric FINAL : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HLoadKeyedGeneric, HValue*,
                                              HValue*);
  HValue* object() const { return OperandAt(0); }
  HValue* key() const { return OperandAt(1); }
  HValue* context() const { return OperandAt(2); }
  int slot() const {
    DCHECK(FLAG_vector_ics &&
           slot_ != FeedbackSlotInterface::kInvalidFeedbackSlot);
    return slot_;
  }
  Handle<FixedArray> feedback_vector() const { return feedback_vector_; }
  void SetVectorAndSlot(Handle<FixedArray> vector, int slot) {
    DCHECK(FLAG_vector_ics);
    feedback_vector_ = vector;
    slot_ = slot;
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    // tagged[tagged]
    return Representation::Tagged();
  }

  virtual HValue* Canonicalize() OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedGeneric)

 private:
  HLoadKeyedGeneric(HValue* context, HValue* obj, HValue* key)
      : slot_(FeedbackSlotInterface::kInvalidFeedbackSlot) {
    set_representation(Representation::Tagged());
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, context);
    SetAllSideEffects();
  }

  Handle<FixedArray> feedback_vector_;
  int slot_;
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


class HStoreNamedField FINAL : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HStoreNamedField, HValue*,
                                 HObjectAccess, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P4(HStoreNamedField, HValue*,
                                 HObjectAccess, HValue*, StoreFieldOrKeyedMode);

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField)

  virtual bool HasEscapingOperandAt(int index) OVERRIDE {
    return index == 1;
  }
  virtual bool HasOutOfBoundsAccess(int size) OVERRIDE {
    return !access().IsInobject() || access().offset() >= size;
  }
  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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
                                         HValue* dominator) OVERRIDE {
    DCHECK(side_effect == kNewSpacePromotion);
    if (!FLAG_use_write_barrier_elimination) return false;
    dominator_ = dominator;
    return false;
  }
  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  HValue* object() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }
  HValue* transition() const { return OperandAt(2); }

  HObjectAccess access() const { return access_; }
  HValue* dominator() const { return dominator_; }
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

  void SetTransition(HConstant* transition) {
    DCHECK(!has_transition());  // Only set once.
    SetOperandAt(2, transition);
    has_transition_ = true;
    SetChangesFlag(kMaps);
  }

  bool NeedsWriteBarrier() const {
    DCHECK(!field_representation().IsDouble() || !has_transition());
    if (field_representation().IsDouble()) return false;
    if (field_representation().IsSmi()) return false;
    if (field_representation().IsInteger32()) return false;
    if (field_representation().IsExternal()) return false;
    return StoringValueNeedsWriteBarrier(value()) &&
        ReceiverObjectNeedsWriteBarrier(object(), value(), dominator());
  }

  bool NeedsWriteBarrierForMap() {
    return ReceiverObjectNeedsWriteBarrier(object(), transition(),
                                           dominator());
  }

  SmiCheck SmiCheckForWriteBarrier() const {
    if (field_representation().IsHeapObject()) return OMIT_SMI_CHECK;
    if (value()->type().IsHeapObject()) return OMIT_SMI_CHECK;
    return INLINE_SMI_CHECK;
  }

  PointersToHereCheck PointersToHereCheckForValue() const {
    return PointersToHereCheckForObject(value(), dominator());
  }

  Representation field_representation() const {
    return access_.representation();
  }

  void UpdateValue(HValue* value) {
    SetOperandAt(1, value);
  }

  bool CanBeReplacedWith(HStoreNamedField* that) const {
    if (!this->access().Equals(that->access())) return false;
    if (SmiValuesAre32Bits() &&
        this->field_representation().IsSmi() &&
        this->store_mode() == INITIALIZING_STORE &&
        that->store_mode() == STORE_TO_INITIALIZED_ENTRY) {
      // We cannot replace an initializing store to a smi field with a store to
      // an initialized entry on 64-bit architectures (with 32-bit smis).
      return false;
    }
    return true;
  }

 private:
  HStoreNamedField(HValue* obj,
                   HObjectAccess access,
                   HValue* val,
                   StoreFieldOrKeyedMode store_mode = INITIALIZING_STORE)
      : access_(access),
        dominator_(NULL),
        has_transition_(false),
        store_mode_(store_mode) {
    // Stores to a non existing in-object property are allowed only to the
    // newly allocated objects (via HAllocate or HInnerAllocatedObject).
    DCHECK(!access.IsInobject() || access.existing_inobject_property() ||
           obj->IsAllocate() || obj->IsInnerAllocatedObject());
    SetOperandAt(0, obj);
    SetOperandAt(1, val);
    SetOperandAt(2, obj);
    access.SetGVNFlags(this, STORE);
  }

  HObjectAccess access_;
  HValue* dominator_;
  bool has_transition_ : 1;
  StoreFieldOrKeyedMode store_mode_ : 1;
};


class HStoreNamedGeneric FINAL : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(HStoreNamedGeneric, HValue*,
                                              Handle<String>, HValue*,
                                              StrictMode);
  HValue* object() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }
  HValue* context() const { return OperandAt(2); }
  Handle<String> name() const { return name_; }
  StrictMode strict_mode() const { return strict_mode_; }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedGeneric)

 private:
  HStoreNamedGeneric(HValue* context,
                     HValue* object,
                     Handle<String> name,
                     HValue* value,
                     StrictMode strict_mode)
      : name_(name),
        strict_mode_(strict_mode) {
    SetOperandAt(0, object);
    SetOperandAt(1, value);
    SetOperandAt(2, context);
    SetAllSideEffects();
  }

  Handle<String> name_;
  StrictMode strict_mode_;
};


class HStoreKeyed FINAL
    : public HTemplateInstruction<3>, public ArrayInstructionInterface {
 public:
  DECLARE_INSTRUCTION_FACTORY_P4(HStoreKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind);
  DECLARE_INSTRUCTION_FACTORY_P5(HStoreKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind, StoreFieldOrKeyedMode);
  DECLARE_INSTRUCTION_FACTORY_P6(HStoreKeyed, HValue*, HValue*, HValue*,
                                 ElementsKind, StoreFieldOrKeyedMode, int);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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

    DCHECK_EQ(index, 2);
    return RequiredValueRepresentation(elements_kind_, store_mode_);
  }

  static Representation RequiredValueRepresentation(
      ElementsKind kind, StoreFieldOrKeyedMode mode) {
    if (IsDoubleOrFloatElementsKind(kind)) {
      return Representation::Double();
    }

    if (kind == FAST_SMI_ELEMENTS && SmiValuesAre32Bits() &&
        mode == STORE_TO_INITIALIZED_ENTRY) {
      return Representation::Integer32();
    }

    if (IsFastSmiElementsKind(kind)) {
      return Representation::Smi();
    }

    return IsExternalArrayElementsKind(kind) ||
                   IsFixedTypedArrayElementsKind(kind)
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

  virtual Representation observed_input_representation(int index) OVERRIDE {
    if (index < 2) return RequiredInputRepresentation(index);
    if (IsUninitialized()) {
      return Representation::None();
    }
    Representation r = RequiredValueRepresentation(elements_kind_, store_mode_);
    // For fast object elements kinds, don't assume anything.
    if (r.IsTagged()) return Representation::None();
    return r;
  }

  HValue* elements() const { return OperandAt(0); }
  HValue* key() const { return OperandAt(1); }
  HValue* value() const { return OperandAt(2); }
  bool value_is_smi() const {
    return IsFastSmiElementsKind(elements_kind_);
  }
  StoreFieldOrKeyedMode store_mode() const { return store_mode_; }
  ElementsKind elements_kind() const { return elements_kind_; }
  uint32_t base_offset() const { return base_offset_; }
  bool TryIncreaseBaseOffset(uint32_t increase_by_value);
  HValue* GetKey() { return key(); }
  void SetKey(HValue* key) { SetOperandAt(1, key); }
  bool IsDehoisted() const { return is_dehoisted_; }
  void SetDehoisted(bool is_dehoisted) { is_dehoisted_ = is_dehoisted; }
  bool IsUninitialized() { return is_uninitialized_; }
  void SetUninitialized(bool is_uninitialized) {
    is_uninitialized_ = is_uninitialized;
  }

  bool IsConstantHoleStore() {
    return value()->IsConstant() && HConstant::cast(value())->IsTheHole();
  }

  virtual bool HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) OVERRIDE {
    DCHECK(side_effect == kNewSpacePromotion);
    dominator_ = dominator;
    return false;
  }

  HValue* dominator() const { return dominator_; }

  bool NeedsWriteBarrier() {
    if (value_is_smi()) {
      return false;
    } else {
      return StoringValueNeedsWriteBarrier(value()) &&
          ReceiverObjectNeedsWriteBarrier(elements(), value(), dominator());
    }
  }

  PointersToHereCheck PointersToHereCheckForValue() const {
    return PointersToHereCheckForObject(value(), dominator());
  }

  bool NeedsCanonicalization();

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyed)

 private:
  HStoreKeyed(HValue* obj, HValue* key, HValue* val,
              ElementsKind elements_kind,
              StoreFieldOrKeyedMode store_mode = INITIALIZING_STORE,
              int offset = kDefaultKeyedHeaderOffsetSentinel)
      : elements_kind_(elements_kind),
      base_offset_(offset == kDefaultKeyedHeaderOffsetSentinel
          ? GetDefaultHeaderSizeForElementsKind(elements_kind)
          : offset),
      is_dehoisted_(false),
      is_uninitialized_(false),
      store_mode_(store_mode),
      dominator_(NULL) {
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, val);

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
  uint32_t base_offset_;
  bool is_dehoisted_ : 1;
  bool is_uninitialized_ : 1;
  StoreFieldOrKeyedMode store_mode_: 1;
  HValue* dominator_;
};


class HStoreKeyedGeneric FINAL : public HTemplateInstruction<4> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(HStoreKeyedGeneric, HValue*,
                                              HValue*, HValue*, StrictMode);

  HValue* object() const { return OperandAt(0); }
  HValue* key() const { return OperandAt(1); }
  HValue* value() const { return OperandAt(2); }
  HValue* context() const { return OperandAt(3); }
  StrictMode strict_mode() const { return strict_mode_; }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    // tagged[tagged] = tagged
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedGeneric)

 private:
  HStoreKeyedGeneric(HValue* context,
                     HValue* object,
                     HValue* key,
                     HValue* value,
                     StrictMode strict_mode)
      : strict_mode_(strict_mode) {
    SetOperandAt(0, object);
    SetOperandAt(1, key);
    SetOperandAt(2, value);
    SetOperandAt(3, context);
    SetAllSideEffects();
  }

  StrictMode strict_mode_;
};


class HTransitionElementsKind FINAL : public HTemplateInstruction<2> {
 public:
  inline static HTransitionElementsKind* New(Zone* zone,
                                             HValue* context,
                                             HValue* object,
                                             Handle<Map> original_map,
                                             Handle<Map> transitioned_map) {
    return new(zone) HTransitionElementsKind(context, object,
                                             original_map, transitioned_map);
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  HValue* object() const { return OperandAt(0); }
  HValue* context() const { return OperandAt(1); }
  Unique<Map> original_map() const { return original_map_; }
  Unique<Map> transitioned_map() const { return transitioned_map_; }
  ElementsKind from_kind() const { return from_kind_; }
  ElementsKind to_kind() const { return to_kind_; }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(TransitionElementsKind)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    HTransitionElementsKind* instr = HTransitionElementsKind::cast(other);
    return original_map_ == instr->original_map_ &&
           transitioned_map_ == instr->transitioned_map_;
  }

  virtual int RedefinedOperandIndex() { return 0; }

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


class HStringAdd FINAL : public HBinaryOperation {
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

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(StringAdd)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
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
  virtual bool IsDeletable() const OVERRIDE { return true; }

  const StringAddFlags flags_;
  const PretenureFlag pretenure_flag_;
};


class HStringCharCodeAt FINAL : public HTemplateInstruction<3> {
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
  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

  virtual Range* InferRange(Zone* zone) OVERRIDE {
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
  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HStringCharFromCode FINAL : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* char_code);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }

  virtual bool DataEquals(HValue* other) OVERRIDE { return true; }

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

  virtual bool IsDeletable() const OVERRIDE {
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
  virtual bool IsDeletable() const FINAL OVERRIDE { return true; }

  int literal_index_;
  int depth_;
  AllocationSiteMode allocation_site_mode_;
};


class HRegExpLiteral FINAL : public HMaterializedLiteral<1> {
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

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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


class HFunctionLiteral FINAL : public HTemplateInstruction<1> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HFunctionLiteral,
                                              Handle<SharedFunctionInfo>,
                                              bool);
  HValue* context() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(FunctionLiteral)

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  bool pretenure() const { return pretenure_; }
  bool has_no_literals() const { return has_no_literals_; }
  bool is_arrow() const { return IsArrowFunction(kind_); }
  bool is_generator() const { return IsGeneratorFunction(kind_); }
  bool is_concise_method() const { return IsConciseMethod(kind_); }
  FunctionKind kind() const { return kind_; }
  StrictMode strict_mode() const { return strict_mode_; }

 private:
  HFunctionLiteral(HValue* context, Handle<SharedFunctionInfo> shared,
                   bool pretenure)
      : HTemplateInstruction<1>(HType::JSObject()),
        shared_info_(shared),
        kind_(shared->kind()),
        pretenure_(pretenure),
        has_no_literals_(shared->num_literals() == 0),
        strict_mode_(shared->strict_mode()) {
    SetOperandAt(0, context);
    set_representation(Representation::Tagged());
    SetChangesFlag(kNewSpacePromotion);
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }

  Handle<SharedFunctionInfo> shared_info_;
  FunctionKind kind_;
  bool pretenure_ : 1;
  bool has_no_literals_ : 1;
  StrictMode strict_mode_;
};


class HTypeof FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HTypeof, HValue*);

  HValue* context() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Typeof)

 private:
  explicit HTypeof(HValue* context, HValue* value) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    set_representation(Representation::Tagged());
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HTrapAllocationMemento FINAL : public HTemplateInstruction<1> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HTrapAllocationMemento, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  HValue* object() { return OperandAt(0); }

  DECLARE_CONCRETE_INSTRUCTION(TrapAllocationMemento)

 private:
  explicit HTrapAllocationMemento(HValue* obj) {
    SetOperandAt(0, obj);
  }
};


class HToFastProperties FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HToFastProperties, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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
    DCHECK(value->IsCallRuntime());
#ifdef DEBUG
    const Runtime::Function* function = HCallRuntime::cast(value)->function();
    DCHECK(function->function_id == Runtime::kCreateObjectLiteral);
#endif
  }

  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HDateField FINAL : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HDateField, HValue*, Smi*);

  Smi* index() const { return index_; }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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


class HSeqStringGetChar FINAL : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           String::Encoding encoding,
                           HValue* string,
                           HValue* index);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return (index == 0) ? Representation::Tagged()
                        : Representation::Integer32();
  }

  String::Encoding encoding() const { return encoding_; }
  HValue* string() const { return OperandAt(0); }
  HValue* index() const { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(SeqStringGetChar)

 protected:
  virtual bool DataEquals(HValue* other) OVERRIDE {
    return encoding() == HSeqStringGetChar::cast(other)->encoding();
  }

  virtual Range* InferRange(Zone* zone) OVERRIDE {
    if (encoding() == String::ONE_BYTE_ENCODING) {
      return new(zone) Range(0, String::kMaxOneByteCharCode);
    } else {
      DCHECK_EQ(String::TWO_BYTE_ENCODING, encoding());
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

  virtual bool IsDeletable() const OVERRIDE { return true; }

  String::Encoding encoding_;
};


class HSeqStringSetChar FINAL : public HTemplateInstruction<4> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(
      HSeqStringSetChar, String::Encoding,
      HValue*, HValue*, HValue*);

  String::Encoding encoding() { return encoding_; }
  HValue* context() { return OperandAt(0); }
  HValue* string() { return OperandAt(1); }
  HValue* index() { return OperandAt(2); }
  HValue* value() { return OperandAt(3); }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
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


class HCheckMapValue FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HCheckMapValue, HValue*, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual HType CalculateInferredType() OVERRIDE {
    if (value()->type().IsHeapObject()) return value()->type();
    return HType::HeapObject();
  }

  HValue* value() const { return OperandAt(0); }
  HValue* map() const { return OperandAt(1); }

  virtual HValue* Canonicalize() OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(CheckMapValue)

 protected:
  virtual int RedefinedOperandIndex() { return 0; }

  virtual bool DataEquals(HValue* other) OVERRIDE {
    return true;
  }

 private:
  HCheckMapValue(HValue* value, HValue* map)
      : HTemplateInstruction<2>(HType::HeapObject()) {
    SetOperandAt(0, value);
    SetOperandAt(1, map);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kMaps);
    SetDependsOnFlag(kElementsKind);
  }
};


class HForInPrepareMap FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HForInPrepareMap, HValue*);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* enumerable() const { return OperandAt(1); }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual HType CalculateInferredType() OVERRIDE {
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


class HForInCacheArray FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HForInCacheArray, HValue*, HValue*, int);

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    return Representation::Tagged();
  }

  HValue* enumerable() const { return OperandAt(0); }
  HValue* map() const { return OperandAt(1); }
  int idx() const { return idx_; }

  HForInCacheArray* index_cache() {
    return index_cache_;
  }

  void set_index_cache(HForInCacheArray* index_cache) {
    index_cache_ = index_cache;
  }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual HType CalculateInferredType() OVERRIDE {
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


class HLoadFieldByIndex FINAL : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HLoadFieldByIndex, HValue*, HValue*);

  HLoadFieldByIndex(HValue* object,
                    HValue* index) {
    SetOperandAt(0, object);
    SetOperandAt(1, index);
    SetChangesFlag(kNewSpacePromotion);
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) OVERRIDE {
    if (index == 1) {
      return Representation::Smi();
    } else {
      return Representation::Tagged();
    }
  }

  HValue* object() const { return OperandAt(0); }
  HValue* index() const { return OperandAt(1); }

  virtual OStream& PrintDataTo(OStream& os) const OVERRIDE;  // NOLINT

  virtual HType CalculateInferredType() OVERRIDE {
    return HType::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFieldByIndex);

 private:
  virtual bool IsDeletable() const OVERRIDE { return true; }
};


class HStoreFrameContext: public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HStoreFrameContext, HValue*);

  HValue* context() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreFrameContext)
 private:
  explicit HStoreFrameContext(HValue* context)
      : HUnaryOperation(context) {
    set_representation(Representation::Tagged());
    SetChangesFlag(kContextSlots);
  }
};


class HAllocateBlockContext: public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HAllocateBlockContext, HValue*,
                                 HValue*, Handle<ScopeInfo>);
  HValue* context() const { return OperandAt(0); }
  HValue* function() const { return OperandAt(1); }
  Handle<ScopeInfo> scope_info() const { return scope_info_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual OStream& PrintDataTo(OStream& os) const;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(AllocateBlockContext)

 private:
  HAllocateBlockContext(HValue* context,
                        HValue* function,
                        Handle<ScopeInfo> scope_info)
      : scope_info_(scope_info) {
    SetOperandAt(0, context);
    SetOperandAt(1, function);
    set_representation(Representation::Tagged());
  }

  Handle<ScopeInfo> scope_info_;
};



#undef DECLARE_INSTRUCTION
#undef DECLARE_CONCRETE_INSTRUCTION

} }  // namespace v8::internal

#endif  // V8_HYDROGEN_INSTRUCTIONS_H_
