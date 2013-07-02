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
  V(AllocateObject)                            \
  V(ApplyArguments)                            \
  V(ArgumentsElements)                         \
  V(ArgumentsLength)                           \
  V(ArgumentsObject)                           \
  V(Bitwise)                                   \
  V(BitNot)                                    \
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
  V(Change)                                    \
  V(CheckFunction)                             \
  V(CheckHeapObject)                           \
  V(CheckInstanceType)                         \
  V(CheckMaps)                                 \
  V(CheckPrototypeMaps)                        \
  V(ClampToUint8)                              \
  V(ClassOfTestAndBranch)                      \
  V(CompareIDAndBranch)                        \
  V(CompareGeneric)                            \
  V(CompareObjectEqAndBranch)                  \
  V(CompareMap)                                \
  V(CompareConstantEqAndBranch)                \
  V(Constant)                                  \
  V(Context)                                   \
  V(DebugBreak)                                \
  V(DeclareGlobals)                            \
  V(DeleteProperty)                            \
  V(Deoptimize)                                \
  V(Div)                                       \
  V(DummyUse)                                  \
  V(ElementsKind)                              \
  V(EnterInlined)                              \
  V(EnvironmentMarker)                         \
  V(ForceRepresentation)                       \
  V(FunctionLiteral)                           \
  V(GetCachedArrayIndex)                       \
  V(GlobalObject)                              \
  V(GlobalReceiver)                            \
  V(Goto)                                      \
  V(HasCachedArrayIndexAndBranch)              \
  V(HasInstanceTypeAndBranch)                  \
  V(InductionVariableAnnotation)               \
  V(In)                                        \
  V(InnerAllocatedObject)                      \
  V(InstanceOf)                                \
  V(InstanceOfKnownGlobal)                     \
  V(InstanceSize)                              \
  V(InvokeFunction)                            \
  V(IsConstructCallAndBranch)                  \
  V(IsObjectAndBranch)                         \
  V(IsStringAndBranch)                         \
  V(IsSmiAndBranch)                            \
  V(IsUndetectableAndBranch)                   \
  V(LeaveInlined)                              \
  V(LoadContextSlot)                           \
  V(LoadExternalArrayPointer)                  \
  V(LoadFunctionPrototype)                     \
  V(LoadGlobalCell)                            \
  V(LoadGlobalGeneric)                         \
  V(LoadKeyed)                                 \
  V(LoadKeyedGeneric)                          \
  V(LoadNamedField)                            \
  V(LoadNamedFieldPolymorphic)                 \
  V(LoadNamedGeneric)                          \
  V(MapEnumLength)                             \
  V(MathFloorOfDiv)                            \
  V(MathMinMax)                                \
  V(Mod)                                       \
  V(Mul)                                       \
  V(NumericConstraint)                         \
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
  V(SoftDeoptimize)                            \
  V(StackCheck)                                \
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
  V(StringLength)                              \
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
  V(ForInPrepareMap)                           \
  V(ForInCacheArray)                           \
  V(CheckMapValue)                             \
  V(LoadFieldByIndex)                          \
  V(DateField)                                 \
  V(WrapReceiver)

#define GVN_TRACKED_FLAG_LIST(V)               \
  V(Maps)                                      \
  V(NewSpacePromotion)

#define GVN_UNTRACKED_FLAG_LIST(V)             \
  V(Calls)                                     \
  V(InobjectFields)                            \
  V(BackingStoreFields)                        \
  V(DoubleFields)                              \
  V(ElementsKind)                              \
  V(ElementsPointer)                           \
  V(ArrayElements)                             \
  V(DoubleArrayElements)                       \
  V(SpecializedArrayElements)                  \
  V(GlobalVars)                                \
  V(ArrayLengths)                              \
  V(ContextSlots)                              \
  V(OsrEntries)

#define DECLARE_ABSTRACT_INSTRUCTION(type)          \
  virtual bool Is##type() const { return true; }    \
  static H##type* cast(HValue* value) {             \
    ASSERT(value->Is##type());                      \
    return reinterpret_cast<H##type*>(value);       \
  }


#define DECLARE_CONCRETE_INSTRUCTION(type)                        \
  virtual LInstruction* CompileToLithium(LChunkBuilder* builder); \
  static H##type* cast(HValue* value) {                           \
    ASSERT(value->Is##type());                                    \
    return reinterpret_cast<H##type*>(value);                     \
  }                                                               \
  virtual Opcode opcode() const { return HValue::k##type; }


class Range: public ZoneObject {
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
  bool AddAndCheckOverflow(Range* other);
  bool SubAndCheckOverflow(Range* other);
  bool MulAndCheckOverflow(Range* other);

 private:
  int32_t lower_;
  int32_t upper_;
  Range* next_;
  bool can_be_minus_zero_;
};


class UniqueValueId {
 public:
  UniqueValueId() : raw_address_(NULL) { }

  explicit UniqueValueId(Object* object) {
    raw_address_ = reinterpret_cast<Address>(object);
    ASSERT(IsInitialized());
  }

  explicit UniqueValueId(Handle<Object> handle) {
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

 private:
  Address raw_address_;
};


class HType {
 public:
  HType() : type_(kUninitialized) { }

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
  static HType Uninitialized() { return HType(kUninitialized); }

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

  bool IsTagged() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kTagged) == kTagged);
  }

  bool IsTaggedPrimitive() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kTaggedPrimitive) == kTaggedPrimitive);
  }

  bool IsTaggedNumber() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kTaggedNumber) == kTaggedNumber);
  }

  bool IsSmi() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kSmi) == kSmi);
  }

  bool IsHeapNumber() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kHeapNumber) == kHeapNumber);
  }

  bool IsString() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kString) == kString);
  }

  bool IsBoolean() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kBoolean) == kBoolean);
  }

  bool IsNonPrimitive() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kNonPrimitive) == kNonPrimitive);
  }

  bool IsJSArray() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kJSArray) == kJSArray);
  }

  bool IsJSObject() const {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kJSObject) == kJSObject);
  }

  bool IsUninitialized() const {
    return type_ == kUninitialized;
  }

  bool IsHeapObject() const {
    ASSERT(type_ != kUninitialized);
    return IsHeapNumber() || IsString() || IsBoolean() || IsNonPrimitive();
  }

  static HType TypeFromValue(Handle<Object> value);

  const char* ToString();

 private:
  enum Type {
    kTagged = 0x1,           // 0000 0000 0000 0001
    kTaggedPrimitive = 0x5,  // 0000 0000 0000 0101
    kTaggedNumber = 0xd,     // 0000 0000 0000 1101
    kSmi = 0x1d,             // 0000 0000 0001 1101
    kHeapNumber = 0x2d,      // 0000 0000 0010 1101
    kString = 0x45,          // 0000 0000 0100 0101
    kBoolean = 0x85,         // 0000 0000 1000 0101
    kNonPrimitive = 0x101,   // 0000 0001 0000 0001
    kJSObject = 0x301,       // 0000 0011 0000 0001
    kJSArray = 0x701,        // 0000 0111 0000 0001
    kUninitialized = 0x1fff  // 0001 1111 1111 1111
  };

  // Make sure type fits in int16.
  STATIC_ASSERT(kUninitialized < (1 << (2 * kBitsPerByte)));

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
class HUseIterator BASE_EMBEDDED {
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


class NumericRelation {
 public:
  enum Kind { NONE, EQ, GT, GE, LT, LE, NE };
  static const char* MnemonicFromKind(Kind kind) {
    switch (kind) {
      case NONE: return "NONE";
      case EQ: return "EQ";
      case GT: return "GT";
      case GE: return "GE";
      case LT: return "LT";
      case LE: return "LE";
      case NE: return "NE";
    }
    UNREACHABLE();
    return NULL;
  }
  const char* Mnemonic() const { return MnemonicFromKind(kind_); }

  static NumericRelation None() { return NumericRelation(NONE); }
  static NumericRelation Eq() { return NumericRelation(EQ); }
  static NumericRelation Gt() { return NumericRelation(GT); }
  static NumericRelation Ge() { return NumericRelation(GE); }
  static NumericRelation Lt() { return NumericRelation(LT); }
  static NumericRelation Le() { return NumericRelation(LE); }
  static NumericRelation Ne() { return NumericRelation(NE); }

  bool IsNone() { return kind_ == NONE; }

  static NumericRelation FromToken(Token::Value token) {
    switch (token) {
      case Token::EQ: return Eq();
      case Token::EQ_STRICT: return Eq();
      case Token::LT: return Lt();
      case Token::GT: return Gt();
      case Token::LTE: return Le();
      case Token::GTE: return Ge();
      case Token::NE: return Ne();
      case Token::NE_STRICT: return Ne();
      default: return None();
    }
  }

  // The semantics of "Reversed" is that if "x rel y" is true then also
  // "y rel.Reversed() x" is true, and that rel.Reversed().Reversed() == rel.
  NumericRelation Reversed() {
    switch (kind_) {
      case NONE: return None();
      case EQ: return Eq();
      case GT: return Lt();
      case GE: return Le();
      case LT: return Gt();
      case LE: return Ge();
      case NE: return Ne();
    }
    UNREACHABLE();
    return None();
  }

  // The semantics of "Negated" is that if "x rel y" is true then also
  // "!(x rel.Negated() y)" is true.
  NumericRelation Negated() {
    switch (kind_) {
      case NONE: return None();
      case EQ: return Ne();
      case GT: return Le();
      case GE: return Lt();
      case LT: return Ge();
      case LE: return Gt();
      case NE: return Eq();
    }
    UNREACHABLE();
    return None();
  }

  // The semantics of "Implies" is that if "x rel y" is true
  // then also "x other_relation y" is true.
  bool Implies(NumericRelation other_relation) {
    switch (kind_) {
      case NONE: return false;
      case EQ: return (other_relation.kind_ == EQ)
          || (other_relation.kind_ == GE)
          || (other_relation.kind_ == LE);
      case GT: return (other_relation.kind_ == GT)
          || (other_relation.kind_ == GE)
          || (other_relation.kind_ == NE);
      case LT: return (other_relation.kind_ == LT)
          || (other_relation.kind_ == LE)
          || (other_relation.kind_ == NE);
      case GE: return (other_relation.kind_ == GE);
      case LE: return (other_relation.kind_ == LE);
      case NE: return (other_relation.kind_ == NE);
    }
    UNREACHABLE();
    return false;
  }

  // The semantics of "IsExtendable" is that if
  // "rel.IsExtendable(direction)" is true then
  // "x rel y" implies "(x + direction) rel y" .
  bool IsExtendable(int direction) {
    switch (kind_) {
      case NONE: return false;
      case EQ: return false;
      case GT: return (direction >= 0);
      case GE: return (direction >= 0);
      case LT: return (direction <= 0);
      case LE: return (direction <= 0);
      case NE: return false;
    }
    UNREACHABLE();
    return false;
  }

  // CompoundImplies returns true when
  // "((x + my_offset) >> my_scale) rel y" implies
  // "((x + other_offset) >> other_scale) other_relation y".
  bool CompoundImplies(NumericRelation other_relation,
                       int my_offset,
                       int my_scale,
                       int other_offset = 0,
                       int other_scale = 0) {
    return Implies(other_relation) && ComponentsImply(
        my_offset, my_scale, other_offset, other_scale);
  }

 private:
  // ComponentsImply returns true when
  // "((x + my_offset) >> my_scale) rel y" implies
  // "((x + other_offset) >> other_scale) rel y".
  bool ComponentsImply(int my_offset,
                       int my_scale,
                       int other_offset,
                       int other_scale) {
    switch (kind_) {
      case NONE: break;  // Fall through to UNREACHABLE().
      case EQ:
      case NE: return my_offset == other_offset && my_scale == other_scale;
      case GT:
      case GE: return my_offset <= other_offset && my_scale >= other_scale;
      case LT:
      case LE: return my_offset >= other_offset && my_scale <= other_scale;
    }
    UNREACHABLE();
    return false;
  }

  explicit NumericRelation(Kind kind) : kind_(kind) {}

  Kind kind_;
};


class DecompositionResult BASE_EMBEDDED {
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


class RangeEvaluationContext BASE_EMBEDDED {
 public:
  RangeEvaluationContext(HValue* value, HValue* upper);

  HValue* lower_bound() { return lower_bound_; }
  HValue* lower_bound_guarantee() { return lower_bound_guarantee_; }
  HValue* candidate() { return candidate_; }
  HValue* upper_bound() { return upper_bound_; }
  HValue* upper_bound_guarantee() { return upper_bound_guarantee_; }
  int offset() { return offset_; }
  int scale() { return scale_; }

  bool is_range_satisfied() {
    return lower_bound_guarantee() != NULL && upper_bound_guarantee() != NULL;
  }

  void set_lower_bound_guarantee(HValue* guarantee) {
    lower_bound_guarantee_ = ConvertGuarantee(guarantee);
  }
  void set_upper_bound_guarantee(HValue* guarantee) {
    upper_bound_guarantee_ = ConvertGuarantee(guarantee);
  }

  void swap_candidate(DecompositionResult* other_candicate) {
    other_candicate->SwapValues(&candidate_, &offset_, &scale_);
  }

