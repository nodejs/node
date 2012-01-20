// Copyright 2011 the V8 project authors. All rights reserved.
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
class HInstruction;
class HLoopInformation;
class HValue;
class LInstruction;
class LChunkBuilder;


#define HYDROGEN_ABSTRACT_INSTRUCTION_LIST(V)  \
  V(BitwiseBinaryOperation)                    \
  V(ControlInstruction)                        \
  V(Instruction)                               \


#define HYDROGEN_CONCRETE_INSTRUCTION_LIST(V)  \
  V(AbnormalExit)                              \
  V(AccessArgumentsAt)                         \
  V(Add)                                       \
  V(ApplyArguments)                            \
  V(ArgumentsElements)                         \
  V(ArgumentsLength)                           \
  V(ArgumentsObject)                           \
  V(ArrayLiteral)                              \
  V(BitAnd)                                    \
  V(BitNot)                                    \
  V(BitOr)                                     \
  V(BitXor)                                    \
  V(BlockEntry)                                \
  V(BoundsCheck)                               \
  V(Branch)                                    \
  V(CallConstantFunction)                      \
  V(CallFunction)                              \
  V(CallGlobal)                                \
  V(CallKeyed)                                 \
  V(CallKnownGlobal)                           \
  V(CallNamed)                                 \
  V(CallNew)                                   \
  V(CallRuntime)                               \
  V(CallStub)                                  \
  V(Change)                                    \
  V(CheckFunction)                             \
  V(CheckInstanceType)                         \
  V(CheckMap)                                  \
  V(CheckNonSmi)                               \
  V(CheckPrototypeMaps)                        \
  V(CheckSmi)                                  \
  V(ClampToUint8)                              \
  V(ClassOfTestAndBranch)                      \
  V(CompareIDAndBranch)                        \
  V(CompareGeneric)                            \
  V(CompareObjectEqAndBranch)                  \
  V(CompareMap)                                \
  V(CompareConstantEqAndBranch)                \
  V(Constant)                                  \
  V(Context)                                   \
  V(DeleteProperty)                            \
  V(Deoptimize)                                \
  V(Div)                                       \
  V(ElementsKind)                              \
  V(EnterInlined)                              \
  V(FixedArrayBaseLength)                      \
  V(ForceRepresentation)                       \
  V(FunctionLiteral)                           \
  V(GetCachedArrayIndex)                       \
  V(GlobalObject)                              \
  V(GlobalReceiver)                            \
  V(Goto)                                      \
  V(HasCachedArrayIndexAndBranch)              \
  V(HasInstanceTypeAndBranch)                  \
  V(In)                                        \
  V(InstanceOf)                                \
  V(InstanceOfKnownGlobal)                     \
  V(InvokeFunction)                            \
  V(IsConstructCallAndBranch)                  \
  V(IsNullAndBranch)                           \
  V(IsObjectAndBranch)                         \
  V(IsSmiAndBranch)                            \
  V(IsUndetectableAndBranch)                   \
  V(JSArrayLength)                             \
  V(LeaveInlined)                              \
  V(LoadContextSlot)                           \
  V(LoadElements)                              \
  V(LoadExternalArrayPointer)                  \
  V(LoadFunctionPrototype)                     \
  V(LoadGlobalCell)                            \
  V(LoadGlobalGeneric)                         \
  V(LoadKeyedFastDoubleElement)                \
  V(LoadKeyedFastElement)                      \
  V(LoadKeyedGeneric)                          \
  V(LoadKeyedSpecializedArrayElement)          \
  V(LoadNamedField)                            \
  V(LoadNamedFieldPolymorphic)                 \
  V(LoadNamedGeneric)                          \
  V(Mod)                                       \
  V(Mul)                                       \
  V(ObjectLiteral)                             \
  V(OsrEntry)                                  \
  V(OuterContext)                              \
  V(Parameter)                                 \
  V(Power)                                     \
  V(PushArgument)                              \
  V(RegExpLiteral)                             \
  V(Return)                                    \
  V(Sar)                                       \
  V(Shl)                                       \
  V(Shr)                                       \
  V(Simulate)                                  \
  V(SoftDeoptimize)                            \
  V(StackCheck)                                \
  V(StoreContextSlot)                          \
  V(StoreGlobalCell)                           \
  V(StoreGlobalGeneric)                        \
  V(StoreKeyedFastDoubleElement)               \
  V(StoreKeyedFastElement)                     \
  V(StoreKeyedGeneric)                         \
  V(StoreKeyedSpecializedArrayElement)         \
  V(StoreNamedField)                           \
  V(StoreNamedGeneric)                         \
  V(StringAdd)                                 \
  V(StringCharCodeAt)                          \
  V(StringCharFromCode)                        \
  V(StringLength)                              \
  V(Sub)                                       \
  V(ThisFunction)                              \
  V(Throw)                                     \
  V(ToFastProperties)                          \
  V(ToInt32)                                   \
  V(Typeof)                                    \
  V(TypeofIsAndBranch)                         \
  V(UnaryMathOperation)                        \
  V(UnknownOSRValue)                           \
  V(UseConst)                                  \
  V(ValueOf)

#define GVN_FLAG_LIST(V)                       \
  V(Calls)                                     \
  V(InobjectFields)                            \
  V(BackingStoreFields)                        \
  V(ArrayElements)                             \
  V(DoubleArrayElements)                       \
  V(SpecializedArrayElements)                  \
  V(GlobalVars)                                \
  V(Maps)                                      \
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
  Range* CopyClearLower() const { return new Range(kMinInt, upper_); }
  Range* CopyClearUpper() const { return new Range(lower_, kMaxInt); }
  Range* Copy() const {
    Range* result = new Range(lower_, upper_);
    result->set_can_be_minus_zero(CanBeMinusZero());
    return result;
  }
  int32_t Mask() const;
  void set_can_be_minus_zero(bool b) { can_be_minus_zero_ = b; }
  bool CanBeMinusZero() const { return CanBeZero() && can_be_minus_zero_; }
  bool CanBeZero() const { return upper_ >= 0 && lower_ <= 0; }
  bool CanBeNegative() const { return lower_ < 0; }
  bool Includes(int value) const { return lower_ <= value && upper_ >= value; }
  bool IsMostGeneric() const {
    return lower_ == kMinInt && upper_ == kMaxInt && CanBeMinusZero();
  }
  bool IsInSmiRange() const {
    return lower_ >= Smi::kMinValue && upper_ <= Smi::kMaxValue;
  }
  void KeepOrder();
  void Verify() const;

  void StackUpon(Range* other) {
    Intersect(other);
    next_ = other;
  }

  void Intersect(Range* other);
  void Union(Range* other);

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


class Representation {
 public:
  enum Kind {
    kNone,
    kTagged,
    kDouble,
    kInteger32,
    kExternal,
    kNumRepresentations
  };

  Representation() : kind_(kNone) { }

  static Representation None() { return Representation(kNone); }
  static Representation Tagged() { return Representation(kTagged); }
  static Representation Integer32() { return Representation(kInteger32); }
  static Representation Double() { return Representation(kDouble); }
  static Representation External() { return Representation(kExternal); }

  bool Equals(const Representation& other) {
    return kind_ == other.kind_;
  }

  Kind kind() const { return static_cast<Kind>(kind_); }
  bool IsNone() const { return kind_ == kNone; }
  bool IsTagged() const { return kind_ == kTagged; }
  bool IsInteger32() const { return kind_ == kInteger32; }
  bool IsDouble() const { return kind_ == kDouble; }
  bool IsExternal() const { return kind_ == kExternal; }
  bool IsSpecialization() const {
    return kind_ == kInteger32 || kind_ == kDouble;
  }
  const char* Mnemonic() const;

 private:
  explicit Representation(Kind k) : kind_(k) { }

  // Make sure kind fits in int8.
  STATIC_ASSERT(kNumRepresentations <= (1 << kBitsPerByte));

  int8_t kind_;
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