 private:
  HValue* ConvertGuarantee(HValue* guarantee);

  HValue* lower_bound_;
  HValue* lower_bound_guarantee_;
  HValue* candidate_;
  HValue* upper_bound_;
  HValue* upper_bound_guarantee_;
  int offset_;
  int scale_;
};


typedef EnumSet<GVNFlag> GVNFlagSet;


class HValue: public ZoneObject {
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
    // sets this flag, it must implement SetSideEffectDominator() and should
    // indicate which side effects to track by setting GVN flags.
    kTrackSideEffectDominators,
    kCanOverflow,
    kBailoutOnMinusZero,
    kCanBeDivByZero,
    kAllowUndefinedAsNaN,
    kIsArguments,
    kTruncatingToInt32,
    kAllUsesTruncatingToInt32,
    // Set after an instruction is killed.
    kIsDead,
    // Instructions that are allowed to produce full range unsigned integer
    // values are marked with kUint32 flag. If arithmetic shift or a load from
    // EXTERNAL_UNSIGNED_INT_ELEMENTS array is not marked with this flag
    // it will deoptimize if result does not fit into signed integer range.
    // HGraph::ComputeSafeUint32Operations is responsible for setting this
    // flag.
    kUint32,
    // If a phi is involved in the evaluation of a numeric constraint the
    // recursion can cause an endless cycle: we use this flag to exit the loop.
    kNumericConstraintEvaluationInProgress,
    // This flag is set to true after the SetupInformativeDefinitions() pass
    // has processed this instruction.
    kIDefsProcessingDone,
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

  HValue() : block_(NULL),
             id_(kNoNumber),
             type_(HType::Tagged()),
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
    return IsInformativeDefinition() ? OperandAt(RedefinedOperandIndex())
                                     : NULL;
  }

  // A purely informative definition is an idef that will not emit code and
  // should therefore be removed from the graph in the RestoreActualValues
  // phase (so that live ranges will be shorter).
  virtual bool IsPurelyInformativeDefinition() { return false; }

  // This method must always return the original HValue SSA definition
  // (regardless of any iDef of this value).
  HValue* ActualValue() {
    return IsInformativeDefinition() ? RedefinedOperand()->ActualValue()
                                     : this;
  }

  virtual void AddInformativeDefinitions() {}