  bool Equals(const HType& other) {
    return type_ == other.type_;
  }

  bool IsSubtypeOf(const HType& other) {
    return Combine(other).Equals(other);
  }

  bool IsTagged() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kTagged) == kTagged);
  }

  bool IsTaggedPrimitive() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kTaggedPrimitive) == kTaggedPrimitive);
  }

  bool IsTaggedNumber() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kTaggedNumber) == kTaggedNumber);
  }

  bool IsSmi() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kSmi) == kSmi);
  }

  bool IsHeapNumber() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kHeapNumber) == kHeapNumber);
  }

  bool IsString() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kString) == kString);
  }

  bool IsBoolean() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kBoolean) == kBoolean);
  }

  bool IsNonPrimitive() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kNonPrimitive) == kNonPrimitive);
  }

  bool IsJSArray() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kJSArray) == kJSArray);
  }

  bool IsJSObject() {
    ASSERT(type_ != kUninitialized);
    return ((type_ & kJSObject) == kJSObject);
  }

  bool IsUninitialized() {
    return type_ == kUninitialized;
  }

  static HType TypeFromValue(Handle<Object> value);

  const char* ToString();
  const char* ToShortString();

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

  HUseListNode* tail() const { return tail_; }
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


class HValue: public ZoneObject {
 public:
  static const int kNoNumber = -1;

  // There must be one corresponding kDepends flag for every kChanges flag and
  // the order of the kChanges flags must be exactly the same as of the kDepends
  // flags.
  enum Flag {
    // Declare global value numbering flags.
  #define DECLARE_DO(type) kChanges##type, kDependsOn##type,
    GVN_FLAG_LIST(DECLARE_DO)
  #undef DECLARE_DO
    kFlexibleRepresentation,
    // Participate in Global Value Numbering, i.e. elimination of
    // unnecessary recomputations. If an instruction sets this flag, it must
    // implement DataEquals(), which will be used to determine if other
    // occurrences of the instruction are indeed the same.
    kUseGVN,
    kCanOverflow,
    kBailoutOnMinusZero,
    kCanBeDivByZero,
    kDeoptimizeOnUndefined,
    kIsArguments,
    kTruncatingToInt32,
    kLastFlag = kTruncatingToInt32
  };

  STATIC_ASSERT(kLastFlag < kBitsPerInt);

  static const int kChangesToDependsFlagsLeftShift = 1;

  static int ConvertChangesToDependsFlags(int flags) {
    return flags << kChangesToDependsFlagsLeftShift;
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

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  HUseIterator uses() const { return HUseIterator(use_list_); }

  virtual bool EmitAtUses() { return false; }
  Representation representation() const { return representation_; }
  void ChangeRepresentation(Representation r) {
    // Representation was already set and is allowed to be changed.
    ASSERT(!r.IsNone());
    ASSERT(CheckFlag(kFlexibleRepresentation));
    RepresentationChanged(r);
    representation_ = r;
  }
  void AssumeRepresentation(Representation r);

  virtual bool IsConvertibleToInteger() const { return true; }

  HType type() const { return type_; }
  void set_type(HType new_type) {
    ASSERT(new_type.IsSubtypeOf(type_));
    type_ = new_type;
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

  bool IsDefinedAfter(HBasicBlock* other) const;

  // Operands.
  virtual int OperandCount() = 0;
  virtual HValue* OperandAt(int index) = 0;
  void SetOperandAt(int index, HValue* value);

  void DeleteAndReplaceWith(HValue* other);
  void ReplaceAllUsesWith(HValue* other);
  bool HasNoUses() const { return use_list_ == NULL; }
  bool HasMultipleUses() const {
    return use_list_ != NULL && use_list_->tail() != NULL;
  }
  int UseCount() const;
  void ClearOperands();

  int flags() const { return flags_; }
  void SetFlag(Flag f) { flags_ |= (1 << f); }
  void ClearFlag(Flag f) { flags_ &= ~(1 << f); }
  bool CheckFlag(Flag f) const { return (flags_ & (1 << f)) != 0; }

  void SetAllSideEffects() { flags_ |= AllSideEffects(); }
  void ClearAllSideEffects() { flags_ &= ~AllSideEffects(); }
  bool HasSideEffects() const { return (flags_ & AllSideEffects()) != 0; }

  int ChangesFlags() const { return flags_ & ChangesFlagsMask(); }

  Range* range() const { return range_; }
  bool HasRange() const { return range_ != NULL; }
  void AddNewRange(Range* r);
  void RemoveLastAddedRange();
  void ComputeInitialRange();

  // Representation helpers.
  virtual Representation RequiredInputRepresentation(int index) const = 0;

  virtual Representation InferredRepresentation() {
    return representation();
  }

  // This gives the instruction an opportunity to replace itself with an
  // instruction that does the same in some better way.  To replace an
  // instruction with a new one, first add the new instruction to the graph,
  // then return it.  Return NULL to have the instruction deleted.
  virtual HValue* Canonicalize() { return this; }

  bool Equals(HValue* other);
  virtual intptr_t Hashcode();

  // Printing support.
  virtual void PrintTo(StringStream* stream) = 0;
  void PrintNameTo(StringStream* stream);
  void PrintTypeTo(StringStream* stream);
  void PrintRangeTo(StringStream* stream);
  void PrintChangesTo(StringStream* stream);

  const char* Mnemonic() const;

  // Updated the inferred type of this instruction and returns true if
  // it has changed.
  bool UpdateInferredType();

  virtual HType CalculateInferredType();

#ifdef DEBUG
  virtual void Verify() = 0;
#endif

 protected:
  // This function must be overridden for instructions with flag kUseGVN, to
  // compare the non-Operand parts of the instruction.
  virtual bool DataEquals(HValue* other) {
    UNREACHABLE();
    return false;
  }
  virtual void RepresentationChanged(Representation to) { }
  virtual Range* InferRange();
  virtual void DeleteFromGraph() = 0;
  virtual void InternalSetOperandAt(int index, HValue* value) = 0;
  void clear_block() {
    ASSERT(block_ != NULL);
    block_ = NULL;
  }

  void set_representation(Representation r) {
    // Representation is set-once.
    ASSERT(representation_.IsNone() && !r.IsNone());
    representation_ = r;
  }

 private:
  static int ChangesFlagsMask() {
    int result = 0;
    // Create changes mask.
#define ADD_FLAG(type) result |= (1 << kChanges##type);
  GVN_FLAG_LIST(ADD_FLAG)
#undef ADD_FLAG
    return result;
  }

  // A flag mask to mark an instruction as having arbitrary side effects.
  static int AllSideEffects() {
    return ChangesFlagsMask() & ~(1 << kChangesOsrEntries);
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

  DISALLOW_COPY_AND_ASSIGN(HValue);
};


class HInstruction: public HValue {
 public:
  HInstruction* next() const { return next_; }
  HInstruction* previous() const { return previous_; }

  virtual void PrintTo(StringStream* stream);
  virtual void PrintDataTo(StringStream* stream) { }

  bool IsLinked() const { return block() != NULL; }
  void Unlink();
  void InsertBefore(HInstruction* next);
  void InsertAfter(HInstruction* previous);

  int position() const { return position_; }
  bool has_position() const { return position_ != RelocInfo::kNoPosition; }
  void set_position(int position) { position_ = position; }

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
    SetFlag(kDependsOnOsrEntries);
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
  HValue* OperandAt(int i) { return inputs_[i]; }

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
  HValue* OperandAt(int i) { return inputs_[i]; }


 protected:
  void InternalSetOperandAt(int i, HValue* value) { inputs_[i] = value; }

 private:
  EmbeddedContainer<HBasicBlock*, S> successors_;
  EmbeddedContainer<HValue*, V> inputs_;
};


class HBlockEntry: public HTemplateInstruction<0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(BlockEntry)
};


// We insert soft-deoptimize when we hit code with unknown typefeedback,
// so that we get a chance of re-optimizing with useful typefeedback.
// HSoftDeoptimize does not end a basic block as opposed to HDeoptimize.
class HSoftDeoptimize: public HTemplateInstruction<0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(SoftDeoptimize)
};


class HDeoptimize: public HControlInstruction {
 public:
  explicit HDeoptimize(int environment_length) : values_(environment_length) { }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  virtual int OperandCount() { return values_.length(); }
  virtual HValue* OperandAt(int index) { return values_[index]; }
  virtual void PrintDataTo(StringStream* stream);

  virtual int SuccessorCount() { return 0; }
  virtual HBasicBlock* SuccessorAt(int i) {
    UNREACHABLE();
    return NULL;
  }
  virtual void SetSuccessorAt(int i, HBasicBlock* block) {
    UNREACHABLE();
  }

  void AddEnvironmentValue(HValue* value) {
    values_.Add(NULL);
    SetOperandAt(values_.length() - 1, value);
  }

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
};


class HGoto: public HTemplateControlInstruction<1, 0> {
 public:
  explicit HGoto(HBasicBlock* target) {
        SetSuccessorAt(0, target);
      }

  virtual Representation RequiredInputRepresentation(int index) const {
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
          ToBooleanStub::Types expected_input_types = ToBooleanStub::no_types())
      : HUnaryControlInstruction(value, true_target, false_target),
        expected_input_types_(expected_input_types) {
    ASSERT(true_target != NULL && false_target != NULL);
  }
  explicit HBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }


  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareMap)

 private:
  Handle<Map> map_;
};


class HReturn: public HTemplateControlInstruction<0, 1> {
 public:
  explicit HReturn(HValue* value) {
    SetOperandAt(0, value);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  HValue* value() { return OperandAt(0); }

  DECLARE_CONCRETE_INSTRUCTION(Return)
};


class HAbnormalExit: public HTemplateControlInstruction<0, 0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual bool CanTruncateToInt32() const {
    return CheckFlag(kTruncatingToInt32);
  }

  HValue* value() { return OperandAt(0); }
  virtual void PrintDataTo(StringStream* stream);
};


class HThrow: public HTemplateInstruction<2> {
 public:
  HThrow(HValue* context, HValue* value) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(Throw)
};


class HUseConst: public HUnaryOperation {
 public:
  explicit HUseConst(HValue* old_value) : HUnaryOperation(old_value) { }

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return representation();  // Same as the output representation.
  }

  DECLARE_CONCRETE_INSTRUCTION(ForceRepresentation)
};