  void UpdateRedefinedUsesWhileSettingUpInformativeDefinitions() {
    UpdateRedefinedUsesInner<TestDominanceUsingProcessedFlag>();
  }
  void UpdateRedefinedUses() {
    UpdateRedefinedUsesInner<Dominates>();
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
  bool CheckUsesForFlag(Flag f);
  // Returns true if the flag specified is set for all uses, and this set
  // of uses is non-empty.
  bool HasAtLeastOneUseWithFlagAndNoneWithout(Flag f);

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

  // Compute unique ids upfront that is safe wrt GC and parallel recompilation.
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
  virtual void SetSideEffectDominator(GVNFlag side_effect, HValue* dominator) {
    UNREACHABLE();
  }

  // Check if this instruction has some reason that prevents elimination.
  bool CannotBeEliminated() const {
    return HasObservableSideEffects() || !IsDeletable();
  }

#ifdef DEBUG
  virtual void Verify() = 0;
#endif

  bool IsRelationTrue(NumericRelation relation,
                      HValue* other,
                      int offset = 0,
                      int scale = 0);

  bool TryGuaranteeRange(HValue* upper_bound);
  virtual bool TryDecompose(DecompositionResult* decomposition) {
    if (RedefinedOperand() != NULL) {
      return RedefinedOperand()->TryDecompose(decomposition);
    } else {
      return false;
    }
  }

 protected:
  void TryGuaranteeRangeRecursive(RangeEvaluationContext* context);

  enum RangeGuaranteeDirection {
    DIRECTION_NONE = 0,
    DIRECTION_UPPER = 1,
    DIRECTION_LOWER = 2,
    DIRECTION_BOTH = DIRECTION_UPPER | DIRECTION_LOWER
  };
  virtual void SetResponsibilityForRange(RangeGuaranteeDirection direction) {}
  virtual void TryGuaranteeRangeChanging(RangeEvaluationContext* context) {}

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

  // Signature of a function testing if a HValue properly dominates another.
  typedef bool (*DominanceTest)(HValue*, HValue*);

  // Simple implementation of DominanceTest implemented walking the chain
  // of Hinstructions (used in UpdateRedefinedUsesInner).
  static bool Dominates(HValue* dominator, HValue* dominated);

  // A fast implementation of DominanceTest that works only for the
  // "current" instruction in the SetupInformativeDefinitions() phase.
  // During that phase we use a flag to mark processed instructions, and by
  // checking the flag we can quickly test if an instruction comes before or
  // after the "current" one.
  static bool TestDominanceUsingProcessedFlag(HValue* dominator,
                                              HValue* dominated);

  // If we are redefining an operand, update all its dominated uses (the
  // function that checks if a use is dominated is the template argument).
  template<DominanceTest TestDominance>
  void UpdateRedefinedUsesInner() {
    HValue* input = RedefinedOperand();
    if (input != NULL) {
      for (HUseIterator uses = input->uses(); !uses.Done(); uses.Advance()) {
        HValue* use = uses.value();
        if (TestDominance(this, use)) {
          use->SetOperandAt(uses.index(), this);
        }
      }
    }
  }

  // Informative definitions can override this method to state any numeric
  // relation they provide on the redefined value.
  // Returns true if it is guaranteed that:
  // ((this + offset) >> scale) relation other
  virtual bool IsRelationTrueInternal(NumericRelation relation,
                                      HValue* other,
                                      int offset = 0,
                                      int scale = 0) {
    return false;
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


class HInstruction: public HValue {
 public:
  HInstruction* next() const { return next_; }
  HInstruction* previous() const { return previous_; }

  virtual void PrintTo(StringStream* stream);
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
  virtual void Verify();
#endif

  virtual bool IsCall() { return false; }

  DECLARE_ABSTRACT_INSTRUCTION(Instruction)

 protected:
  HInstruction()
      : next_(NULL),
        previous_(NULL),
        position_(RelocInfo::kNoPosition) {
    SetGVNFlag(kDependsOnOsrEntries);
  }

  virtual void DeleteFromGraph() { Unlink(); }

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
  int OperandCount() { return V; }
  HValue* OperandAt(int i) const { return inputs_[i]; }

 protected:
  void InternalSetOperandAt(int i, HValue* value) { inputs_[i] = value; }

 private:
  EmbeddedContainer<HValue*, V> inputs_;
};


class HControlInstruction: public HInstruction {
 public:
  virtual HBasicBlock* SuccessorAt(int i) = 0;
  virtual int SuccessorCount() = 0;
  virtual void SetSuccessorAt(int i, HBasicBlock* block) = 0;

  virtual void PrintDataTo(StringStream* stream);

  HBasicBlock* FirstSuccessor() {
    return SuccessorCount() > 0 ? SuccessorAt(0) : NULL;
  }
  HBasicBlock* SecondSuccessor() {
    return SuccessorCount() > 1 ? SuccessorAt(1) : NULL;
  }

  DECLARE_ABSTRACT_INSTRUCTION(ControlInstruction)
};


class HSuccessorIterator BASE_EMBEDDED {
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
class HTemplateControlInstruction: public HControlInstruction {
 public:
  int SuccessorCount() { return S; }
  HBasicBlock* SuccessorAt(int i) { return successors_[i]; }
  void SetSuccessorAt(int i, HBasicBlock* block) { successors_[i] = block; }

  int OperandCount() { return V; }
  HValue* OperandAt(int i) const { return inputs_[i]; }


 protected:
  void InternalSetOperandAt(int i, HValue* value) { inputs_[i] = value; }

 private:
  EmbeddedContainer<HBasicBlock*, S> successors_;
  EmbeddedContainer<HValue*, V> inputs_;
};


class HBlockEntry: public HTemplateInstruction<0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(BlockEntry)
};


class HDummyUse: public HTemplateInstruction<1> {
 public:
  explicit HDummyUse(HValue* value) {
    SetOperandAt(0, value);
    // Pretend to be a Smi so that the HChange instructions inserted
    // before any use generate as little code as possible.
    set_representation(Representation::Tagged());
    set_type(HType::Smi());
  }

  HValue* value() { return OperandAt(0); }

  virtual bool HasEscapingOperandAt(int index) { return false; }
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(DummyUse);
};


class HNumericConstraint : public HTemplateInstruction<2> {
 public:
  static HNumericConstraint* AddToGraph(HValue* constrained_value,
                                        NumericRelation relation,
                                        HValue* related_value,
                                        HInstruction* insertion_point = NULL);

  HValue* constrained_value() { return OperandAt(0); }
  HValue* related_value() { return OperandAt(1); }
  NumericRelation relation() { return relation_; }

  virtual int RedefinedOperandIndex() { return 0; }
  virtual bool IsPurelyInformativeDefinition() { return true; }

  virtual Representation RequiredInputRepresentation(int index) {
    return representation();
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual bool IsRelationTrueInternal(NumericRelation other_relation,
                                      HValue* other_related_value,
                                      int offset = 0,
                                      int scale = 0) {
    if (related_value() == other_related_value) {
      return relation().CompoundImplies(other_relation, offset, scale);
    } else {
      return false;
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(NumericConstraint)

 private:
  HNumericConstraint(HValue* constrained_value,
                     NumericRelation relation,
                     HValue* related_value)
      : relation_(relation) {
    SetOperandAt(0, constrained_value);
    SetOperandAt(1, related_value);
  }

  NumericRelation relation_;
};


// We insert soft-deoptimize when we hit code with unknown typefeedback,
// so that we get a chance of re-optimizing with useful typefeedback.
// HSoftDeoptimize does not end a basic block as opposed to HDeoptimize.
class HSoftDeoptimize: public HTemplateInstruction<0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(SoftDeoptimize)
};


// Inserts an int3/stop break instruction for debugging purposes.
class HDebugBreak: public HTemplateInstruction<0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(DebugBreak)
};


class HDeoptimize: public HControlInstruction {
 public:
  HDeoptimize(int environment_length,
              int first_local_index,
              int first_expression_index,
              Zone* zone)
      : values_(environment_length, zone),
        first_local_index_(first_local_index),
        first_expression_index_(first_expression_index) { }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  virtual int OperandCount() { return values_.length(); }
  virtual HValue* OperandAt(int index) const { return values_[index]; }
  virtual void PrintDataTo(StringStream* stream);

  virtual int SuccessorCount() { return 0; }
  virtual HBasicBlock* SuccessorAt(int i) {
    UNREACHABLE();
    return NULL;
  }
  virtual void SetSuccessorAt(int i, HBasicBlock* block) {
    UNREACHABLE();
  }

  void AddEnvironmentValue(HValue* value, Zone* zone) {
    values_.Add(NULL, zone);
    SetOperandAt(values_.length() - 1, value);
  }
  int first_local_index() { return first_local_index_; }
  int first_expression_index() { return first_expression_index_; }

  DECLARE_CONCRETE_INSTRUCTION(Deoptimize)

  enum UseEnvironment {
    kNoUses,
    kUseAll
  };

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    values_[index] = value;
  }

 private:
  ZoneList<HValue*> values_;
  int first_local_index_;
  int first_expression_index_;
};


class HGoto: public HTemplateControlInstruction<1, 0> {
 public:
  explicit HGoto(HBasicBlock* target) {
    SetSuccessorAt(0, target);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(Goto)
};


class HUnaryControlInstruction: public HTemplateControlInstruction<2, 1> {
 public:
  HUnaryControlInstruction(HValue* value,
                           HBasicBlock* true_target,
                           HBasicBlock* false_target) {
    SetOperandAt(0, value);
    SetSuccessorAt(0, true_target);
    SetSuccessorAt(1, false_target);
  }

  virtual void PrintDataTo(StringStream* stream);

  HValue* value() { return OperandAt(0); }
};


class HBranch: public HUnaryControlInstruction {
 public:
  HBranch(HValue* value,
          HBasicBlock* true_target,
          HBasicBlock* false_target,
          ToBooleanStub::Types expected_input_types = ToBooleanStub::Types())
      : HUnaryControlInstruction(value, true_target, false_target),
        expected_input_types_(expected_input_types) {
    ASSERT(true_target != NULL && false_target != NULL);
    SetFlag(kAllowUndefinedAsNaN);
  }
  explicit HBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) {
    SetFlag(kAllowUndefinedAsNaN);
  }
  HBranch(HValue* value, ToBooleanStub::Types expected_input_types)
      : HUnaryControlInstruction(value, NULL, NULL),
        expected_input_types_(expected_input_types) {
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }
  virtual Representation observed_input_representation(int index);

  ToBooleanStub::Types expected_input_types() const {
    return expected_input_types_;
  }

  DECLARE_CONCRETE_INSTRUCTION(Branch)

 private:
  ToBooleanStub::Types expected_input_types_;
};


class HCompareMap: public HUnaryControlInstruction {
 public:
  HCompareMap(HValue* value,
              Handle<Map> map,
              HBasicBlock* true_target,
              HBasicBlock* false_target)
      : HUnaryControlInstruction(value, true_target, false_target),
        map_(map) {
    ASSERT(true_target != NULL);
    ASSERT(false_target != NULL);
    ASSERT(!map.is_null());
  }

  virtual void PrintDataTo(StringStream* stream);

  Handle<Map> map() const { return map_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareMap)

 private:
  Handle<Map> map_;
};


class HReturn: public HTemplateControlInstruction<0, 3> {
 public:
  HReturn(HValue* value, HValue* context, HValue* parameter_count) {
    SetOperandAt(0, value);
    SetOperandAt(1, context);
    SetOperandAt(2, parameter_count);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  HValue* value() { return OperandAt(0); }
  HValue* context() { return OperandAt(1); }
  HValue* parameter_count() { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(Return)
};


class HAbnormalExit: public HTemplateControlInstruction<0, 0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(AbnormalExit)
};


class HUnaryOperation: public HTemplateInstruction<1> {
 public:
  explicit HUnaryOperation(HValue* value) {
    SetOperandAt(0, value);
  }

  static HUnaryOperation* cast(HValue* value) {
    return reinterpret_cast<HUnaryOperation*>(value);
  }

  HValue* value() const { return OperandAt(0); }
  virtual void PrintDataTo(StringStream* stream);
};


class HThrow: public HTemplateInstruction<2> {
 public:
  HThrow(HValue* context, HValue* value) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(Throw)
};


class HUseConst: public HUnaryOperation {
 public:
  explicit HUseConst(HValue* old_value) : HUnaryOperation(old_value) { }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(UseConst)
};


class HForceRepresentation: public HTemplateInstruction<1> {
 public:
  HForceRepresentation(HValue* value, Representation required_representation) {
    SetOperandAt(0, value);
    set_representation(required_representation);
  }

  HValue* value() { return OperandAt(0); }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual Representation RequiredInputRepresentation(int index) {
    return representation();  // Same as the output representation.
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(ForceRepresentation)
};


class HChange: public HUnaryOperation {
 public:
  HChange(HValue* value,
          Representation to,
          bool is_truncating,
          bool allow_undefined_as_nan)
      : HUnaryOperation(value) {
    ASSERT(!value->representation().IsNone());
    ASSERT(!to.IsNone());
    ASSERT(!value->representation().Equals(to));
    set_representation(to);
    SetFlag(kUseGVN);
    if (allow_undefined_as_nan) SetFlag(kAllowUndefinedAsNaN);
    if (is_truncating) SetFlag(kTruncatingToInt32);
    if (value->representation().IsSmi() || value->type().IsSmi()) {
      set_type(HType::Smi());
    } else {
      set_type(HType::TaggedNumber());
      if (to.IsTagged()) SetGVNFlag(kChangesNewSpacePromotion);
    }
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);
  virtual HType CalculateInferredType();
  virtual HValue* Canonicalize();

  Representation from() const { return value()->representation(); }
  Representation to() const { return representation(); }
  bool allow_undefined_as_nan() const {
    return CheckFlag(kAllowUndefinedAsNaN);
  }
  bool deoptimize_on_minus_zero() const {
    return CheckFlag(kBailoutOnMinusZero);
  }
  virtual Representation RequiredInputRepresentation(int index) {
    return from();
  }

  virtual Range* InferRange(Zone* zone);

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(Change)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const {
    return !from().IsTagged() || value()->type().IsSmi();
  }
};


class HClampToUint8: public HUnaryOperation {
 public:
  explicit HClampToUint8(HValue* value)
      : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kAllowUndefinedAsNaN);
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ClampToUint8)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


enum RemovableSimulate {
  REMOVABLE_SIMULATE,
  FIXED_SIMULATE
};


class HSimulate: public HInstruction {
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
  virtual ~HSimulate() {}

  virtual void PrintDataTo(StringStream* stream);

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
  virtual int OperandCount() { return values_.length(); }
  virtual HValue* OperandAt(int index) const { return values_[index]; }

  virtual bool HasEscapingOperandAt(int index) { return false; }
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  void MergeWith(ZoneList<HSimulate*>* list);
  bool is_candidate_for_removal() { return removable_ == REMOVABLE_SIMULATE; }

  DECLARE_CONCRETE_INSTRUCTION(Simulate)

#ifdef DEBUG
  virtual void Verify();
  void set_closure(Handle<JSFunction> closure) { closure_ = closure; }
  Handle<JSFunction> closure() const { return closure_; }
#endif

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
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


class HEnvironmentMarker: public HTemplateInstruction<1> {
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

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  virtual void PrintDataTo(StringStream* stream);

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


class HStackCheck: public HTemplateInstruction<1> {
 public:
  enum Type {
    kFunctionEntry,
    kBackwardsBranch
  };

  HStackCheck(HValue* context, Type type) : type_(type) {
    SetOperandAt(0, context);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  HValue* context() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) {
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


class HEnterInlined: public HTemplateInstruction<0> {
 public:
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

  void RegisterReturnTarget(HBasicBlock* return_target, Zone* zone);
  ZoneList<HBasicBlock*>* return_targets() { return &return_targets_; }

  virtual void PrintDataTo(StringStream* stream);

  Handle<JSFunction> closure() const { return closure_; }
  int arguments_count() const { return arguments_count_; }
  bool arguments_pushed() const { return arguments_pushed_; }
  void set_arguments_pushed() { arguments_pushed_ = true; }
  FunctionLiteral* function() const { return function_; }
  InliningKind inlining_kind() const { return inlining_kind_; }
  bool undefined_receiver() const { return undefined_receiver_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  Variable* arguments_var() { return arguments_var_; }
  HArgumentsObject* arguments_object() { return arguments_object_; }

  DECLARE_CONCRETE_INSTRUCTION(EnterInlined)

 private:
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


class HLeaveInlined: public HTemplateInstruction<0> {
 public:
  HLeaveInlined() { }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(LeaveInlined)
};


class HPushArgument: public HUnaryOperation {
 public:
  explicit HPushArgument(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* argument() { return OperandAt(0); }

  DECLARE_CONCRETE_INSTRUCTION(PushArgument)
};


class HThisFunction: public HTemplateInstruction<0> {
 public:
  HThisFunction() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ThisFunction)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HContext: public HTemplateInstruction<0> {
 public:
  HContext() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Context)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HOuterContext: public HUnaryOperation {
 public:
  explicit HOuterContext(HValue* inner) : HUnaryOperation(inner) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(OuterContext);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HDeclareGlobals: public HUnaryOperation {
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

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> pairs() const { return pairs_; }
  int flags() const { return flags_; }

  DECLARE_CONCRETE_INSTRUCTION(DeclareGlobals)

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }
 private:
  Handle<FixedArray> pairs_;
  int flags_;
};


class HGlobalObject: public HUnaryOperation {
 public:
  explicit HGlobalObject(HValue* context) : HUnaryOperation(context) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(GlobalObject)

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HGlobalReceiver: public HUnaryOperation {
 public:
  explicit HGlobalReceiver(HValue* global_object)
      : HUnaryOperation(global_object) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(GlobalReceiver)

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


template <int V>
class HCall: public HTemplateInstruction<V> {
 public:
  // The argument count includes the receiver.
  explicit HCall<V>(int argument_count) : argument_count_(argument_count) {
    this->set_representation(Representation::Tagged());
    this->SetAllSideEffects();
  }

  virtual HType CalculateInferredType() { return HType::Tagged(); }

  virtual int argument_count() const { return argument_count_; }

  virtual bool IsCall() { return true; }

 private:
  int argument_count_;
};


class HUnaryCall: public HCall<1> {
 public:
  HUnaryCall(HValue* value, int argument_count)
      : HCall<1>(argument_count) {
    SetOperandAt(0, value);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  HValue* value() { return OperandAt(0); }
};


class HBinaryCall: public HCall<2> {
 public:
  HBinaryCall(HValue* first, HValue* second, int argument_count)
      : HCall<2>(argument_count) {
    SetOperandAt(0, first);
    SetOperandAt(1, second);
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* first() { return OperandAt(0); }
  HValue* second() { return OperandAt(1); }
};


class HInvokeFunction: public HBinaryCall {
 public:
  HInvokeFunction(HValue* context, HValue* function, int argument_count)
      : HBinaryCall(context, function, argument_count) {
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

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
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


class HCallConstantFunction: public HCall<0> {
 public:
  HCallConstantFunction(Handle<JSFunction> function, int argument_count)
      : HCall<0>(argument_count),
        function_(function),
        formal_parameter_count_(function->shared()->formal_parameter_count()) {}

  Handle<JSFunction> function() const { return function_; }
  int formal_parameter_count() const { return formal_parameter_count_; }

  bool IsApplyFunction() const {
    return function_->code() ==
        Isolate::Current()->builtins()->builtin(Builtins::kFunctionApply);
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallConstantFunction)

 private:
  Handle<JSFunction> function_;
  int formal_parameter_count_;
};


class HCallKeyed: public HBinaryCall {
 public:
  HCallKeyed(HValue* context, HValue* key, int argument_count)
      : HBinaryCall(context, key, argument_count) {
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* context() { return first(); }
  HValue* key() { return second(); }

  DECLARE_CONCRETE_INSTRUCTION(CallKeyed)
};


class HCallNamed: public HUnaryCall {
 public:
  HCallNamed(HValue* context, Handle<String> name, int argument_count)
      : HUnaryCall(context, argument_count), name_(name) {
  }

  virtual void PrintDataTo(StringStream* stream);

  HValue* context() { return value(); }
  Handle<String> name() const { return name_; }

  DECLARE_CONCRETE_INSTRUCTION(CallNamed)

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

 private:
  Handle<String> name_;
};


class HCallFunction: public HBinaryCall {
 public:
  HCallFunction(HValue* context, HValue* function, int argument_count)
      : HBinaryCall(context, function, argument_count) {
  }

  HValue* context() { return first(); }
  HValue* function() { return second(); }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallFunction)
};


class HCallGlobal: public HUnaryCall {
 public:
  HCallGlobal(HValue* context, Handle<String> name, int argument_count)
      : HUnaryCall(context, argument_count), name_(name) {
  }

  virtual void PrintDataTo(StringStream* stream);

  HValue* context() { return value(); }
  Handle<String> name() const { return name_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallGlobal)

 private:
  Handle<String> name_;
};


class HCallKnownGlobal: public HCall<0> {
 public:
  HCallKnownGlobal(Handle<JSFunction> target, int argument_count)
      : HCall<0>(argument_count),
        target_(target),
        formal_parameter_count_(target->shared()->formal_parameter_count()) { }

  virtual void PrintDataTo(StringStream* stream);

  Handle<JSFunction> target() const { return target_; }
  int formal_parameter_count() const { return formal_parameter_count_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallKnownGlobal)

 private:
  Handle<JSFunction> target_;
  int formal_parameter_count_;
};


class HCallNew: public HBinaryCall {
 public:
  HCallNew(HValue* context, HValue* constructor, int argument_count)
      : HBinaryCall(context, constructor, argument_count) {
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* context() { return first(); }
  HValue* constructor() { return second(); }

  DECLARE_CONCRETE_INSTRUCTION(CallNew)
};


class HCallNewArray: public HCallNew {
 public:
  HCallNewArray(HValue* context, HValue* constructor, int argument_count,
                Handle<Cell> type_cell, ElementsKind elements_kind)
      : HCallNew(context, constructor, argument_count),
        elements_kind_(elements_kind),
        type_cell_(type_cell) {}

  virtual void PrintDataTo(StringStream* stream);

  Handle<Cell> property_cell() const {
    return type_cell_;
  }

  ElementsKind elements_kind() const { return elements_kind_; }

  DECLARE_CONCRETE_INSTRUCTION(CallNewArray)

 private:
  ElementsKind elements_kind_;
  Handle<Cell> type_cell_;
};


class HCallRuntime: public HCall<1> {
 public:
  HCallRuntime(HValue* context,
               Handle<String> name,
               const Runtime::Function* c_function,
               int argument_count)
      : HCall<1>(argument_count), c_function_(c_function), name_(name) {
    SetOperandAt(0, context);
  }

  virtual void PrintDataTo(StringStream* stream);

  HValue* context() { return OperandAt(0); }
  const Runtime::Function* function() const { return c_function_; }
  Handle<String> name() const { return name_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallRuntime)

 private:
  const Runtime::Function* c_function_;
  Handle<String> name_;
};


class HMapEnumLength: public HUnaryOperation {
 public:
  explicit HMapEnumLength(HValue* value) : HUnaryOperation(value) {
    set_type(HType::Smi());
    set_representation(Representation::Smi());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(MapEnumLength)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HElementsKind: public HUnaryOperation {
 public:
  explicit HElementsKind(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnElementsKind);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ElementsKind)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HBitNot: public HUnaryOperation {
 public:
  explicit HBitNot(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetFlag(kTruncatingToInt32);
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Integer32();
  }
  virtual Representation observed_input_representation(int index) {
    return Representation::Integer32();
  }
  virtual HType CalculateInferredType();

  virtual HValue* Canonicalize();

  DECLARE_CONCRETE_INSTRUCTION(BitNot)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HUnaryMathOperation: public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* value,
                           BuiltinFunctionId op);

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream);

  virtual HType CalculateInferredType();

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual Representation RequiredInputRepresentation(int index) {
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

  virtual Range* InferRange(Zone* zone);

  virtual HValue* Canonicalize();
  virtual Representation RepresentationFromInputs();

  BuiltinFunctionId op() const { return op_; }
  const char* OpName() const;

  DECLARE_CONCRETE_INSTRUCTION(UnaryMathOperation)

 protected:
  virtual bool DataEquals(HValue* other) {
    HUnaryMathOperation* b = HUnaryMathOperation::cast(other);
    return op_ == b->op();
  }

 private:
  HUnaryMathOperation(HValue* context, HValue* value, BuiltinFunctionId op)
      : op_(op) {
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

  virtual bool IsDeletable() const { return true; }

  BuiltinFunctionId op_;
};


class HLoadExternalArrayPointer: public HUnaryOperation {
 public:
  explicit HLoadExternalArrayPointer(HValue* value)
      : HUnaryOperation(value) {
    set_representation(Representation::External());
    // The result of this instruction is idempotent as long as its inputs don't
    // change.  The external array of a specialized array elements object cannot
    // change once set, so it's no necessary to introduce any additional
    // dependencies on top of the inputs.
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadExternalArrayPointer)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HCheckMaps: public HTemplateInstruction<2> {
 public:
  static HCheckMaps* New(HValue* value, Handle<Map> map, Zone* zone,
                         HValue *typecheck = NULL) {
    HCheckMaps* check_map = new(zone) HCheckMaps(value, zone, typecheck);
    check_map->map_set_.Add(map, zone);
    return check_map;
  }

  static HCheckMaps* New(HValue* value, SmallMapList* maps, Zone* zone,
                         HValue *typecheck = NULL) {
    HCheckMaps* check_map = new(zone) HCheckMaps(value, zone, typecheck);
    for (int i = 0; i < maps->length(); i++) {
      check_map->map_set_.Add(maps->at(i), zone);
    }
    check_map->map_set_.Sort();
    return check_map;
  }

  static HCheckMaps* NewWithTransitions(HValue* value, Handle<Map> map,
                                        Zone* zone) {
    HCheckMaps* check_map = new(zone) HCheckMaps(value, zone, value);
    check_map->map_set_.Add(map, zone);

    // Since transitioned elements maps of the initial map don't fail the map
    // check, the CheckMaps instruction doesn't need to depend on ElementsKinds.
    check_map->ClearGVNFlag(kDependsOnElementsKind);

    ElementsKind kind = map->elements_kind();
    bool packed = IsFastPackedElementsKind(kind);
    while (CanTransitionToMoreGeneralFastElementsKind(kind, packed)) {
      kind = GetNextMoreGeneralFastElementsKind(kind, packed);
      Map* transitioned_map =
          map->LookupElementsTransitionMap(kind);
      if (transitioned_map) {
        check_map->map_set_.Add(Handle<Map>(transitioned_map), zone);
      }
    };
    check_map->map_set_.Sort();
    return check_map;
  }

  virtual bool HasEscapingOperandAt(int index) { return false; }
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }
  virtual void SetSideEffectDominator(GVNFlag side_effect, HValue* dominator);
  virtual void PrintDataTo(StringStream* stream);
  virtual HType CalculateInferredType();

  HValue* value() { return OperandAt(0); }
  SmallMapList* map_set() { return &map_set_; }

  virtual void FinalizeUniqueValueId();

  DECLARE_CONCRETE_INSTRUCTION(CheckMaps)

 protected:
  virtual bool DataEquals(HValue* other) {
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

 private:
  // Clients should use one of the static New* methods above.
  HCheckMaps(HValue* value, Zone *zone, HValue* typecheck)
      : map_unique_ids_(0, zone) {
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

  SmallMapList map_set_;
  ZoneList<UniqueValueId> map_unique_ids_;
};


class HCheckFunction: public HUnaryOperation {
 public:
  HCheckFunction(HValue* value, Handle<JSFunction> function)
      : HUnaryOperation(value), target_(function), target_unique_id_() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    target_in_new_space_ = Isolate::Current()->heap()->InNewSpace(*function);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream);
  virtual HType CalculateInferredType();

#ifdef DEBUG
  virtual void Verify();
#endif

  virtual void FinalizeUniqueValueId() {
    target_unique_id_ = UniqueValueId(target_);
  }

  Handle<JSFunction> target() const { return target_; }
  bool target_in_new_space() const { return target_in_new_space_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckFunction)

 protected:
  virtual bool DataEquals(HValue* other) {
    HCheckFunction* b = HCheckFunction::cast(other);
    return target_unique_id_ == b->target_unique_id_;
  }

 private:
  Handle<JSFunction> target_;
  UniqueValueId target_unique_id_;
  bool target_in_new_space_;
};


class HCheckInstanceType: public HUnaryOperation {
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

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual HValue* Canonicalize();

  bool is_interval_check() const { return check_ <= LAST_INTERVAL_CHECK; }
  void GetCheckInterval(InstanceType* first, InstanceType* last);
  void GetCheckMaskAndTag(uint8_t* mask, uint8_t* tag);

  DECLARE_CONCRETE_INSTRUCTION(CheckInstanceType)

 protected:
  // TODO(ager): It could be nice to allow the ommision of instance
  // type checks if we have already performed an instance type check
  // with a larger range.
  virtual bool DataEquals(HValue* other) {
    HCheckInstanceType* b = HCheckInstanceType::cast(other);
    return check_ == b->check_;
  }

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


class HCheckHeapObject: public HUnaryOperation {
 public:
  explicit HCheckHeapObject(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType();

#ifdef DEBUG
  virtual void Verify();
#endif

  virtual HValue* Canonicalize() {
    HType value_type = value()->type();
    if (!value_type.IsUninitialized() && value_type.IsHeapObject()) {
      return NULL;
    }
    return this;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckHeapObject)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HCheckPrototypeMaps: public HTemplateInstruction<0> {
 public:
  HCheckPrototypeMaps(Handle<JSObject> prototype,
                      Handle<JSObject> holder,
                      Zone* zone,
                      CompilationInfo* info)
      : prototypes_(2, zone),
        maps_(2, zone),
        first_prototype_unique_id_(),
        last_prototype_unique_id_(),
        can_omit_prototype_maps_(true) {
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
    // Keep a list of all objects on the prototype chain up to the holder
    // and the expected maps.
    while (true) {
      prototypes_.Add(prototype, zone);
      Handle<Map> map(prototype->map());
      maps_.Add(map, zone);
      can_omit_prototype_maps_ &= map->CanOmitPrototypeChecks();
      if (prototype.is_identical_to(holder)) break;
      prototype = Handle<JSObject>(JSObject::cast(prototype->GetPrototype()));
    }
    if (can_omit_prototype_maps_) {
      // Mark in-flight compilation as dependent on those maps.
      for (int i = 0; i < maps()->length(); i++) {
        Handle<Map> map = maps()->at(i);
        map->AddDependentCompilationInfo(DependentCode::kPrototypeCheckGroup,
                                         info);
      }
    }
  }

  ZoneList<Handle<JSObject> >* prototypes() { return &prototypes_; }

  ZoneList<Handle<Map> >* maps() { return &maps_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckPrototypeMaps)

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual intptr_t Hashcode() {
    return first_prototype_unique_id_.Hashcode() * 17 +
           last_prototype_unique_id_.Hashcode();
  }

  virtual void FinalizeUniqueValueId() {
    first_prototype_unique_id_ = UniqueValueId(prototypes_.first());
    last_prototype_unique_id_ = UniqueValueId(prototypes_.last());
  }

  bool CanOmitPrototypeChecks() { return can_omit_prototype_maps_; }

 protected:
  virtual bool DataEquals(HValue* other) {
    HCheckPrototypeMaps* b = HCheckPrototypeMaps::cast(other);
    return first_prototype_unique_id_ == b->first_prototype_unique_id_ &&
           last_prototype_unique_id_ == b->last_prototype_unique_id_;
  }

 private:
  ZoneList<Handle<JSObject> > prototypes_;
  ZoneList<Handle<Map> > maps_;
  UniqueValueId first_prototype_unique_id_;
  UniqueValueId last_prototype_unique_id_;
  bool can_omit_prototype_maps_;
};


class HPhi: public HValue {
 public:
  HPhi(int merged_index, Zone* zone)
      : inputs_(2, zone),
        merged_index_(merged_index),
        phi_id_(-1) {
    for (int i = 0; i < Representation::kNumRepresentations; i++) {
      non_phi_uses_[i] = 0;
      indirect_uses_[i] = 0;
    }
    ASSERT(merged_index >= 0);
    SetFlag(kFlexibleRepresentation);
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual Representation RepresentationFromInputs();

  virtual Range* InferRange(Zone* zone);
  virtual void InferRepresentation(HInferRepresentationPhase* h_infer);
  virtual Representation RequiredInputRepresentation(int index) {
    return representation();
  }
  virtual Representation KnownOptimalRepresentation() {
    return representation();
  }
  virtual HType CalculateInferredType();
  virtual int OperandCount() { return inputs_.length(); }
  virtual HValue* OperandAt(int index) const { return inputs_[index]; }
  HValue* GetRedundantReplacement();
  void AddInput(HValue* value);
  bool HasRealUses();

  bool IsReceiver() const { return merged_index_ == 0; }

  int merged_index() const { return merged_index_; }

  virtual void AddInformativeDefinitions();

  virtual void PrintTo(StringStream* stream);

#ifdef DEBUG
  virtual void Verify();
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
  virtual Opcode opcode() const { return HValue::kPhi; }

  void SimplifyConstantInputs();

  // TODO(titzer): we can't eliminate the receiver for generating backtraces
  virtual bool IsDeletable() const { return !IsReceiver(); }

 protected:
  virtual void DeleteFromGraph();
  virtual void InternalSetOperandAt(int index, HValue* value) {
    inputs_[index] = value;
  }

  virtual bool IsRelationTrueInternal(NumericRelation relation,
                                      HValue* other,
                                      int offset = 0,
                                      int scale = 0);

 private:
  ZoneList<HValue*> inputs_;
  int merged_index_;

  int non_phi_uses_[Representation::kNumRepresentations];
  int indirect_uses_[Representation::kNumRepresentations];
  int phi_id_;
};


class HInductionVariableAnnotation : public HUnaryOperation {
 public:
  static HInductionVariableAnnotation* AddToGraph(HPhi* phi,
                                                  NumericRelation relation,
                                                  int operand_index);

  NumericRelation relation() { return relation_; }
  HValue* induction_base() { return phi_->OperandAt(operand_index_); }

  virtual int RedefinedOperandIndex() { return 0; }
  virtual bool IsPurelyInformativeDefinition() { return true; }
  virtual Representation RequiredInputRepresentation(int index) {
    return representation();
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual bool IsRelationTrueInternal(NumericRelation other_relation,
                                      HValue* other_related_value,
                                      int offset = 0,
                                      int scale = 0) {
    if (induction_base() == other_related_value) {
      return relation().CompoundImplies(other_relation, offset, scale);
    } else {
      return false;
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(InductionVariableAnnotation)

 private:
  HInductionVariableAnnotation(HPhi* phi,
                               NumericRelation relation,
                               int operand_index)
      : HUnaryOperation(phi),
    phi_(phi), relation_(relation), operand_index_(operand_index) {
  }

  // We need to store the phi both here and in the instruction operand because
  // the operand can change if a new idef of the phi is added between the phi
  // and this instruction (inserting an idef updates every use).
  HPhi* phi_;
  NumericRelation relation_;
  int operand_index_;
};


class HArgumentsObject: public HTemplateInstruction<0> {
 public:
  HArgumentsObject(int count, Zone* zone) : values_(count, zone) {
    set_representation(Representation::Tagged());
    SetFlag(kIsArguments);
  }

  const ZoneList<HValue*>* arguments_values() const { return &values_; }
  int arguments_count() const { return values_.length(); }

  void AddArgument(HValue* argument, Zone* zone) {
    values_.Add(NULL, zone);  // Resize list.
    SetOperandAt(values_.length() - 1, argument);
  }

  virtual int OperandCount() { return values_.length(); }
  virtual HValue* OperandAt(int index) const { return values_[index]; }

  virtual bool HasEscapingOperandAt(int index) { return false; }
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsObject)

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    values_[index] = value;
  }

 private:
  virtual bool IsDeletable() const { return true; }

  ZoneList<HValue*> values_;
};


class HConstant: public HTemplateInstruction<0> {
 public:
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
            bool boolean_value);

  Handle<Object> handle() {
    if (handle_.is_null()) {
      Factory* factory = Isolate::Current()->factory();
      // Default arguments to is_not_in_new_space depend on this heap number
      // to be tenured so that it's guaranteed not be be located in new space.
      handle_ = factory->NewNumber(double_value_, TENURED);
    }
    AllowDeferredHandleDereference smi_check;
    ASSERT(has_int32_value_ || !handle_->IsSmi());
    return handle_;
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

    ASSERT(!handle_.is_null());
    Heap* heap = isolate()->heap();
    ASSERT(unique_id_ != UniqueValueId(heap->minus_zero_value()));
    ASSERT(unique_id_ != UniqueValueId(heap->nan_value()));
    return unique_id_ == UniqueValueId(heap->undefined_value()) ||
           unique_id_ == UniqueValueId(heap->null_value()) ||
           unique_id_ == UniqueValueId(heap->true_value()) ||
           unique_id_ == UniqueValueId(heap->false_value()) ||
           unique_id_ == UniqueValueId(heap->the_hole_value()) ||
           unique_id_ == UniqueValueId(heap->empty_string());
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  virtual Representation KnownOptimalRepresentation() {
    if (HasSmiValue()) return Representation::Smi();
    if (HasInteger32Value()) return Representation::Integer32();
    if (HasNumberValue()) return Representation::Double();
    return Representation::Tagged();
  }

  virtual bool EmitAtUses() { return !representation().IsDouble(); }
  virtual void PrintDataTo(StringStream* stream);
  virtual HType CalculateInferredType();
  bool IsInteger() { return handle()->IsSmi(); }
  HConstant* CopyToRepresentation(Representation r, Zone* zone) const;
  HConstant* CopyToTruncatedInt32(Zone* zone) const;
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
    return type_from_value_.IsString();
  }
  Handle<String> StringValue() const {
    ASSERT(HasStringValue());
    return Handle<String>::cast(handle_);
  }
  bool HasInternalizedStringValue() const {
    return HasStringValue() && is_internalized_string_;
  }

  bool BooleanValue() const { return boolean_value_; }

  virtual intptr_t Hashcode() {
    if (has_int32_value_) {
      return static_cast<intptr_t>(int32_value_);
    } else if (has_double_value_) {
      return static_cast<intptr_t>(BitCast<int64_t>(double_value_));
    } else {
      ASSERT(!handle_.is_null());
      return unique_id_.Hashcode();
    }
  }

  virtual void FinalizeUniqueValueId() {
    if (!has_double_value_) {
      ASSERT(!handle_.is_null());
      unique_id_ = UniqueValueId(handle_);
    }
  }

#ifdef DEBUG
  virtual void Verify() { }
#endif

  DECLARE_CONCRETE_INSTRUCTION(Constant)

 protected:
  virtual Range* InferRange(Zone* zone);

  virtual bool DataEquals(HValue* other) {
    HConstant* other_constant = HConstant::cast(other);
    if (has_int32_value_) {
      return other_constant->has_int32_value_ &&
          int32_value_ == other_constant->int32_value_;
    } else if (has_double_value_) {
      return other_constant->has_double_value_ &&
          BitCast<int64_t>(double_value_) ==
          BitCast<int64_t>(other_constant->double_value_);
    } else {
      ASSERT(!handle_.is_null());
      return !other_constant->handle_.is_null() &&
             unique_id_ == other_constant->unique_id_;
    }
  }

 private:
  void Initialize(Representation r);

  virtual bool IsDeletable() const { return true; }

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
  bool is_internalized_string_ : 1;  // TODO(yangguo): make this part of HType.
  bool is_not_in_new_space_ : 1;
  bool boolean_value_ : 1;
  int32_t int32_value_;
  double double_value_;
  HType type_from_value_;
};


class HBinaryOperation: public HTemplateInstruction<3> {
 public:
  HBinaryOperation(HValue* context, HValue* left, HValue* right)
      : observed_output_representation_(Representation::None()) {
    ASSERT(left != NULL && right != NULL);
    SetOperandAt(0, context);
    SetOperandAt(1, left);
    SetOperandAt(2, right);
    observed_input_representation_[0] = Representation::None();
    observed_input_representation_[1] = Representation::None();
  }

  HValue* context() { return OperandAt(0); }
  HValue* left() { return OperandAt(1); }
  HValue* right() { return OperandAt(2); }

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
    return (right()->UseCount() == 1);
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

  virtual Representation observed_input_representation(int index) {
    if (index == 0) return Representation::Tagged();
    return observed_input_representation_[index - 1];
  }

  virtual void InferRepresentation(HInferRepresentationPhase* h_infer);
  virtual Representation RepresentationFromInputs();
  virtual void AssumeRepresentation(Representation r);

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) {
    // By default, binary operations don't handle Smis.
    if (new_rep.IsSmi()) {
      new_rep = Representation::Integer32();
    }
    HValue::UpdateRepresentation(new_rep, h_infer, reason);
  }

  virtual bool IsCommutative() const { return false; }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_ABSTRACT_INSTRUCTION(BinaryOperation)

 private:
  bool IgnoreObservedOutputRepresentation(Representation current_rep);

  Representation observed_input_representation_[2];
  Representation observed_output_representation_;
};


class HWrapReceiver: public HTemplateInstruction<2> {
 public:
  HWrapReceiver(HValue* receiver, HValue* function) {
    set_representation(Representation::Tagged());
    SetOperandAt(0, receiver);
    SetOperandAt(1, function);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* receiver() { return OperandAt(0); }
  HValue* function() { return OperandAt(1); }

  virtual HValue* Canonicalize();

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(WrapReceiver)
};


class HApplyArguments: public HTemplateInstruction<4> {
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

  virtual Representation RequiredInputRepresentation(int index) {
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


class HArgumentsElements: public HTemplateInstruction<0> {
 public:
  explicit HArgumentsElements(bool from_inlined) : from_inlined_(from_inlined) {
    // The value produced by this instruction is a pointer into the stack
    // that looks as if it was a smi because of alignment.
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements)

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  bool from_inlined() const { return from_inlined_; }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }

  bool from_inlined_;
};


class HArgumentsLength: public HUnaryOperation {
 public:
  explicit HArgumentsLength(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HAccessArgumentsAt: public HTemplateInstruction<3> {
 public:
  HAccessArgumentsAt(HValue* arguments, HValue* length, HValue* index) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetOperandAt(0, arguments);
    SetOperandAt(1, length);
    SetOperandAt(2, index);
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    // The arguments elements is considered tagged.
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }

  HValue* arguments() { return OperandAt(0); }
  HValue* length() { return OperandAt(1); }
  HValue* index() { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(AccessArgumentsAt)

  virtual bool DataEquals(HValue* other) { return true; }
};


class HBoundsCheckBaseIndexInformation;


class HBoundsCheck: public HTemplateInstruction<2> {
 public:
  // Normally HBoundsCheck should be created using the
  // HGraphBuilder::AddBoundsCheck() helper.
  // However when building stubs, where we know that the arguments are Int32,
  // it makes sense to invoke this constructor directly.
  HBoundsCheck(HValue* index, HValue* length)
    : skip_check_(false),
      base_(NULL), offset_(0), scale_(0),
      responsibility_direction_(DIRECTION_NONE) {
    SetOperandAt(0, index);
    SetOperandAt(1, length);
    SetFlag(kFlexibleRepresentation);
    SetFlag(kUseGVN);
  }

  bool skip_check() { return skip_check_; }
  void set_skip_check(bool skip_check) { skip_check_ = skip_check; }
  HValue* base() { return base_; }
  int offset() { return offset_; }
  int scale() { return scale_; }
  bool index_can_increase() {
    return (responsibility_direction_ & DIRECTION_LOWER) == 0;
  }
  bool index_can_decrease() {
    return (responsibility_direction_ & DIRECTION_UPPER) == 0;
  }

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

  virtual Representation RequiredInputRepresentation(int arg_index) {
    return representation();
  }

  virtual bool IsRelationTrueInternal(NumericRelation relation,
                                      HValue* related_value,
                                      int offset = 0,
                                      int scale = 0);

  virtual void PrintDataTo(StringStream* stream);
  virtual void InferRepresentation(HInferRepresentationPhase* h_infer);

  HValue* index() { return OperandAt(0); }
  HValue* length() { return OperandAt(1); }

  virtual int RedefinedOperandIndex() { return 0; }
  virtual bool IsPurelyInformativeDefinition() { return skip_check(); }
  virtual void AddInformativeDefinitions();

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheck)

 protected:
  friend class HBoundsCheckBaseIndexInformation;

  virtual void SetResponsibilityForRange(RangeGuaranteeDirection direction) {
    responsibility_direction_ = static_cast<RangeGuaranteeDirection>(
        responsibility_direction_ | direction);
  }

  virtual bool DataEquals(HValue* other) { return true; }
  virtual void TryGuaranteeRangeChanging(RangeEvaluationContext* context);
  bool skip_check_;
  HValue* base_;
  int offset_;
  int scale_;
  RangeGuaranteeDirection responsibility_direction_;
};


class HBoundsCheckBaseIndexInformation: public HTemplateInstruction<2> {
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

  virtual Representation RequiredInputRepresentation(int arg_index) {
    return representation();
  }

  virtual bool IsRelationTrueInternal(NumericRelation relation,
                                      HValue* related_value,
                                      int offset = 0,
                                      int scale = 0);
  virtual void PrintDataTo(StringStream* stream);

  virtual int RedefinedOperandIndex() { return 0; }
  virtual bool IsPurelyInformativeDefinition() { return true; }

 protected:
  virtual void SetResponsibilityForRange(RangeGuaranteeDirection direction) {
    bounds_check()->SetResponsibilityForRange(direction);
  }
  virtual void TryGuaranteeRangeChanging(RangeEvaluationContext* context) {
    bounds_check()->TryGuaranteeRangeChanging(context);
  }
};


class HBitwiseBinaryOperation: public HBinaryOperation {
 public:
  HBitwiseBinaryOperation(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    SetFlag(kFlexibleRepresentation);
    SetFlag(kTruncatingToInt32);
    SetFlag(kAllowUndefinedAsNaN);
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return index == 0
        ? Representation::Tagged()
        : representation();
  }

  virtual void RepresentationChanged(Representation to) {
    if (!to.IsTagged()) {
      ASSERT(to.IsInteger32());
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    } else {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    }
  }

  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason) {
    // We only generate either int32 or generic tagged bitwise operations.
    if (new_rep.IsSmi() || new_rep.IsDouble()) {
      new_rep = Representation::Integer32();
    }
    HValue::UpdateRepresentation(new_rep, h_infer, reason);
  }

  virtual void initialize_output_representation(Representation observed) {
    if (observed.IsDouble()) observed = Representation::Integer32();
    HBinaryOperation::initialize_output_representation(observed);
  }

  virtual HType CalculateInferredType();

  DECLARE_ABSTRACT_INSTRUCTION(BitwiseBinaryOperation)

 private:
  virtual bool IsDeletable() const { return true; }
};


class HMathFloorOfDiv: public HBinaryOperation {
 public:
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

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Integer32();
  }

  DECLARE_CONCRETE_INSTRUCTION(MathFloorOfDiv)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HArithmeticBinaryOperation: public HBinaryOperation {
 public:
  HArithmeticBinaryOperation(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    SetAllSideEffects();
    SetFlag(kFlexibleRepresentation);
    SetFlag(kAllowUndefinedAsNaN);
  }

  virtual void RepresentationChanged(Representation to) {
    if (to.IsTagged()) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
  }

  virtual HType CalculateInferredType();
  virtual Representation RequiredInputRepresentation(int index) {
    return index == 0
        ? Representation::Tagged()
        : representation();
  }

  DECLARE_ABSTRACT_INSTRUCTION(ArithmeticBinaryOperation)

 private:
  virtual bool IsDeletable() const { return true; }
};


class HCompareGeneric: public HBinaryOperation {
 public:
  HCompareGeneric(HValue* context,
                  HValue* left,
                  HValue* right,
                  Token::Value token)
      : HBinaryOperation(context, left, right), token_(token) {
    ASSERT(Token::IsCompareOp(token));
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return index == 0
        ? Representation::Tagged()
        : representation();
  }

  Token::Value token() const { return token_; }
  virtual void PrintDataTo(StringStream* stream);

  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(CompareGeneric)

 private:
  Token::Value token_;
};


class HCompareIDAndBranch: public HTemplateControlInstruction<2, 2> {
 public:
  HCompareIDAndBranch(HValue* left, HValue* right, Token::Value token)
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

  virtual void InferRepresentation(HInferRepresentationPhase* h_infer);

  virtual Representation RequiredInputRepresentation(int index) {
    return representation();
  }
  virtual Representation observed_input_representation(int index) {
    return observed_input_representation_[index];
  }
  virtual void PrintDataTo(StringStream* stream);

  virtual void AddInformativeDefinitions();

  DECLARE_CONCRETE_INSTRUCTION(CompareIDAndBranch)

 private:
  Representation observed_input_representation_[2];
  Token::Value token_;
};


class HCompareObjectEqAndBranch: public HTemplateControlInstruction<2, 2> {
 public:
  HCompareObjectEqAndBranch(HValue* left, HValue* right) {
    SetOperandAt(0, left);
    SetOperandAt(1, right);
  }

  HValue* left() { return OperandAt(0); }
  HValue* right() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual Representation observed_input_representation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareObjectEqAndBranch)
};


class HCompareConstantEqAndBranch: public HUnaryControlInstruction {
 public:
  HCompareConstantEqAndBranch(HValue* left, int right, Token::Value op)
      : HUnaryControlInstruction(left, NULL, NULL), op_(op), right_(right) {
    ASSERT(op == Token::EQ_STRICT);
  }

  Token::Value op() const { return op_; }
  HValue* left() { return value(); }
  int right() const { return right_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Integer32();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareConstantEqAndBranch);

 private:
  const Token::Value op_;
  const int right_;
};


class HIsObjectAndBranch: public HUnaryControlInstruction {
 public:
  explicit HIsObjectAndBranch(HValue* value)
    : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsObjectAndBranch)
};

class HIsStringAndBranch: public HUnaryControlInstruction {
 public:
  explicit HIsStringAndBranch(HValue* value)
    : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsStringAndBranch)
};


class HIsSmiAndBranch: public HUnaryControlInstruction {
 public:
  explicit HIsSmiAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch)

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HIsUndetectableAndBranch: public HUnaryControlInstruction {
 public:
  explicit HIsUndetectableAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsUndetectableAndBranch)
};


class HStringCompareAndBranch: public HTemplateControlInstruction<2, 3> {
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
  }

  HValue* context() { return OperandAt(0); }
  HValue* left() { return OperandAt(1); }
  HValue* right() { return OperandAt(2); }
  Token::Value token() const { return token_; }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  Representation GetInputRepresentation() const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StringCompareAndBranch)

 private:
  Token::Value token_;
};


class HIsConstructCallAndBranch: public HTemplateControlInstruction<2, 0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsConstructCallAndBranch)
};


class HHasInstanceTypeAndBranch: public HUnaryControlInstruction {
 public:
  HHasInstanceTypeAndBranch(HValue* value, InstanceType type)
      : HUnaryControlInstruction(value, NULL, NULL), from_(type), to_(type) { }
  HHasInstanceTypeAndBranch(HValue* value, InstanceType from, InstanceType to)
      : HUnaryControlInstruction(value, NULL, NULL), from_(from), to_(to) {
    ASSERT(to == LAST_TYPE);  // Others not implemented yet in backend.
  }

  InstanceType from() { return from_; }
  InstanceType to() { return to_; }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceTypeAndBranch)

 private:
  InstanceType from_;
  InstanceType to_;  // Inclusive range, not all combinations work.
};


class HHasCachedArrayIndexAndBranch: public HUnaryControlInstruction {
 public:
  explicit HHasCachedArrayIndexAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndexAndBranch)
};


class HGetCachedArrayIndex: public HUnaryOperation {
 public:
  explicit HGetCachedArrayIndex(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(GetCachedArrayIndex)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  virtual bool IsDeletable() const { return true; }
};


class HClassOfTestAndBranch: public HUnaryControlInstruction {
 public:
  HClassOfTestAndBranch(HValue* value, Handle<String> class_name)
      : HUnaryControlInstruction(value, NULL, NULL),
        class_name_(class_name) { }

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch)

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  Handle<String> class_name() const { return class_name_; }

 private:
  Handle<String> class_name_;
};


class HTypeofIsAndBranch: public HUnaryControlInstruction {
 public:
  HTypeofIsAndBranch(HValue* value, Handle<String> type_literal)
      : HUnaryControlInstruction(value, NULL, NULL),
        type_literal_(type_literal) { }

  Handle<String> type_literal() { return type_literal_; }
  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(TypeofIsAndBranch)

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

 private:
  Handle<String> type_literal_;
};


class HInstanceOf: public HBinaryOperation {
 public:
  HInstanceOf(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType();

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(InstanceOf)
};


class HInstanceOfKnownGlobal: public HTemplateInstruction<2> {
 public:
  HInstanceOfKnownGlobal(HValue* context,
                         HValue* left,
                         Handle<JSFunction> right)
      : function_(right) {
    SetOperandAt(0, context);
    SetOperandAt(1, left);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  HValue* context() { return OperandAt(0); }
  HValue* left() { return OperandAt(1); }
  Handle<JSFunction> function() { return function_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfKnownGlobal)

 private:
  Handle<JSFunction> function_;
};


// TODO(mstarzinger): This instruction should be modeled as a load of the map
// field followed by a load of the instance size field once HLoadNamedField is
// flexible enough to accommodate byte-field loads.
class HInstanceSize: public HTemplateInstruction<1> {
 public:
  explicit HInstanceSize(HValue* object) {
    SetOperandAt(0, object);
    set_representation(Representation::Integer32());
  }

  HValue* object() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(InstanceSize)
};


class HPower: public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone, HValue* left, HValue* right);

  HValue* left() { return OperandAt(0); }
  HValue* right() const { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) {
    return index == 0
      ? Representation::Double()
      : Representation::None();
  }
  virtual Representation observed_input_representation(int index) {
    return RequiredInputRepresentation(index);
  }

  DECLARE_CONCRETE_INSTRUCTION(Power)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  HPower(HValue* left, HValue* right) {
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    set_representation(Representation::Double());
    SetFlag(kUseGVN);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  virtual bool IsDeletable() const {
    return !right()->representation().IsTagged();
  }
};


class HRandom: public HTemplateInstruction<1> {
 public:
  explicit HRandom(HValue* global_object) {
    SetOperandAt(0, global_object);
    set_representation(Representation::Double());
  }

  HValue* global_object() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Random)

 private:
  virtual bool IsDeletable() const { return true; }
};


class HAdd: public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  // Add is only commutative if two integer values are added and not if two
  // tagged values are added (because it might be a String concatenation).
  virtual bool IsCommutative() const {
    return !representation().IsTagged();
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual HType CalculateInferredType();

  virtual HValue* Canonicalize();

  virtual bool TryDecompose(DecompositionResult* decomposition) {
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

  DECLARE_CONCRETE_INSTRUCTION(Add)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange(Zone* zone);

 private:
  HAdd(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HSub: public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual HValue* Canonicalize();

  virtual bool TryDecompose(DecompositionResult* decomposition) {
    if (right()->IsInteger32Constant()) {
      decomposition->Apply(left(), -right()->GetInteger32Constant());
      return true;
    } else {
      return false;
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(Sub)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange(Zone* zone);

 private:
  HSub(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HMul: public HArithmeticBinaryOperation {
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

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual HValue* Canonicalize();

  // Only commutative if it is certain that not two objects are multiplicated.
  virtual bool IsCommutative() const {
    return !representation().IsTagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Mul)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange(Zone* zone);

 private:
  HMul(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HMod: public HArithmeticBinaryOperation {
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

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual HValue* Canonicalize();

  DECLARE_CONCRETE_INSTRUCTION(Mod)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange(Zone* zone);

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


class HDiv: public HArithmeticBinaryOperation {
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

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual HValue* Canonicalize();

  DECLARE_CONCRETE_INSTRUCTION(Div)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange(Zone* zone);

 private:
  HDiv(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
  }
};


class HMathMinMax: public HArithmeticBinaryOperation {
 public:
  enum Operation { kMathMin, kMathMax };

  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right,
                           Operation op);

  virtual Representation RequiredInputRepresentation(int index) {
    return index == 0 ? Representation::Tagged()
                      : representation();
  }

  virtual Representation observed_input_representation(int index) {
    return RequiredInputRepresentation(index);
  }

  virtual void InferRepresentation(HInferRepresentationPhase* h_infer);

  virtual Representation RepresentationFromInputs() {
    Representation left_rep = left()->representation();
    Representation right_rep = right()->representation();
    if ((left_rep.IsNone() || left_rep.IsInteger32()) &&
        (right_rep.IsNone() || right_rep.IsInteger32())) {
      return Representation::Integer32();
    }
    return Representation::Double();
  }

  virtual bool IsCommutative() const { return true; }

  Operation operation() { return operation_; }

  DECLARE_CONCRETE_INSTRUCTION(MathMinMax)

 protected:
  virtual bool DataEquals(HValue* other) {
    return other->IsMathMinMax() &&
        HMathMinMax::cast(other)->operation_ == operation_;
  }

  virtual Range* InferRange(Zone* zone);

 private:
  HMathMinMax(HValue* context, HValue* left, HValue* right, Operation op)
      : HArithmeticBinaryOperation(context, left, right),
        operation_(op) { }

  Operation operation_;
};


class HBitwise: public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           Token::Value op,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  Token::Value op() const { return op_; }

  virtual bool IsCommutative() const { return true; }

  virtual HValue* Canonicalize();

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(Bitwise)

 protected:
  virtual bool DataEquals(HValue* other) {
    return op() == HBitwise::cast(other)->op();
  }

  virtual Range* InferRange(Zone* zone);

 private:
  HBitwise(Token::Value op, HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right), op_(op) {
    ASSERT(op == Token::BIT_AND || op == Token::BIT_OR || op == Token::BIT_XOR);
  }

  Token::Value op_;
};


class HShl: public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual Range* InferRange(Zone* zone);

  DECLARE_CONCRETE_INSTRUCTION(Shl)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  HShl(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }
};


class HShr: public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual bool TryDecompose(DecompositionResult* decomposition) {
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

  virtual Range* InferRange(Zone* zone);

  DECLARE_CONCRETE_INSTRUCTION(Shr)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  HShr(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }
};


class HSar: public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual bool TryDecompose(DecompositionResult* decomposition) {
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

  virtual Range* InferRange(Zone* zone);

  DECLARE_CONCRETE_INSTRUCTION(Sar)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

 private:
  HSar(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }
};


class HRor: public HBitwiseBinaryOperation {
 public:
  HRor(HValue* context, HValue* left, HValue* right)
       : HBitwiseBinaryOperation(context, left, right) {
    ChangeRepresentation(Representation::Integer32());
  }

  DECLARE_CONCRETE_INSTRUCTION(Ror)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HOsrEntry: public HTemplateInstruction<0> {
 public:
  explicit HOsrEntry(BailoutId ast_id) : ast_id_(ast_id) {
    SetGVNFlag(kChangesOsrEntries);
  }

  BailoutId ast_id() const { return ast_id_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(OsrEntry)

 private:
  BailoutId ast_id_;
};


class HParameter: public HTemplateInstruction<0> {
 public:
  enum ParameterKind {
    STACK_PARAMETER,
    REGISTER_PARAMETER
  };

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

  unsigned index() const { return index_; }
  ParameterKind kind() const { return kind_; }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Parameter)

 private:
  unsigned index_;
  ParameterKind kind_;
};


class HCallStub: public HUnaryCall {
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

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallStub)

 private:
  CodeStub::Major major_key_;
  TranscendentalCache::Type transcendental_type_;
};


class HUnknownOSRValue: public HTemplateInstruction<0> {
 public:
  HUnknownOSRValue()
      : incoming_value_(NULL) {
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  void set_incoming_value(HPhi* value) {
    incoming_value_ = value;
  }

  HPhi* incoming_value() {
    return incoming_value_;
  }

  virtual Representation KnownOptimalRepresentation() {
    if (incoming_value_ == NULL) return Representation::None();
    return incoming_value_->KnownOptimalRepresentation();
  }

  DECLARE_CONCRETE_INSTRUCTION(UnknownOSRValue)

 private:
  HPhi* incoming_value_;
};


class HLoadGlobalCell: public HTemplateInstruction<0> {
 public:
  HLoadGlobalCell(Handle<Cell> cell, PropertyDetails details)
      : cell_(cell), details_(details), unique_id_() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnGlobalVars);
  }

  Handle<Cell> cell() const { return cell_; }
  bool RequiresHoleCheck() const;

  virtual void PrintDataTo(StringStream* stream);

  virtual intptr_t Hashcode() {
    return unique_id_.Hashcode();
  }

  virtual void FinalizeUniqueValueId() {
    unique_id_ = UniqueValueId(cell_);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalCell)

 protected:
  virtual bool DataEquals(HValue* other) {
    HLoadGlobalCell* b = HLoadGlobalCell::cast(other);
    return unique_id_ == b->unique_id_;
  }

 private:
  virtual bool IsDeletable() const { return !RequiresHoleCheck(); }

  Handle<Cell> cell_;
  PropertyDetails details_;
  UniqueValueId unique_id_;
};


class HLoadGlobalGeneric: public HTemplateInstruction<2> {
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

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalGeneric)

 private:
  Handle<Object> name_;
  bool for_typeof_;
};


class HAllocateObject: public HTemplateInstruction<1> {
 public:
  HAllocateObject(HValue* context, Handle<JSFunction> constructor)
      : constructor_(constructor) {
    SetOperandAt(0, context);
    set_representation(Representation::Tagged());
    SetGVNFlag(kChangesNewSpacePromotion);
    constructor_initial_map_ = constructor->has_initial_map()
        ? Handle<Map>(constructor->initial_map())
        : Handle<Map>::null();
    // If slack tracking finished, the instance size and property counts
    // remain unchanged so that we can allocate memory for the object.
    ASSERT(!constructor->shared()->IsInobjectSlackTrackingInProgress());
  }

  // Maximum instance size for which allocations will be inlined.
  static const int kMaxSize = 64 * kPointerSize;

  HValue* context() { return OperandAt(0); }
  Handle<JSFunction> constructor() { return constructor_; }
  Handle<Map> constructor_initial_map() { return constructor_initial_map_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }
  virtual Handle<Map> GetMonomorphicJSObjectMap() {
    ASSERT(!constructor_initial_map_.is_null());
    return constructor_initial_map_;
  }
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(AllocateObject)

 private:
  // TODO(svenpanne) Might be safe, but leave it out until we know for sure.
  //  virtual bool IsDeletable() const { return true; }

  Handle<JSFunction> constructor_;
  Handle<Map> constructor_initial_map_;
};


class HAllocate: public HTemplateInstruction<2> {
 public:
  enum Flags {
    CAN_ALLOCATE_IN_NEW_SPACE = 1 << 0,
    CAN_ALLOCATE_IN_OLD_DATA_SPACE = 1 << 1,
    CAN_ALLOCATE_IN_OLD_POINTER_SPACE = 1 << 2,
    ALLOCATE_DOUBLE_ALIGNED = 1 << 3
  };

  HAllocate(HValue* context, HValue* size, HType type, Flags flags)
      : type_(type),
        flags_(flags) {
    SetOperandAt(0, context);
    SetOperandAt(1, size);
    set_representation(Representation::Tagged());
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  static Flags DefaultFlags() {
    return CAN_ALLOCATE_IN_NEW_SPACE;
  }

  static Flags DefaultFlags(ElementsKind kind) {
    Flags flags = CAN_ALLOCATE_IN_NEW_SPACE;
    if (IsFastDoubleElementsKind(kind)) {
      flags = static_cast<HAllocate::Flags>(
          flags | HAllocate::ALLOCATE_DOUBLE_ALIGNED);
    }
    return flags;
  }

  HValue* context() { return OperandAt(0); }
  HValue* size() { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      return Representation::Integer32();
    }
  }

  virtual HType CalculateInferredType();

  bool CanAllocateInNewSpace() const {
    return (flags_ & CAN_ALLOCATE_IN_NEW_SPACE) != 0;
  }

  bool CanAllocateInOldDataSpace() const {
    return (flags_ & CAN_ALLOCATE_IN_OLD_DATA_SPACE) != 0;
  }

  bool CanAllocateInOldPointerSpace() const {
    return (flags_ & CAN_ALLOCATE_IN_OLD_POINTER_SPACE) != 0;
  }

  bool CanAllocateInOldSpace() const {
    return CanAllocateInOldDataSpace() ||
        CanAllocateInOldPointerSpace();
  }

  bool GuaranteedInNewSpace() const {
    return CanAllocateInNewSpace() && !CanAllocateInOldSpace();
  }

  bool MustAllocateDoubleAligned() const {
    return (flags_ & ALLOCATE_DOUBLE_ALIGNED) != 0;
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(Allocate)

 private:
  HType type_;
  Flags flags_;
};


class HInnerAllocatedObject: public HTemplateInstruction<1> {
 public:
  HInnerAllocatedObject(HValue* value, int offset)
      : offset_(offset) {
    ASSERT(value->IsAllocate());
    SetOperandAt(0, value);
    set_representation(Representation::Tagged());
  }

  HValue* base_object() { return OperandAt(0); }
  int offset() { return offset_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(InnerAllocatedObject)

 private:
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
  if (object != new_space_dominator) return true;
  if (object->IsAllocateObject()) return false;
  if (object->IsAllocate()) {
    return !HAllocate::cast(object)->GuaranteedInNewSpace();
  }
  return true;
}


class HStoreGlobalCell: public HUnaryOperation {
 public:
  HStoreGlobalCell(HValue* value,
                   Handle<PropertyCell> cell,
                   PropertyDetails details)
      : HUnaryOperation(value),
        cell_(cell),
        details_(details) {
    SetGVNFlag(kChangesGlobalVars);
  }

  Handle<PropertyCell> cell() const { return cell_; }
  bool RequiresHoleCheck() {
    return !details_.IsDontDelete() || details_.IsReadOnly();
  }
  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobalCell)

 private:
  Handle<PropertyCell> cell_;
  PropertyDetails details_;
};


class HStoreGlobalGeneric: public HTemplateInstruction<3> {
 public:
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

  HValue* context() { return OperandAt(0); }
  HValue* global_object() { return OperandAt(1); }
  Handle<Object> name() const { return name_; }
  HValue* value() { return OperandAt(2); }
  StrictModeFlag strict_mode_flag() { return strict_mode_flag_; }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobalGeneric)

 private:
  Handle<Object> name_;
  StrictModeFlag strict_mode_flag_;
};


class HLoadContextSlot: public HUnaryOperation {
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

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(LoadContextSlot)

 protected:
  virtual bool DataEquals(HValue* other) {
    HLoadContextSlot* b = HLoadContextSlot::cast(other);
    return (slot_index() == b->slot_index());
  }

 private:
  virtual bool IsDeletable() const { return !RequiresHoleCheck(); }

  int slot_index_;
  Mode mode_;
};


class HStoreContextSlot: public HTemplateInstruction<2> {
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

  HStoreContextSlot(HValue* context, int slot_index, Mode mode, HValue* value)
      : slot_index_(slot_index), mode_(mode) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    SetGVNFlag(kChangesContextSlots);
  }

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

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreContextSlot)

 private:
  int slot_index_;
  Mode mode_;
};


// Represents an access to a portion of an object, such as the map pointer,
// array elements pointer, etc, but not accesses to array elements themselves.
class HObjectAccess {
 public:
  inline bool IsInobject() const {
    return portion() != kBackingStore;
  }

  inline int offset() const {
    return OffsetField::decode(value_);
  }

  inline Handle<String> name() const {
    return name_;
  }

  static HObjectAccess ForHeapNumberValue() {
    return HObjectAccess(kDouble, HeapNumber::kValueOffset);
  }

  static HObjectAccess ForElementsPointer() {
    return HObjectAccess(kElementsPointer, JSObject::kElementsOffset);
  }

  static HObjectAccess ForArrayLength() {
    return HObjectAccess(kArrayLengths, JSArray::kLengthOffset);
  }

  static HObjectAccess ForFixedArrayLength() {
    return HObjectAccess(kArrayLengths, FixedArray::kLengthOffset);
  }

  static HObjectAccess ForPropertiesPointer() {
    return HObjectAccess(kInobject, JSObject::kPropertiesOffset);
  }

  static HObjectAccess ForPrototypeOrInitialMap() {
    return HObjectAccess(kInobject, JSFunction::kPrototypeOrInitialMapOffset);
  }

  static HObjectAccess ForMap() {
    return HObjectAccess(kMaps, JSObject::kMapOffset);
  }

  static HObjectAccess ForAllocationSitePayload() {
    return HObjectAccess(kInobject, AllocationSiteInfo::kPayloadOffset);
  }

  // Create an access to an offset in a fixed array header.
  static HObjectAccess ForFixedArrayHeader(int offset);

  // Create an access to an in-object property in a JSObject.
  static HObjectAccess ForJSObjectOffset(int offset);

  // Create an access to an in-object property in a JSArray.
  static HObjectAccess ForJSArrayOffset(int offset);

  // Create an access to the backing store of an object.
  static HObjectAccess ForBackingStoreOffset(int offset);

  // Create an access to a resolved field (in-object or backing store).
  static HObjectAccess ForField(Handle<Map> map,
      LookupResult *lookup, Handle<String> name = Handle<String>::null());

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
    kElementsPointer,  // elements pointer
    kBackingStore,     // some field in the backing store
    kDouble,           // some double field
    kInobject          // some other in-object field
  };

  HObjectAccess(Portion portion, int offset,
      Handle<String> name = Handle<String>::null())
    : value_(PortionField::encode(portion) | OffsetField::encode(offset)),
      name_(name) {
    ASSERT(this->offset() == offset);    // offset should decode correctly
    ASSERT(this->portion() == portion);  // portion should decode correctly
  }

  class PortionField : public BitField<Portion, 0, 3> {};
  class OffsetField : public BitField<int, 3, 29> {};

  uint32_t value_;  // encodes both portion and offset
  Handle<String> name_;

  friend class HLoadNamedField;
  friend class HStoreNamedField;

  inline Portion portion() const {
    return PortionField::decode(value_);
  }
};


class HLoadNamedField: public HTemplateInstruction<2> {
 public:
  HLoadNamedField(HValue* object,
                  HObjectAccess access,
                  HValue* typecheck = NULL,
                  Representation field_representation
                      = Representation::Tagged())
      : access_(access),
        field_representation_(field_representation) {
    ASSERT(object != NULL);
    SetOperandAt(0, object);
    SetOperandAt(1, typecheck != NULL ? typecheck : object);

    if (FLAG_track_fields && field_representation.IsSmi()) {
      set_type(HType::Smi());
      set_representation(field_representation);
    } else if (FLAG_track_double_fields && field_representation.IsDouble()) {
      set_representation(field_representation);
    } else if (FLAG_track_heap_object_fields &&
               field_representation.IsHeapObject()) {
      set_type(HType::NonPrimitive());
      set_representation(Representation::Tagged());
    } else {
      set_representation(Representation::Tagged());
    }
    access.SetGVNFlags(this, false);
  }

  HValue* object() { return OperandAt(0); }
  HValue* typecheck() {
    ASSERT(HasTypeCheck());
    return OperandAt(1);
  }

  bool HasTypeCheck() const { return OperandAt(0) != OperandAt(1); }
  HObjectAccess access() const { return access_; }
  Representation field_representation() const { return representation_; }

  virtual bool HasEscapingOperandAt(int index) { return false; }
  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField)

 protected:
  virtual bool DataEquals(HValue* other) {
    HLoadNamedField* b = HLoadNamedField::cast(other);
    return access_.Equals(b->access_);
  }

 private:
  virtual bool IsDeletable() const { return true; }

  HObjectAccess access_;
  Representation field_representation_;
};


class HLoadNamedFieldPolymorphic: public HTemplateInstruction<2> {
 public:
  HLoadNamedFieldPolymorphic(HValue* context,
                             HValue* object,
                             SmallMapList* types,
                             Handle<String> name,
                             Zone* zone);

  HValue* context() { return OperandAt(0); }
  HValue* object() { return OperandAt(1); }
  SmallMapList* types() { return &types_; }
  Handle<String> name() { return name_; }
  bool need_generic() { return need_generic_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedFieldPolymorphic)

  static const int kMaxLoadPolymorphism = 4;

  virtual void FinalizeUniqueValueId();

 protected:
  virtual bool DataEquals(HValue* value);

 private:
  SmallMapList types_;
  Handle<String> name_;
  ZoneList<UniqueValueId> types_unique_ids_;
  UniqueValueId name_unique_id_;
  bool need_generic_;
};



class HLoadNamedGeneric: public HTemplateInstruction<2> {
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

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedGeneric)

 private:
  Handle<Object> name_;
};


class HLoadFunctionPrototype: public HUnaryOperation {
 public:
  explicit HLoadFunctionPrototype(HValue* function)
      : HUnaryOperation(function) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnCalls);
  }

  HValue* function() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
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
    return r.IsInteger32() ? Representation::Integer32()
                           : Representation::Smi();
  }
};


enum LoadKeyedHoleMode {
  NEVER_RETURN_HOLE,
  ALLOW_RETURN_HOLE
};


class HLoadKeyed
    : public HTemplateInstruction<3>, public ArrayInstructionInterface {
 public:
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

      SetGVNFlag(kDependsOnSpecializedArrayElements);
      // Native code could change the specialized array.
      SetGVNFlag(kDependsOnCalls);
    }

    SetFlag(kUseGVN);
  }

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

  virtual Representation RequiredInputRepresentation(int index) {
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

  virtual Representation observed_input_representation(int index) {
    return RequiredInputRepresentation(index);
  }

  virtual void PrintDataTo(StringStream* stream);

  bool UsesMustHandleHole() const;
  bool AllUsesCanTreatHoleAsNaN() const;
  bool RequiresHoleCheck() const;

  virtual Range* InferRange(Zone* zone);

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyed)

 protected:
  virtual bool DataEquals(HValue* other) {
    if (!other->IsLoadKeyed()) return false;
    HLoadKeyed* other_load = HLoadKeyed::cast(other);

    if (IsDehoisted() && index_offset() != other_load->index_offset())
      return false;
    return elements_kind() == other_load->elements_kind();
  }

 private:
  virtual bool IsDeletable() const {
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


class HLoadKeyedGeneric: public HTemplateInstruction<3> {
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

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    // tagged[tagged]
    return Representation::Tagged();
  }

  virtual HValue* Canonicalize();

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedGeneric)
};


class HStoreNamedField: public HTemplateInstruction<2> {
 public:
  HStoreNamedField(HValue* obj,
                   HObjectAccess access,
                   HValue* val,
                   Representation field_representation
                       = Representation::Tagged())
      : access_(access),
        field_representation_(field_representation),
        transition_(),
        transition_unique_id_(),
        new_space_dominator_(NULL) {
    SetOperandAt(0, obj);
    SetOperandAt(1, val);
    access.SetGVNFlags(this, true);
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField)

  virtual bool HasEscapingOperandAt(int index) { return index == 1; }
  virtual Representation RequiredInputRepresentation(int index) {
    if (FLAG_track_double_fields &&
        index == 1 && field_representation_.IsDouble()) {
      return field_representation_;
    } else if (FLAG_track_fields &&
               index == 1 && field_representation_.IsSmi()) {
      return field_representation_;
    }
    return Representation::Tagged();
  }
  virtual void SetSideEffectDominator(GVNFlag side_effect, HValue* dominator) {
    ASSERT(side_effect == kChangesNewSpacePromotion);
    new_space_dominator_ = dominator;
  }
  virtual void PrintDataTo(StringStream* stream);

  HValue* object() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  HObjectAccess access() const { return access_; }
  Handle<Map> transition() const { return transition_; }
  UniqueValueId transition_unique_id() const { return transition_unique_id_; }
  void SetTransition(Handle<Map> map, CompilationInfo* info) {
    ASSERT(transition_.is_null());  // Only set once.
    if (map->CanBeDeprecated()) {
      map->AddDependentCompilationInfo(DependentCode::kTransitionGroup, info);
    }
    transition_ = map;
  }
  HValue* new_space_dominator() const { return new_space_dominator_; }

  bool NeedsWriteBarrier() {
    ASSERT(!(FLAG_track_double_fields && field_representation_.IsDouble()) ||
           transition_.is_null());
    return (!FLAG_track_fields || !field_representation_.IsSmi()) &&
        // If there is a transition, a new storage object needs to be allocated.
        !(FLAG_track_double_fields && field_representation_.IsDouble()) &&
        StoringValueNeedsWriteBarrier(value()) &&
        ReceiverObjectNeedsWriteBarrier(object(), new_space_dominator());
  }

  bool NeedsWriteBarrierForMap() {
    return ReceiverObjectNeedsWriteBarrier(object(), new_space_dominator());
  }

  virtual void FinalizeUniqueValueId() {
    transition_unique_id_ = UniqueValueId(transition_);
  }

  Representation field_representation() const {
    return field_representation_;
  }

 private:
  HObjectAccess access_;
  Representation field_representation_;
  Handle<Map> transition_;
  UniqueValueId transition_unique_id_;
  HValue* new_space_dominator_;
};


class HStoreNamedGeneric: public HTemplateInstruction<3> {
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

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedGeneric)

 private:
  Handle<String> name_;
  StrictModeFlag strict_mode_flag_;
};


class HStoreKeyed
    : public HTemplateInstruction<3>, public ArrayInstructionInterface {
 public:
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
      SetGVNFlag(kChangesSpecializedArrayElements);
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

  virtual bool HasEscapingOperandAt(int index) { return index != 0; }
  virtual Representation RequiredInputRepresentation(int index) {
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

  virtual Representation observed_input_representation(int index) {
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

  virtual void SetSideEffectDominator(GVNFlag side_effect, HValue* dominator) {
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

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyed)

 private:
  ElementsKind elements_kind_;
  uint32_t index_offset_;
  bool is_dehoisted_ : 1;
  bool is_uninitialized_ : 1;
  HValue* new_space_dominator_;
};


class HStoreKeyedGeneric: public HTemplateInstruction<4> {
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

  virtual Representation RequiredInputRepresentation(int index) {
    // tagged[tagged] = tagged
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedGeneric)

 private:
  StrictModeFlag strict_mode_flag_;
};


class HTransitionElementsKind: public HTemplateInstruction<2> {
 public:
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
    if (original_map->has_fast_double_elements()) {
      SetGVNFlag(kChangesElementsPointer);
      SetGVNFlag(kChangesNewSpacePromotion);
    }
    if (transitioned_map->has_fast_double_elements()) {
      SetGVNFlag(kChangesElementsPointer);
      SetGVNFlag(kChangesNewSpacePromotion);
    }
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* object() { return OperandAt(0); }
  HValue* context() { return OperandAt(1); }
  Handle<Map> original_map() { return original_map_; }
  Handle<Map> transitioned_map() { return transitioned_map_; }
  ElementsKind from_kind() { return from_kind_; }
  ElementsKind to_kind() { return to_kind_; }

  virtual void PrintDataTo(StringStream* stream);

  virtual void FinalizeUniqueValueId() {
    original_map_unique_id_ = UniqueValueId(original_map_);
    transitioned_map_unique_id_ = UniqueValueId(transitioned_map_);
  }

  DECLARE_CONCRETE_INSTRUCTION(TransitionElementsKind)

 protected:
  virtual bool DataEquals(HValue* other) {
    HTransitionElementsKind* instr = HTransitionElementsKind::cast(other);
    return original_map_unique_id_ == instr->original_map_unique_id_ &&
           transitioned_map_unique_id_ == instr->transitioned_map_unique_id_;
  }

 private:
  Handle<Map> original_map_;
  Handle<Map> transitioned_map_;
  UniqueValueId original_map_unique_id_;
  UniqueValueId transitioned_map_unique_id_;
  ElementsKind from_kind_;
  ElementsKind to_kind_;
};


class HStringAdd: public HBinaryOperation {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* left,
                           HValue* right);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() {
    return HType::String();
  }

  DECLARE_CONCRETE_INSTRUCTION(StringAdd)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }


 private:
  HStringAdd(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  // TODO(svenpanne) Might be safe, but leave it out until we know for sure.
  //  virtual bool IsDeletable() const { return true; }
};


class HStringCharCodeAt: public HTemplateInstruction<3> {
 public:
  HStringCharCodeAt(HValue* context, HValue* string, HValue* index) {
    SetOperandAt(0, context);
    SetOperandAt(1, string);
    SetOperandAt(2, index);
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    // The index is supposed to be Integer32.
    return index == 2
        ? Representation::Integer32()
        : Representation::Tagged();
  }

  HValue* context() { return OperandAt(0); }
  HValue* string() { return OperandAt(1); }
  HValue* index() { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(StringCharCodeAt)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange(Zone* zone) {
    return new(zone) Range(0, String::kMaxUtf16CodeUnit);
  }

  // TODO(svenpanne) Might be safe, but leave it out until we know for sure.
  // private:
  //  virtual bool IsDeletable() const { return true; }
};


class HStringCharFromCode: public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Zone* zone,
                           HValue* context,
                           HValue* char_code);

  virtual Representation RequiredInputRepresentation(int index) {
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }
  virtual HType CalculateInferredType();

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  virtual bool DataEquals(HValue* other) { return true; }

  DECLARE_CONCRETE_INSTRUCTION(StringCharFromCode)

 private:
  HStringCharFromCode(HValue* context, HValue* char_code) {
    SetOperandAt(0, context);
    SetOperandAt(1, char_code);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  // TODO(svenpanne) Might be safe, but leave it out until we know for sure.
  // virtual bool IsDeletable() const { return true; }
};


class HStringLength: public HUnaryOperation {
 public:
  static HInstruction* New(Zone* zone, HValue* string);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() {
    STATIC_ASSERT(String::kMaxLength <= Smi::kMaxValue);
    return HType::Smi();
  }

  DECLARE_CONCRETE_INSTRUCTION(StringLength)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange(Zone* zone) {
    return new(zone) Range(0, String::kMaxLength);
  }

 private:
  explicit HStringLength(HValue* string) : HUnaryOperation(string) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
  }

  virtual bool IsDeletable() const { return true; }
};


template <int V>
class HMaterializedLiteral: public HTemplateInstruction<V> {
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
  virtual bool IsDeletable() const { return true; }

  int literal_index_;
  int depth_;
  AllocationSiteMode allocation_site_mode_;
};


class HRegExpLiteral: public HMaterializedLiteral<1> {
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
  }

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> literals() { return literals_; }
  Handle<String> pattern() { return pattern_; }
  Handle<String> flags() { return flags_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(RegExpLiteral)

 private:
  Handle<FixedArray> literals_;
  Handle<String> pattern_;
  Handle<String> flags_;
};


class HFunctionLiteral: public HTemplateInstruction<1> {
 public:
  HFunctionLiteral(HValue* context,
                   Handle<SharedFunctionInfo> shared,
                   bool pretenure)
      : shared_info_(shared),
        pretenure_(pretenure),
        has_no_literals_(shared->num_literals() == 0),
        is_generator_(shared->is_generator()),
        language_mode_(shared->language_mode()) {
    SetOperandAt(0, context);
    set_representation(Representation::Tagged());
    SetGVNFlag(kChangesNewSpacePromotion);
  }

  HValue* context() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(FunctionLiteral)

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  bool pretenure() const { return pretenure_; }
  bool has_no_literals() const { return has_no_literals_; }
  bool is_generator() const { return is_generator_; }
  LanguageMode language_mode() const { return language_mode_; }

 private:
  virtual bool IsDeletable() const { return true; }

  Handle<SharedFunctionInfo> shared_info_;
  bool pretenure_ : 1;
  bool has_no_literals_ : 1;
  bool is_generator_ : 1;
  LanguageMode language_mode_;
};


class HTypeof: public HTemplateInstruction<2> {
 public:
  explicit HTypeof(HValue* context, HValue* value) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    set_representation(Representation::Tagged());
  }

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Typeof)

 private:
  virtual bool IsDeletable() const { return true; }
};


class HTrapAllocationMemento : public HTemplateInstruction<1> {
 public:
  explicit HTrapAllocationMemento(HValue* obj) {
    SetOperandAt(0, obj);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* object() { return OperandAt(0); }

  DECLARE_CONCRETE_INSTRUCTION(TrapAllocationMemento)
};


class HToFastProperties: public HUnaryOperation {
 public:
  explicit HToFastProperties(HValue* value) : HUnaryOperation(value) {
    // This instruction is not marked as having side effects, but
    // changes the map of the input operand. Use it only when creating
    // object literals via a runtime call.
    ASSERT(value->IsCallRuntime());
#ifdef DEBUG
    const Runtime::Function* function = HCallRuntime::cast(value)->function();
    ASSERT(function->function_id == Runtime::kCreateObjectLiteral ||
           function->function_id == Runtime::kCreateObjectLiteralShallow);
#endif
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ToFastProperties)

 private:
  virtual bool IsDeletable() const { return true; }
};


class HValueOf: public HUnaryOperation {
 public:
  explicit HValueOf(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ValueOf)

 private:
  virtual bool IsDeletable() const { return true; }
};


class HDateField: public HUnaryOperation {
 public:
  HDateField(HValue* date, Smi* index)
      : HUnaryOperation(date), index_(index) {
    set_representation(Representation::Tagged());
  }

  Smi* index() const { return index_; }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(DateField)

 private:
  Smi* index_;
};


class HSeqStringSetChar: public HTemplateInstruction<3> {
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

  virtual Representation RequiredInputRepresentation(int index) {
    return (index == 0) ? Representation::Tagged()
                        : Representation::Integer32();
  }

  DECLARE_CONCRETE_INSTRUCTION(SeqStringSetChar)

 private:
  String::Encoding encoding_;
};


class HDeleteProperty: public HBinaryOperation {
 public:
  HDeleteProperty(HValue* context, HValue* obj, HValue* key)
      : HBinaryOperation(context, obj, key) {
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(DeleteProperty)

  HValue* object() { return left(); }
  HValue* key() { return right(); }
};


class HIn: public HTemplateInstruction<3> {
 public:
  HIn(HValue* context, HValue* key, HValue* object) {
    SetOperandAt(0, context);
    SetOperandAt(1, key);
    SetOperandAt(2, object);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  HValue* context() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }
  HValue* object() { return OperandAt(2); }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() {
    return HType::Boolean();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(In)
};


class HCheckMapValue: public HTemplateInstruction<2> {
 public:
  HCheckMapValue(HValue* value,
                 HValue* map) {
    SetOperandAt(0, value);
    SetOperandAt(1, map);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetGVNFlag(kDependsOnMaps);
    SetGVNFlag(kDependsOnElementsKind);
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual HType CalculateInferredType() {
    return HType::Tagged();
  }

  HValue* value() { return OperandAt(0); }
  HValue* map() { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(CheckMapValue)

 protected:
  virtual bool DataEquals(HValue* other) {
    return true;
  }
};


class HForInPrepareMap : public HTemplateInstruction<2> {
 public:
  HForInPrepareMap(HValue* context,
                   HValue* object) {
    SetOperandAt(0, context);
    SetOperandAt(1, object);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* context() { return OperandAt(0); }
  HValue* enumerable() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream);

  virtual HType CalculateInferredType() {
    return HType::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ForInPrepareMap);
};


class HForInCacheArray : public HTemplateInstruction<2> {
 public:
  HForInCacheArray(HValue* enumerable,
                   HValue* keys,
                   int idx) : idx_(idx) {
    SetOperandAt(0, enumerable);
    SetOperandAt(1, keys);
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) {
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

  virtual void PrintDataTo(StringStream* stream);

  virtual HType CalculateInferredType() {
    return HType::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ForInCacheArray);

 private:
  int idx_;
  HForInCacheArray* index_cache_;
};


class HLoadFieldByIndex : public HTemplateInstruction<2> {
 public:
  HLoadFieldByIndex(HValue* object,
                    HValue* index) {
    SetOperandAt(0, object);
    SetOperandAt(1, index);
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) {
    return Representation::Tagged();
  }

  HValue* object() { return OperandAt(0); }
  HValue* index() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream);

  virtual HType CalculateInferredType() {
    return HType::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFieldByIndex);

 private:
  virtual bool IsDeletable() const { return true; }
};


#undef DECLARE_INSTRUCTION
#undef DECLARE_CONCRETE_INSTRUCTION

} }  // namespace v8::internal

#endif  // V8_HYDROGEN_INSTRUCTIONS_H_