class HChange: public HUnaryOperation {
 public:
  HChange(HValue* value,
          Representation from,
          Representation to,
          bool is_truncating,
          bool deoptimize_on_undefined)
      : HUnaryOperation(value),
        from_(from),
        deoptimize_on_undefined_(deoptimize_on_undefined) {
    ASSERT(!from.IsNone() && !to.IsNone());
    ASSERT(!from.Equals(to));
    set_representation(to);
    SetFlag(kUseGVN);
    if (is_truncating) SetFlag(kTruncatingToInt32);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  Representation from() const { return from_; }
  Representation to() const { return representation(); }
  bool deoptimize_on_undefined() const { return deoptimize_on_undefined_; }
  virtual Representation RequiredInputRepresentation(int index) const {
    return from_;
  }

  virtual Range* InferRange();

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(Change)

 protected:
  virtual bool DataEquals(HValue* other) {
    if (!other->IsChange()) return false;
    HChange* change = HChange::cast(other);
    return to().Equals(change->to())
        && deoptimize_on_undefined() == change->deoptimize_on_undefined();
  }

 private:
  Representation from_;
  bool deoptimize_on_undefined_;
};


class HClampToUint8: public HUnaryOperation {
 public:
  explicit HClampToUint8(HValue* value)
      : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ClampToUint8)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HToInt32: public HUnaryOperation {
 public:
  explicit HToInt32(HValue* value)
      : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  virtual bool CanTruncateToInt32() const {
    return true;
  }

  virtual HValue* Canonicalize() {
    if (value()->representation().IsInteger32()) {
      return value();
    } else {
      return this;
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(ToInt32)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HSimulate: public HInstruction {
 public:
  HSimulate(int ast_id, int pop_count)
      : ast_id_(ast_id),
        pop_count_(pop_count),
        values_(2),
        assigned_indexes_(2) {}
  virtual ~HSimulate() {}

  virtual void PrintDataTo(StringStream* stream);

  bool HasAstId() const { return ast_id_ != AstNode::kNoNumber; }
  int ast_id() const { return ast_id_; }
  void set_ast_id(int id) {
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
  virtual int OperandCount() { return values_.length(); }
  virtual HValue* OperandAt(int index) { return values_[index]; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Simulate)

#ifdef DEBUG
  virtual void Verify();
#endif

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    values_[index] = value;
  }

 private:
  static const int kNoIndex = -1;
  void AddValue(int index, HValue* value) {
    assigned_indexes_.Add(index);
    // Resize the list of pushed values.
    values_.Add(NULL);
    // Set the operand through the base method in HValue to make sure that the
    // use lists are correctly updated.
    SetOperandAt(values_.length() - 1, value);
  }
  int ast_id_;
  int pop_count_;
  ZoneList<HValue*> values_;
  ZoneList<int> assigned_indexes_;
};


class HStackCheck: public HTemplateInstruction<1> {
 public:
  enum Type {
    kFunctionEntry,
    kBackwardsBranch
  };

  HStackCheck(HValue* context, Type type) : type_(type) {
    SetOperandAt(0, context);
  }

  HValue* context() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  void Eliminate() {
    // The stack check eliminator might try to eliminate the same stack
    // check instruction multiple times.
    if (IsLinked()) {
      DeleteFromGraph();
    }
  }

  bool is_function_entry() { return type_ == kFunctionEntry; }
  bool is_backwards_branch() { return type_ == kBackwardsBranch; }

  DECLARE_CONCRETE_INSTRUCTION(StackCheck)

 private:
  Type type_;
};


class HEnterInlined: public HTemplateInstruction<0> {
 public:
  HEnterInlined(Handle<JSFunction> closure,
                FunctionLiteral* function,
                CallKind call_kind)
      : closure_(closure),
        function_(function),
        call_kind_(call_kind) {
  }

  virtual void PrintDataTo(StringStream* stream);

  Handle<JSFunction> closure() const { return closure_; }
  FunctionLiteral* function() const { return function_; }
  CallKind call_kind() const { return call_kind_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(EnterInlined)

 private:
  Handle<JSFunction> closure_;
  FunctionLiteral* function_;
  CallKind call_kind_;
};


class HLeaveInlined: public HTemplateInstruction<0> {
 public:
  HLeaveInlined() {}

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(LeaveInlined)
};


class HPushArgument: public HUnaryOperation {
 public:
  explicit HPushArgument(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ThisFunction)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HContext: public HTemplateInstruction<0> {
 public:
  HContext() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Context)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HOuterContext: public HUnaryOperation {
 public:
  explicit HOuterContext(HValue* inner) : HUnaryOperation(inner) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(OuterContext);

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HGlobalObject: public HUnaryOperation {
 public:
  explicit HGlobalObject(HValue* context) : HUnaryOperation(context) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(GlobalObject)

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HGlobalReceiver: public HUnaryOperation {
 public:
  explicit HGlobalReceiver(HValue* global_object)
      : HUnaryOperation(global_object) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(GlobalReceiver)

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
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

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  HValue* context() { return first(); }
  HValue* function() { return second(); }

  DECLARE_CONCRETE_INSTRUCTION(InvokeFunction)
};


class HCallConstantFunction: public HCall<0> {
 public:
  HCallConstantFunction(Handle<JSFunction> function, int argument_count)
      : HCall<0>(argument_count), function_(function) { }

  Handle<JSFunction> function() const { return function_; }

  bool IsApplyFunction() const {
    return function_->code() ==
        Isolate::Current()->builtins()->builtin(Builtins::kFunctionApply);
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallConstantFunction)

 private:
  Handle<JSFunction> function_;
};


class HCallKeyed: public HBinaryCall {
 public:
  HCallKeyed(HValue* context, HValue* key, int argument_count)
      : HBinaryCall(context, key, argument_count) {
  }

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

 private:
  Handle<String> name_;
};


class HCallFunction: public HUnaryCall {
 public:
  HCallFunction(HValue* context, int argument_count)
      : HUnaryCall(context, argument_count) {
  }

  HValue* context() { return value(); }

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallGlobal)

 private:
  Handle<String> name_;
};


class HCallKnownGlobal: public HCall<0> {
 public:
  HCallKnownGlobal(Handle<JSFunction> target, int argument_count)
      : HCall<0>(argument_count), target_(target) { }

  virtual void PrintDataTo(StringStream* stream);

  Handle<JSFunction> target() const { return target_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallKnownGlobal)

 private:
  Handle<JSFunction> target_;
};


class HCallNew: public HBinaryCall {
 public:
  HCallNew(HValue* context, HValue* constructor, int argument_count)
      : HBinaryCall(context, constructor, argument_count) {
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  HValue* context() { return first(); }
  HValue* constructor() { return second(); }

  DECLARE_CONCRETE_INSTRUCTION(CallNew)
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallRuntime)

 private:
  const Runtime::Function* c_function_;
  Handle<String> name_;
};


class HJSArrayLength: public HTemplateInstruction<2> {
 public:
  HJSArrayLength(HValue* value, HValue* typecheck) {
    // The length of an array is stored as a tagged value in the array
    // object. It is guaranteed to be 32 bit integer, but it can be
    // represented as either a smi or heap number.
    SetOperandAt(0, value);
    SetOperandAt(1, typecheck);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnArrayLengths);
    SetFlag(kDependsOnMaps);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  HValue* value() { return OperandAt(0); }
  HValue* typecheck() { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(JSArrayLength)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HFixedArrayBaseLength: public HUnaryOperation {
 public:
  explicit HFixedArrayBaseLength(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnArrayLengths);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(FixedArrayBaseLength)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HElementsKind: public HUnaryOperation {
 public:
  explicit HElementsKind(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ElementsKind)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HBitNot: public HUnaryOperation {
 public:
  explicit HBitNot(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetFlag(kTruncatingToInt32);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Integer32();
  }
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(BitNot)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HUnaryMathOperation: public HTemplateInstruction<2> {
 public:
  HUnaryMathOperation(HValue* context, HValue* value, BuiltinFunctionId op)
      : op_(op) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    switch (op) {
      case kMathFloor:
      case kMathRound:
      case kMathCeil:
        set_representation(Representation::Integer32());
        break;
      case kMathAbs:
        set_representation(Representation::Tagged());
        SetFlag(kFlexibleRepresentation);
        break;
      case kMathSqrt:
      case kMathPowHalf:
      case kMathLog:
      case kMathSin:
      case kMathCos:
        set_representation(Representation::Double());
        break;
      default:
        UNREACHABLE();
    }
    SetFlag(kUseGVN);
  }

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  virtual void PrintDataTo(StringStream* stream);

  virtual HType CalculateInferredType();

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual Representation RequiredInputRepresentation(int index) const {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      switch (op_) {
        case kMathFloor:
        case kMathRound:
        case kMathCeil:
        case kMathSqrt:
        case kMathPowHalf:
        case kMathLog:
        case kMathSin:
        case kMathCos:
          return Representation::Double();
        case kMathAbs:
          return representation();
        default:
          UNREACHABLE();
          return Representation::None();
      }
    }
  }

  virtual HValue* Canonicalize() {
    // If the input is integer32 then we replace the floor instruction
    // with its inputs.  This happens before the representation changes are
    // introduced.
    if (op() == kMathFloor) {
      if (value()->representation().IsInteger32()) return value();
    }
    return this;
  }

  BuiltinFunctionId op() const { return op_; }
  const char* OpName() const;

  DECLARE_CONCRETE_INSTRUCTION(UnaryMathOperation)

 protected:
  virtual bool DataEquals(HValue* other) {
    HUnaryMathOperation* b = HUnaryMathOperation::cast(other);
    return op_ == b->op();
  }

 private:
  BuiltinFunctionId op_;
};


class HLoadElements: public HUnaryOperation {
 public:
  explicit HLoadElements(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadElements)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadExternalArrayPointer)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HCheckMap: public HTemplateInstruction<2> {
 public:
  HCheckMap(HValue* value, Handle<Map> map, HValue* typecheck = NULL)
      : map_(map) {
    SetOperandAt(0, value);
    // If callers don't depend on a typecheck, they can pass in NULL. In that
    // case we use a copy of the |value| argument as a dummy value.
    SetOperandAt(1, typecheck != NULL ? typecheck : value);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream);
  virtual HType CalculateInferredType();

  HValue* value() { return OperandAt(0); }
  Handle<Map> map() const { return map_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckMap)

 protected:
  virtual bool DataEquals(HValue* other) {
    HCheckMap* b = HCheckMap::cast(other);
    return map_.is_identical_to(b->map());
  }

 private:
  Handle<Map> map_;
};


class HCheckFunction: public HUnaryOperation {
 public:
  HCheckFunction(HValue* value, Handle<JSFunction> function)
      : HUnaryOperation(value), target_(function) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream);
  virtual HType CalculateInferredType();

#ifdef DEBUG
  virtual void Verify();
#endif

  Handle<JSFunction> target() const { return target_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckFunction)

 protected:
  virtual bool DataEquals(HValue* other) {
    HCheckFunction* b = HCheckFunction::cast(other);
    return target_.is_identical_to(b->target());
  }

 private:
  Handle<JSFunction> target_;
};


class HCheckInstanceType: public HUnaryOperation {
 public:
  static HCheckInstanceType* NewIsSpecObject(HValue* value) {
    return new HCheckInstanceType(value, IS_SPEC_OBJECT);
  }
  static HCheckInstanceType* NewIsJSArray(HValue* value) {
    return new HCheckInstanceType(value, IS_JS_ARRAY);
  }
  static HCheckInstanceType* NewIsString(HValue* value) {
    return new HCheckInstanceType(value, IS_STRING);
  }
  static HCheckInstanceType* NewIsSymbol(HValue* value) {
    return new HCheckInstanceType(value, IS_SYMBOL);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
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
    IS_SYMBOL,
    LAST_INTERVAL_CHECK = IS_JS_ARRAY
  };

  HCheckInstanceType(HValue* value, Check check)
      : HUnaryOperation(value), check_(check) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  const Check check_;
};


class HCheckNonSmi: public HUnaryOperation {
 public:
  explicit HCheckNonSmi(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType();

#ifdef DEBUG
  virtual void Verify();
#endif

  virtual HValue* Canonicalize() {
    HType value_type = value()->type();
    if (!value_type.IsUninitialized() &&
        (value_type.IsHeapNumber() ||
         value_type.IsString() ||
         value_type.IsBoolean() ||
         value_type.IsNonPrimitive())) {
      return NULL;
    }
    return this;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckNonSmi)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HCheckPrototypeMaps: public HTemplateInstruction<0> {
 public:
  HCheckPrototypeMaps(Handle<JSObject> prototype, Handle<JSObject> holder)
      : prototype_(prototype), holder_(holder) {
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
  }

#ifdef DEBUG
  virtual void Verify();
#endif

  Handle<JSObject> prototype() const { return prototype_; }
  Handle<JSObject> holder() const { return holder_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckPrototypeMaps)

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  virtual intptr_t Hashcode() {
    ASSERT(!HEAP->IsAllocationAllowed());
    intptr_t hash = reinterpret_cast<intptr_t>(*prototype());
    hash = 17 * hash + reinterpret_cast<intptr_t>(*holder());
    return hash;
  }

 protected:
  virtual bool DataEquals(HValue* other) {
    HCheckPrototypeMaps* b = HCheckPrototypeMaps::cast(other);
    return prototype_.is_identical_to(b->prototype()) &&
        holder_.is_identical_to(b->holder());
  }

 private:
  Handle<JSObject> prototype_;
  Handle<JSObject> holder_;
};


class HCheckSmi: public HUnaryOperation {
 public:
  explicit HCheckSmi(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual HType CalculateInferredType();

#ifdef DEBUG
  virtual void Verify();
#endif

  DECLARE_CONCRETE_INSTRUCTION(CheckSmi)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HPhi: public HValue {
 public:
  explicit HPhi(int merged_index)
      : inputs_(2),
        merged_index_(merged_index),
        phi_id_(-1),
        is_live_(false),
        is_convertible_to_integer_(true) {
    for (int i = 0; i < Representation::kNumRepresentations; i++) {
      non_phi_uses_[i] = 0;
      indirect_uses_[i] = 0;
    }
    ASSERT(merged_index >= 0);
    set_representation(Representation::Tagged());
    SetFlag(kFlexibleRepresentation);
  }

  virtual Representation InferredRepresentation() {
    bool double_occurred = false;
    bool int32_occurred = false;
    for (int i = 0; i < OperandCount(); ++i) {
      HValue* value = OperandAt(i);
      if (value->representation().IsDouble()) double_occurred = true;
      if (value->representation().IsInteger32()) int32_occurred = true;
      if (value->representation().IsTagged()) return Representation::Tagged();
    }

    if (double_occurred) return Representation::Double();
    if (int32_occurred) return Representation::Integer32();
    return Representation::None();
  }

  virtual Range* InferRange();
  virtual Representation RequiredInputRepresentation(int index) const {
    return representation();
  }
  virtual HType CalculateInferredType();
  virtual int OperandCount() { return inputs_.length(); }
  virtual HValue* OperandAt(int index) { return inputs_[index]; }
  HValue* GetRedundantReplacement();
  void AddInput(HValue* value);
  bool HasRealUses();

  bool IsReceiver() { return merged_index_ == 0; }

  int merged_index() const { return merged_index_; }

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
  int int32_non_phi_uses() const {
    return non_phi_uses_[Representation::kInteger32];
  }
  int double_non_phi_uses() const {
    return non_phi_uses_[Representation::kDouble];
  }
  int tagged_indirect_uses() const {
    return indirect_uses_[Representation::kTagged];
  }
  int int32_indirect_uses() const {
    return indirect_uses_[Representation::kInteger32];
  }
  int double_indirect_uses() const {
    return indirect_uses_[Representation::kDouble];
  }
  int phi_id() { return phi_id_; }
  bool is_live() { return is_live_; }
  void set_is_live(bool b) { is_live_ = b; }

  static HPhi* cast(HValue* value) {
    ASSERT(value->IsPhi());
    return reinterpret_cast<HPhi*>(value);
  }
  virtual Opcode opcode() const { return HValue::kPhi; }

  virtual bool IsConvertibleToInteger() const {
    return is_convertible_to_integer_;
  }

  void set_is_convertible_to_integer(bool b) {
    is_convertible_to_integer_ = b;
  }

  bool AllOperandsConvertibleToInteger() {
    for (int i = 0; i < OperandCount(); ++i) {
      if (!OperandAt(i)->IsConvertibleToInteger()) return false;
    }
    return true;
  }

 protected:
  virtual void DeleteFromGraph();
  virtual void InternalSetOperandAt(int index, HValue* value) {
    inputs_[index] = value;
  }

 private:
  ZoneList<HValue*> inputs_;
  int merged_index_;

  int non_phi_uses_[Representation::kNumRepresentations];
  int indirect_uses_[Representation::kNumRepresentations];
  int phi_id_;
  bool is_live_;
  bool is_convertible_to_integer_;
};


class HArgumentsObject: public HTemplateInstruction<0> {
 public:
  HArgumentsObject() {
    set_representation(Representation::Tagged());
    SetFlag(kIsArguments);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsObject)
};


class HConstant: public HTemplateInstruction<0> {
 public:
  HConstant(Handle<Object> handle, Representation r);

  Handle<Object> handle() const { return handle_; }

  bool InOldSpace() const { return !HEAP->InNewSpace(*handle_); }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  virtual bool IsConvertibleToInteger() const {
    if (handle_->IsSmi()) return true;
    if (handle_->IsHeapNumber() &&
        (HeapNumber::cast(*handle_)->value() ==
         static_cast<double>(NumberToInt32(*handle_)))) return true;
    return false;
  }

  virtual bool EmitAtUses() { return !representation().IsDouble(); }
  virtual void PrintDataTo(StringStream* stream);
  virtual HType CalculateInferredType();
  bool IsInteger() const { return handle_->IsSmi(); }
  HConstant* CopyToRepresentation(Representation r) const;
  HConstant* CopyToTruncatedInt32() const;
  bool HasInteger32Value() const { return has_int32_value_; }
  int32_t Integer32Value() const {
    ASSERT(HasInteger32Value());
    return int32_value_;
  }
  bool HasDoubleValue() const { return has_double_value_; }
  double DoubleValue() const {
    ASSERT(HasDoubleValue());
    return double_value_;
  }
  bool HasStringValue() const { return handle_->IsString(); }

  bool ToBoolean() const;

  virtual intptr_t Hashcode() {
    ASSERT(!HEAP->allow_allocation(false));
    return reinterpret_cast<intptr_t>(*handle());
  }

#ifdef DEBUG
  virtual void Verify() { }
#endif

  DECLARE_CONCRETE_INSTRUCTION(Constant)

 protected:
  virtual Range* InferRange();

  virtual bool DataEquals(HValue* other) {
    HConstant* other_constant = HConstant::cast(other);
    return handle().is_identical_to(other_constant->handle());
  }

 private:
  Handle<Object> handle_;

  // The following two values represent the int32 and the double value of the
  // given constant if there is a lossless conversion between the constant
  // and the specific representation.
  bool has_int32_value_ : 1;
  bool has_double_value_ : 1;
  int32_t int32_value_;
  double double_value_;
};


class HBinaryOperation: public HTemplateInstruction<3> {
 public:
  HBinaryOperation(HValue* context, HValue* left, HValue* right) {
    ASSERT(left != NULL && right != NULL);
    SetOperandAt(0, context);
    SetOperandAt(1, left);
    SetOperandAt(2, right);
  }

  HValue* context() { return OperandAt(0); }
  HValue* left() { return OperandAt(1); }
  HValue* right() { return OperandAt(2); }

  // TODO(kasperl): Move these helpers to the IA-32 Lithium
  // instruction sequence builder.
  HValue* LeastConstantOperand() {
    if (IsCommutative() && left()->IsConstant()) return right();
    return left();
  }
  HValue* MostConstantOperand() {
    if (IsCommutative() && left()->IsConstant()) return left();
    return right();
  }

  virtual bool IsCommutative() const { return false; }

  virtual void PrintDataTo(StringStream* stream);
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

  virtual Representation RequiredInputRepresentation(int index) const {
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
  HArgumentsElements() {
    // The value produced by this instruction is a pointer into the stack
    // that looks as if it was a smi because of alignment.
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements)

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HArgumentsLength: public HUnaryOperation {
 public:
  explicit HArgumentsLength(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
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

  virtual Representation RequiredInputRepresentation(int index) const {
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


class HBoundsCheck: public HTemplateInstruction<2> {
 public:
  HBoundsCheck(HValue* index, HValue* length) {
    SetOperandAt(0, index);
    SetOperandAt(1, length);
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Integer32();
  }

  virtual void PrintDataTo(StringStream* stream);

  HValue* index() { return OperandAt(0); }
  HValue* length() { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheck)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HBitwiseBinaryOperation: public HBinaryOperation {
 public:
  HBitwiseBinaryOperation(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    set_representation(Representation::Tagged());
    SetFlag(kFlexibleRepresentation);
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return index == 0
        ? Representation::Tagged()
        : representation();
  }

  virtual void RepresentationChanged(Representation to) {
    if (!to.IsTagged()) {
      ASSERT(to.IsInteger32());
      ClearAllSideEffects();
      SetFlag(kTruncatingToInt32);
      SetFlag(kUseGVN);
    }
  }

  virtual HType CalculateInferredType();

  DECLARE_ABSTRACT_INSTRUCTION(BitwiseBinaryOperation)
};


class HArithmeticBinaryOperation: public HBinaryOperation {
 public:
  HArithmeticBinaryOperation(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    set_representation(Representation::Tagged());
    SetFlag(kFlexibleRepresentation);
    SetAllSideEffects();
  }

  virtual void RepresentationChanged(Representation to) {
    if (!to.IsTagged()) {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
  }

  virtual HType CalculateInferredType();
  virtual Representation RequiredInputRepresentation(int index) const {
    return index == 0
        ? Representation::Tagged()
        : representation();
  }

  virtual Representation InferredRepresentation() {
    if (left()->representation().Equals(right()->representation())) {
      return left()->representation();
    }
    return HValue::InferredRepresentation();
  }
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  Representation GetInputRepresentation() const {
    return Representation::Tagged();
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
    ASSERT(Token::IsCompareOp(token));
    SetOperandAt(0, left);
    SetOperandAt(1, right);
  }

  HValue* left() { return OperandAt(0); }
  HValue* right() { return OperandAt(1); }
  Token::Value token() const { return token_; }

  void SetInputRepresentation(Representation r);
  Representation GetInputRepresentation() const {
    return input_representation_;
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return input_representation_;
  }
  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(CompareIDAndBranch)

 private:
  Representation input_representation_;
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

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Integer32();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareConstantEqAndBranch);

 private:
  const Token::Value op_;
  const int right_;
};


class HIsNullAndBranch: public HUnaryControlInstruction {
 public:
  HIsNullAndBranch(HValue* value, bool is_strict)
      : HUnaryControlInstruction(value, NULL, NULL), is_strict_(is_strict) { }

  bool is_strict() const { return is_strict_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsNullAndBranch)

 private:
  bool is_strict_;
};


class HIsObjectAndBranch: public HUnaryControlInstruction {
 public:
  explicit HIsObjectAndBranch(HValue* value)
    : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsObjectAndBranch)
};


class HIsSmiAndBranch: public HUnaryControlInstruction {
 public:
  explicit HIsSmiAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch)

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HIsUndetectableAndBranch: public HUnaryControlInstruction {
 public:
  explicit HIsUndetectableAndBranch(HValue* value)
      : HUnaryControlInstruction(value, NULL, NULL) { }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(IsUndetectableAndBranch)
};


class HIsConstructCallAndBranch: public HTemplateControlInstruction<2, 0> {
 public:
  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(GetCachedArrayIndex)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HClassOfTestAndBranch: public HUnaryControlInstruction {
 public:
  HClassOfTestAndBranch(HValue* value, Handle<String> class_name)
      : HUnaryControlInstruction(value, NULL, NULL),
        class_name_(class_name) { }

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch)

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfKnownGlobal)

 private:
  Handle<JSFunction> function_;
};


class HPower: public HTemplateInstruction<2> {
 public:
  HPower(HValue* left, HValue* right) {
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    set_representation(Representation::Double());
    SetFlag(kUseGVN);
  }

  HValue* left() { return OperandAt(0); }
  HValue* right() { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) const {
    return index == 0
      ? Representation::Double()
      : Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Power)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HAdd: public HArithmeticBinaryOperation {
 public:
  HAdd(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }

  // Add is only commutative if two integer values are added and not if two
  // tagged values are added (because it might be a String concatenation).
  virtual bool IsCommutative() const {
    return !representation().IsTagged();
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(Add)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange();
};


class HSub: public HArithmeticBinaryOperation {
 public:
  HSub(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  DECLARE_CONCRETE_INSTRUCTION(Sub)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange();
};


class HMul: public HArithmeticBinaryOperation {
 public:
  HMul(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  // Only commutative if it is certain that not two objects are multiplicated.
  virtual bool IsCommutative() const {
    return !representation().IsTagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Mul)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange();
};


class HMod: public HArithmeticBinaryOperation {
 public:
  HMod(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanBeDivByZero);
  }

  bool HasPowerOf2Divisor() {
    if (right()->IsConstant() &&
        HConstant::cast(right())->HasInteger32Value()) {
      int32_t value = HConstant::cast(right())->Integer32Value();
      return value != 0 && (IsPowerOf2(value) || IsPowerOf2(-value));
    }

    return false;
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  DECLARE_CONCRETE_INSTRUCTION(Mod)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange();
};


class HDiv: public HArithmeticBinaryOperation {
 public:
  HDiv(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  DECLARE_CONCRETE_INSTRUCTION(Div)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange();
};


class HBitAnd: public HBitwiseBinaryOperation {
 public:
  HBitAnd(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }

  virtual bool IsCommutative() const { return true; }
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(BitAnd)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange();
};


class HBitXor: public HBitwiseBinaryOperation {
 public:
  HBitXor(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }

  virtual bool IsCommutative() const { return true; }
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(BitXor)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HBitOr: public HBitwiseBinaryOperation {
 public:
  HBitOr(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }

  virtual bool IsCommutative() const { return true; }
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(BitOr)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange();
};


class HShl: public HBitwiseBinaryOperation {
 public:
  HShl(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }

  virtual Range* InferRange();
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(Shl)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HShr: public HBitwiseBinaryOperation {
 public:
  HShr(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }

  virtual Range* InferRange();
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(Shr)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HSar: public HBitwiseBinaryOperation {
 public:
  HSar(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) { }

  virtual Range* InferRange();
  virtual HType CalculateInferredType();

  DECLARE_CONCRETE_INSTRUCTION(Sar)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HOsrEntry: public HTemplateInstruction<0> {
 public:
  explicit HOsrEntry(int ast_id) : ast_id_(ast_id) {
    SetFlag(kChangesOsrEntries);
  }

  int ast_id() const { return ast_id_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(OsrEntry)

 private:
  int ast_id_;
};


class HParameter: public HTemplateInstruction<0> {
 public:
  explicit HParameter(unsigned index) : index_(index) {
    set_representation(Representation::Tagged());
  }

  unsigned index() const { return index_; }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Parameter)

 private:
  unsigned index_;
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallStub)

 private:
  CodeStub::Major major_key_;
  TranscendentalCache::Type transcendental_type_;
};


class HUnknownOSRValue: public HTemplateInstruction<0> {
 public:
  HUnknownOSRValue() { set_representation(Representation::Tagged()); }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(UnknownOSRValue)
};


class HLoadGlobalCell: public HTemplateInstruction<0> {
 public:
  HLoadGlobalCell(Handle<JSGlobalPropertyCell> cell, bool check_hole_value)
      : cell_(cell), check_hole_value_(check_hole_value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnGlobalVars);
  }

  Handle<JSGlobalPropertyCell>  cell() const { return cell_; }
  bool check_hole_value() const { return check_hole_value_; }

  virtual void PrintDataTo(StringStream* stream);

  virtual intptr_t Hashcode() {
    ASSERT(!HEAP->allow_allocation(false));
    return reinterpret_cast<intptr_t>(*cell_);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalCell)

 protected:
  virtual bool DataEquals(HValue* other) {
    HLoadGlobalCell* b = HLoadGlobalCell::cast(other);
    return cell_.is_identical_to(b->cell());
  }

 private:
  Handle<JSGlobalPropertyCell> cell_;
  bool check_hole_value_;
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalGeneric)

 private:
  Handle<Object> name_;
  bool for_typeof_;
};


class HStoreGlobalCell: public HUnaryOperation {
 public:
  HStoreGlobalCell(HValue* value,
                   Handle<JSGlobalPropertyCell> cell,
                   bool check_hole_value)
      : HUnaryOperation(value),
        cell_(cell),
        check_hole_value_(check_hole_value) {
    SetFlag(kChangesGlobalVars);
  }

  Handle<JSGlobalPropertyCell> cell() const { return cell_; }
  bool check_hole_value() const { return check_hole_value_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobalCell)

 private:
  Handle<JSGlobalPropertyCell> cell_;
  bool check_hole_value_;
};


class HStoreGlobalGeneric: public HTemplateInstruction<3> {
 public:
  HStoreGlobalGeneric(HValue* context,
                      HValue* global_object,
                      Handle<Object> name,
                      HValue* value,
                      bool strict_mode)
      : name_(name),
        strict_mode_(strict_mode) {
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
  bool strict_mode() { return strict_mode_; }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobalGeneric)

 private:
  Handle<Object> name_;
  bool strict_mode_;
};


class HLoadContextSlot: public HUnaryOperation {
 public:
  HLoadContextSlot(HValue* context , int slot_index)
      : HUnaryOperation(context), slot_index_(slot_index) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnContextSlots);
  }

  int slot_index() const { return slot_index_; }

  virtual Representation RequiredInputRepresentation(int index) const {
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
  int slot_index_;
};


static inline bool StoringValueNeedsWriteBarrier(HValue* value) {
  return !value->type().IsBoolean()
      && !value->type().IsSmi()
      && !(value->IsConstant() && HConstant::cast(value)->InOldSpace());
}


class HStoreContextSlot: public HTemplateInstruction<2> {
 public:
  HStoreContextSlot(HValue* context, int slot_index, HValue* value)
      : slot_index_(slot_index) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    SetFlag(kChangesContextSlots);
  }

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }
  int slot_index() const { return slot_index_; }

  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreContextSlot)

 private:
  int slot_index_;
};


class HLoadNamedField: public HUnaryOperation {
 public:
  HLoadNamedField(HValue* object, bool is_in_object, int offset)
      : HUnaryOperation(object),
        is_in_object_(is_in_object),
        offset_(offset) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
    if (is_in_object) {
      SetFlag(kDependsOnInobjectFields);
    } else {
      SetFlag(kDependsOnBackingStoreFields);
    }
  }

  HValue* object() { return OperandAt(0); }
  bool is_in_object() const { return is_in_object_; }
  int offset() const { return offset_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField)

 protected:
  virtual bool DataEquals(HValue* other) {
    HLoadNamedField* b = HLoadNamedField::cast(other);
    return is_in_object_ == b->is_in_object_ && offset_ == b->offset_;
  }

 private:
  bool is_in_object_;
  int offset_;
};


class HLoadNamedFieldPolymorphic: public HTemplateInstruction<2> {
 public:
  HLoadNamedFieldPolymorphic(HValue* context,
                             HValue* object,
                             SmallMapList* types,
                             Handle<String> name);

  HValue* context() { return OperandAt(0); }
  HValue* object() { return OperandAt(1); }
  SmallMapList* types() { return &types_; }
  Handle<String> name() { return name_; }
  bool need_generic() { return need_generic_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedFieldPolymorphic)

  static const int kMaxLoadPolymorphism = 4;

 protected:
  virtual bool DataEquals(HValue* value);

 private:
  SmallMapList types_;
  Handle<String> name_;
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

  virtual Representation RequiredInputRepresentation(int index) const {
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
    SetFlag(kDependsOnCalls);
  }

  HValue* function() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HLoadKeyedFastElement: public HTemplateInstruction<2> {
 public:
  HLoadKeyedFastElement(HValue* obj, HValue* key) {
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    set_representation(Representation::Tagged());
    SetFlag(kDependsOnArrayElements);
    SetFlag(kUseGVN);
  }

  HValue* object() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) const {
    // The key is supposed to be Integer32.
    return index == 0
      ? Representation::Tagged()
      : Representation::Integer32();
  }

  virtual void PrintDataTo(StringStream* stream);

  bool RequiresHoleCheck() const;

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedFastElement)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HLoadKeyedFastDoubleElement: public HTemplateInstruction<2> {
 public:
  HLoadKeyedFastDoubleElement(HValue* elements, HValue* key) {
    SetOperandAt(0, elements);
    SetOperandAt(1, key);
    set_representation(Representation::Double());
    SetFlag(kDependsOnDoubleArrayElements);
    SetFlag(kUseGVN);
  }

  HValue* elements() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }

  virtual Representation RequiredInputRepresentation(int index) const {
    // The key is supposed to be Integer32.
    return index == 0
      ? Representation::Tagged()
      : Representation::Integer32();
  }

  virtual void PrintDataTo(StringStream* stream);

  bool RequiresHoleCheck() const;

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedFastDoubleElement)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HLoadKeyedSpecializedArrayElement: public HTemplateInstruction<2> {
 public:
  HLoadKeyedSpecializedArrayElement(HValue* external_elements,
                                    HValue* key,
                                    ElementsKind elements_kind)
      :  elements_kind_(elements_kind) {
    SetOperandAt(0, external_elements);
    SetOperandAt(1, key);
    if (elements_kind == EXTERNAL_FLOAT_ELEMENTS ||
        elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
      set_representation(Representation::Double());
    } else {
      set_representation(Representation::Integer32());
    }
    SetFlag(kDependsOnSpecializedArrayElements);
    // Native code could change the specialized array.
    SetFlag(kDependsOnCalls);
    SetFlag(kUseGVN);
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) const {
    // The key is supposed to be Integer32, but the base pointer
    // for the element load is a naked pointer.
    return index == 0
      ? Representation::External()
      : Representation::Integer32();
  }

  HValue* external_pointer() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }
  ElementsKind elements_kind() const { return elements_kind_; }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedSpecializedArrayElement)

 protected:
  virtual bool DataEquals(HValue* other) {
    if (!other->IsLoadKeyedSpecializedArrayElement()) return false;
    HLoadKeyedSpecializedArrayElement* cast_other =
        HLoadKeyedSpecializedArrayElement::cast(other);
    return elements_kind_ == cast_other->elements_kind();
  }

 private:
  ElementsKind elements_kind_;
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedGeneric)
};


class HStoreNamedField: public HTemplateInstruction<2> {
 public:
  HStoreNamedField(HValue* obj,
                   Handle<String> name,
                   HValue* val,
                   bool in_object,
                   int offset)
      : name_(name),
        is_in_object_(in_object),
        offset_(offset) {
    SetOperandAt(0, obj);
    SetOperandAt(1, val);
    if (is_in_object_) {
      SetFlag(kChangesInobjectFields);
    } else {
      SetFlag(kChangesBackingStoreFields);
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField)

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream);

  HValue* object() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  Handle<String> name() const { return name_; }
  bool is_in_object() const { return is_in_object_; }
  int offset() const { return offset_; }
  Handle<Map> transition() const { return transition_; }
  void set_transition(Handle<Map> map) { transition_ = map; }

  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

 private:
  Handle<String> name_;
  bool is_in_object_;
  int offset_;
  Handle<Map> transition_;
};


class HStoreNamedGeneric: public HTemplateInstruction<3> {
 public:
  HStoreNamedGeneric(HValue* context,
                     HValue* object,
                     Handle<String> name,
                     HValue* value,
                     bool strict_mode)
      : name_(name),
        strict_mode_(strict_mode) {
    SetOperandAt(0, object);
    SetOperandAt(1, value);
    SetOperandAt(2, context);
    SetAllSideEffects();
  }

  HValue* object() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }
  HValue* context() { return OperandAt(2); }
  Handle<String> name() { return name_; }
  bool strict_mode() { return strict_mode_; }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedGeneric)

 private:
  Handle<String> name_;
  bool strict_mode_;
};


class HStoreKeyedFastElement: public HTemplateInstruction<3> {
 public:
  HStoreKeyedFastElement(HValue* obj, HValue* key, HValue* val) {
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, val);
    SetFlag(kChangesArrayElements);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    // The key is supposed to be Integer32.
    return index == 1
        ? Representation::Integer32()
        : Representation::Tagged();
  }

  HValue* object() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }
  HValue* value() { return OperandAt(2); }

  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedFastElement)
};


class HStoreKeyedFastDoubleElement: public HTemplateInstruction<3> {
 public:
  HStoreKeyedFastDoubleElement(HValue* elements,
                               HValue* key,
                               HValue* val) {
    SetOperandAt(0, elements);
    SetOperandAt(1, key);
    SetOperandAt(2, val);
    SetFlag(kChangesDoubleArrayElements);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    if (index == 1) {
      return Representation::Integer32();
    } else if (index == 2) {
      return Representation::Double();
    } else {
      return Representation::Tagged();
    }
  }

  HValue* elements() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }
  HValue* value() { return OperandAt(2); }

  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedFastDoubleElement)
};


class HStoreKeyedSpecializedArrayElement: public HTemplateInstruction<3> {
 public:
  HStoreKeyedSpecializedArrayElement(HValue* external_elements,
                                     HValue* key,
                                     HValue* val,
                                     ElementsKind elements_kind)
      : elements_kind_(elements_kind) {
    SetFlag(kChangesSpecializedArrayElements);
    SetOperandAt(0, external_elements);
    SetOperandAt(1, key);
    SetOperandAt(2, val);
  }

  virtual void PrintDataTo(StringStream* stream);

  virtual Representation RequiredInputRepresentation(int index) const {
    if (index == 0) {
      return Representation::External();
    } else {
      bool float_or_double_elements =
          elements_kind() == EXTERNAL_FLOAT_ELEMENTS ||
          elements_kind() == EXTERNAL_DOUBLE_ELEMENTS;
      if (index == 2 && float_or_double_elements) {
        return Representation::Double();
      } else {
        return Representation::Integer32();
      }
    }
  }

  HValue* external_pointer() { return OperandAt(0); }
  HValue* key() { return OperandAt(1); }
  HValue* value() { return OperandAt(2); }
  ElementsKind elements_kind() const { return elements_kind_; }

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedSpecializedArrayElement)

 private:
  ElementsKind elements_kind_;
};


class HStoreKeyedGeneric: public HTemplateInstruction<4> {
 public:
  HStoreKeyedGeneric(HValue* context,
                     HValue* object,
                     HValue* key,
                     HValue* value,
                     bool strict_mode)
      : strict_mode_(strict_mode) {
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
  bool strict_mode() { return strict_mode_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedGeneric)

 private:
  bool strict_mode_;
};


class HStringAdd: public HBinaryOperation {
 public:
  HStringAdd(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() {
    return HType::String();
  }

  DECLARE_CONCRETE_INSTRUCTION(StringAdd)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }
};


class HStringCharCodeAt: public HTemplateInstruction<3> {
 public:
  HStringCharCodeAt(HValue* context, HValue* string, HValue* index) {
    SetOperandAt(0, context);
    SetOperandAt(1, string);
    SetOperandAt(2, index);
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Range* InferRange() {
    return new Range(0, String::kMaxUC16CharCode);
  }
};


class HStringCharFromCode: public HTemplateInstruction<2> {
 public:
  HStringCharFromCode(HValue* context, HValue* char_code) {
    SetOperandAt(0, context);
    SetOperandAt(1, char_code);
     set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }

  HValue* context() { return OperandAt(0); }
  HValue* value() { return OperandAt(1); }

  virtual bool DataEquals(HValue* other) { return true; }

  DECLARE_CONCRETE_INSTRUCTION(StringCharFromCode)
};


class HStringLength: public HUnaryOperation {
 public:
  explicit HStringLength(HValue* string) : HUnaryOperation(string) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() {
    STATIC_ASSERT(String::kMaxLength <= Smi::kMaxValue);
    return HType::Smi();
  }

  DECLARE_CONCRETE_INSTRUCTION(StringLength)

 protected:
  virtual bool DataEquals(HValue* other) { return true; }

  virtual Range* InferRange() {
    return new Range(0, String::kMaxLength);
  }
};


template <int V>
class HMaterializedLiteral: public HTemplateInstruction<V> {
 public:
  HMaterializedLiteral<V>(int index, int depth)
      : literal_index_(index), depth_(depth) {
    this->set_representation(Representation::Tagged());
  }

  int literal_index() const { return literal_index_; }
  int depth() const { return depth_; }

 private:
  int literal_index_;
  int depth_;
};


class HArrayLiteral: public HMaterializedLiteral<1> {
 public:
  HArrayLiteral(HValue* context,
                Handle<FixedArray> constant_elements,
                int length,
                int literal_index,
                int depth)
      : HMaterializedLiteral<1>(literal_index, depth),
        length_(length),
        constant_elements_(constant_elements) {
    SetOperandAt(0, context);
  }

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> constant_elements() const { return constant_elements_; }
  int length() const { return length_; }

  bool IsCopyOnWrite() const;

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ArrayLiteral)

 private:
  int length_;
  Handle<FixedArray> constant_elements_;
};


class HObjectLiteral: public HMaterializedLiteral<1> {
 public:
  HObjectLiteral(HValue* context,
                 Handle<FixedArray> constant_properties,
                 bool fast_elements,
                 int literal_index,
                 int depth,
                 bool has_function)
      : HMaterializedLiteral<1>(literal_index, depth),
        constant_properties_(constant_properties),
        fast_elements_(fast_elements),
        has_function_(has_function) {
    SetOperandAt(0, context);
  }

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> constant_properties() const {
    return constant_properties_;
  }
  bool fast_elements() const { return fast_elements_; }
  bool has_function() const { return has_function_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ObjectLiteral)

 private:
  Handle<FixedArray> constant_properties_;
  bool fast_elements_;
  bool has_function_;
};


class HRegExpLiteral: public HMaterializedLiteral<1> {
 public:
  HRegExpLiteral(HValue* context,
                 Handle<String> pattern,
                 Handle<String> flags,
                 int literal_index)
      : HMaterializedLiteral<1>(literal_index, 0),
        pattern_(pattern),
        flags_(flags) {
    SetOperandAt(0, context);
    SetAllSideEffects();
  }

  HValue* context() { return OperandAt(0); }
  Handle<String> pattern() { return pattern_; }
  Handle<String> flags() { return flags_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(RegExpLiteral)

 private:
  Handle<String> pattern_;
  Handle<String> flags_;
};


class HFunctionLiteral: public HTemplateInstruction<1> {
 public:
  HFunctionLiteral(HValue* context,
                   Handle<SharedFunctionInfo> shared,
                   bool pretenure)
      : shared_info_(shared), pretenure_(pretenure) {
    SetOperandAt(0, context);
    set_representation(Representation::Tagged());
  }

  HValue* context() { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(FunctionLiteral)

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  bool pretenure() const { return pretenure_; }

 private:
  Handle<SharedFunctionInfo> shared_info_;
  bool pretenure_;
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Typeof)
};


class HToFastProperties: public HUnaryOperation {
 public:
  explicit HToFastProperties(HValue* value) : HUnaryOperation(value) {
    // This instruction is not marked as having side effects, but
    // changes the map of the input operand. Use it only when creating
    // object literals.
    ASSERT(value->IsObjectLiteral());
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ToFastProperties)
};


class HValueOf: public HUnaryOperation {
 public:
  explicit HValueOf(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ValueOf)
};


class HDeleteProperty: public HBinaryOperation {
 public:
  HDeleteProperty(HValue* context, HValue* obj, HValue* key)
      : HBinaryOperation(context, obj, key) {
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  virtual Representation RequiredInputRepresentation(int index) const {
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

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual HType CalculateInferredType() {
    return HType::Boolean();
  }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(In)
};

#undef DECLARE_INSTRUCTION
#undef DECLARE_CONCRETE_INSTRUCTION

} }  // namespace v8::internal

#endif  // V8_HYDROGEN_INSTRUCTIONS_H_
