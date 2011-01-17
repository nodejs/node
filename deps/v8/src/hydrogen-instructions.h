// Copyright 2010 the V8 project authors. All rights reserved.
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
#include "code-stubs.h"
#include "string-stream.h"
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


// Type hierarchy:
//
// HValue
//   HInstruction
//     HAccessArgumentsAt
//     HApplyArguments
//     HArgumentsElements
//     HArgumentsLength
//     HArgumentsObject
//     HBinaryOperation
//       HArithmeticBinaryOperation
//         HAdd
//         HDiv
//         HMod
//         HMul
//         HSub
//       HBitwiseBinaryOperation
//         HBitAnd
//         HBitOr
//         HBitXor
//         HSar
//         HShl
//         HShr
//       HBoundsCheck
//       HCompare
//       HCompareJSObjectEq
//       HInstanceOf
//       HInstanceOfKnownGlobal
//       HLoadKeyed
//         HLoadKeyedFastElement
//         HLoadKeyedGeneric
//       HPower
//       HStoreNamed
//         HStoreNamedField
//         HStoreNamedGeneric
//     HBlockEntry
//     HCall
//       HCallConstantFunction
//       HCallFunction
//       HCallGlobal
//       HCallKeyed
//       HCallKnownGlobal
//       HCallNamed
//       HCallNew
//       HCallRuntime
//     HCallStub
//     HCheckPrototypeMaps
//     HConstant
//     HControlInstruction
//       HDeoptimize
//       HGoto
//       HUnaryControlInstruction
//         HBranch
//         HCompareMapAndBranch
//         HReturn
//         HThrow
//     HEnterInlined
//     HFunctionLiteral
//     HGlobalObject
//     HGlobalReceiver
//     HLeaveInlined
//     HLoadContextSlot
//     HLoadGlobal
//     HMaterializedLiteral
//       HArrayLiteral
//       HObjectLiteral
//       HRegExpLiteral
//     HOsrEntry
//     HParameter
//     HSimulate
//     HStackCheck
//     HStoreKeyed
//       HStoreKeyedFastElement
//       HStoreKeyedGeneric
//     HUnaryOperation
//       HBitNot
//       HChange
//       HCheckFunction
//       HCheckInstanceType
//       HCheckMap
//       HCheckNonSmi
//       HCheckSmi
//       HDeleteProperty
//       HFixedArrayLength
//       HJSArrayLength
//       HLoadElements
//         HTypeofIs
//       HLoadNamedField
//       HLoadNamedGeneric
//       HLoadFunctionPrototype
//       HPushArgument
//       HTypeof
//       HUnaryMathOperation
//       HUnaryPredicate
//         HClassOfTest
//         HHasCachedArrayIndex
//         HHasInstanceType
//         HIsNull
//         HIsObject
//         HIsSmi
//       HValueOf
//     HUnknownOSRValue
//   HPhi

#define HYDROGEN_ALL_INSTRUCTION_LIST(V)       \
  V(ArithmeticBinaryOperation)                 \
  V(BinaryOperation)                           \
  V(BitwiseBinaryOperation)                    \
  V(Call)                                      \
  V(ControlInstruction)                        \
  V(Instruction)                               \
  V(LoadKeyed)                                 \
  V(MaterializedLiteral)                       \
  V(Phi)                                       \
  V(StoreKeyed)                                \
  V(StoreNamed)                                \
  V(UnaryControlInstruction)                   \
  V(UnaryOperation)                            \
  HYDROGEN_CONCRETE_INSTRUCTION_LIST(V)


#define HYDROGEN_CONCRETE_INSTRUCTION_LIST(V)  \
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
  V(Compare)                                   \
  V(CompareJSObjectEq)                         \
  V(CompareMapAndBranch)                       \
  V(Constant)                                  \
  V(DeleteProperty)                            \
  V(Deoptimize)                                \
  V(Div)                                       \
  V(EnterInlined)                              \
  V(FixedArrayLength)                          \
  V(FunctionLiteral)                           \
  V(GlobalObject)                              \
  V(GlobalReceiver)                            \
  V(Goto)                                      \
  V(InstanceOf)                                \
  V(InstanceOfKnownGlobal)                     \
  V(IsNull)                                    \
  V(IsObject)                                  \
  V(IsSmi)                                     \
  V(HasInstanceType)                           \
  V(HasCachedArrayIndex)                       \
  V(JSArrayLength)                             \
  V(ClassOfTest)                               \
  V(LeaveInlined)                              \
  V(LoadContextSlot)                           \
  V(LoadElements)                              \
  V(LoadGlobal)                                \
  V(LoadKeyedFastElement)                      \
  V(LoadKeyedGeneric)                          \
  V(LoadNamedField)                            \
  V(LoadNamedGeneric)                          \
  V(LoadFunctionPrototype)                     \
  V(Mod)                                       \
  V(Mul)                                       \
  V(ObjectLiteral)                             \
  V(OsrEntry)                                  \
  V(Parameter)                                 \
  V(Power)                                     \
  V(PushArgument)                              \
  V(RegExpLiteral)                             \
  V(Return)                                    \
  V(Sar)                                       \
  V(Shl)                                       \
  V(Shr)                                       \
  V(Simulate)                                  \
  V(StackCheck)                                \
  V(StoreGlobal)                               \
  V(StoreKeyedFastElement)                     \
  V(StoreKeyedGeneric)                         \
  V(StoreNamedField)                           \
  V(StoreNamedGeneric)                         \
  V(Sub)                                       \
  V(Throw)                                     \
  V(Typeof)                                    \
  V(TypeofIs)                                  \
  V(UnaryMathOperation)                        \
  V(UnknownOSRValue)                           \
  V(ValueOf)

#define GVN_FLAG_LIST(V)                       \
  V(Calls)                                     \
  V(InobjectFields)                            \
  V(BackingStoreFields)                        \
  V(ArrayElements)                             \
  V(GlobalVars)                                \
  V(Maps)                                      \
  V(ArrayLengths)                              \
  V(FunctionPrototypes)                        \
  V(OsrEntries)

#define DECLARE_INSTRUCTION(type)                   \
  virtual bool Is##type() const { return true; }    \
  static H##type* cast(HValue* value) {             \
    ASSERT(value->Is##type());                      \
    return reinterpret_cast<H##type*>(value);       \
  }                                                 \
  Opcode opcode() const { return HValue::k##type; }


#define DECLARE_CONCRETE_INSTRUCTION(type, mnemonic)              \
  virtual LInstruction* CompileToLithium(LChunkBuilder* builder); \
  virtual const char* Mnemonic() const { return mnemonic; }       \
  DECLARE_INSTRUCTION(type)



template<int kSize>
class HOperandVector : public EmbeddedVector<HValue*, kSize> {
 public:
  HOperandVector() : EmbeddedVector<HValue*, kSize>(NULL) { }
};


class Range: public ZoneObject {
 public:
  Range() : lower_(kMinInt),
            upper_(kMaxInt),
            next_(NULL),
            can_be_minus_zero_(false) { }

  Range(int32_t lower, int32_t upper)
      : lower_(lower), upper_(upper), next_(NULL), can_be_minus_zero_(false) { }

  bool IsInSmiRange() const {
    return lower_ >= Smi::kMinValue && upper_ <= Smi::kMaxValue;
  }
  void KeepOrder();
  void Verify() const;
  int32_t upper() const { return upper_; }
  int32_t lower() const { return lower_; }
  Range* next() const { return next_; }
  Range* CopyClearLower() const { return new Range(kMinInt, upper_); }
  Range* CopyClearUpper() const { return new Range(lower_, kMaxInt); }
  void ClearLower() { lower_ = kMinInt; }
  void ClearUpper() { upper_ = kMaxInt; }
  Range* Copy() const { return new Range(lower_, upper_); }
  bool IsMostGeneric() const { return lower_ == kMinInt && upper_ == kMaxInt; }
  int32_t Mask() const;
  void set_can_be_minus_zero(bool b) { can_be_minus_zero_ = b; }
  bool CanBeMinusZero() const { return CanBeZero() && can_be_minus_zero_; }
  bool CanBeZero() const { return upper_ >= 0 && lower_ <= 0; }
  bool CanBeNegative() const { return lower_ < 0; }
  bool Includes(int value) const {
    return lower_ <= value && upper_ >= value;
  }

  void Sar(int32_t value) {
    int32_t bits = value & 0x1F;
    lower_ = lower_ >> bits;
    upper_ = upper_ >> bits;
    set_can_be_minus_zero(false);
  }

  void Shl(int32_t value) {
    int32_t bits = value & 0x1F;
    int old_lower = lower_;
    int old_upper = upper_;
    lower_ = lower_ << bits;
    upper_ = upper_ << bits;
    if (old_lower != lower_ >> bits || old_upper != upper_ >> bits) {
      upper_ = kMaxInt;
      lower_ = kMinInt;
    }
    set_can_be_minus_zero(false);
  }

  // Adds a constant to the lower and upper bound of the range.
  void AddConstant(int32_t value);

  void StackUpon(Range* other) {
    Intersect(other);
    next_ = other;
  }

  void Intersect(Range* other) {
    upper_ = Min(upper_, other->upper_);
    lower_ = Max(lower_, other->lower_);
    bool b = CanBeMinusZero() && other->CanBeMinusZero();
    set_can_be_minus_zero(b);
  }

  void Union(Range* other) {
    upper_ = Max(upper_, other->upper_);
    lower_ = Min(lower_, other->lower_);
    bool b = CanBeMinusZero() || other->CanBeMinusZero();
    set_can_be_minus_zero(b);
  }

  // Compute a new result range and return true, if the operation
  // can overflow.
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
    kNumRepresentations
  };

  Representation() : kind_(kNone) { }

  static Representation None() { return Representation(kNone); }
  static Representation Tagged() { return Representation(kTagged); }
  static Representation Integer32() { return Representation(kInteger32); }
  static Representation Double() { return Representation(kDouble); }

  bool Equals(const Representation& other) const {
    return kind_ == other.kind_;
  }

  Kind kind() const { return kind_; }
  bool IsNone() const { return kind_ == kNone; }
  bool IsTagged() const { return kind_ == kTagged; }
  bool IsInteger32() const { return kind_ == kInteger32; }
  bool IsDouble() const { return kind_ == kDouble; }
  bool IsSpecialization() const {
    return kind_ == kInteger32 || kind_ == kDouble;
  }
  const char* Mnemonic() const;

 private:
  explicit Representation(Kind k) : kind_(k) { }

  Kind kind_;
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
    kJSArray = 0x701,        // 0000 0111 1000 0001
    kUninitialized = 0x1fff  // 0001 1111 1111 1111
  };

  explicit HType(Type t) : type_(t) { }

  Type type_;
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
    kUseGVN,
    kCanOverflow,
    kBailoutOnMinusZero,
    kCanBeDivByZero,
    kIsArguments,
    kTruncatingToInt32,
    kLastFlag = kTruncatingToInt32
  };

  STATIC_ASSERT(kLastFlag < kBitsPerInt);

  static const int kChangesToDependsFlagsLeftShift = 1;

  static int ChangesFlagsMask() {
    int result = 0;
    // Create changes mask.
#define DECLARE_DO(type) result |= (1 << kChanges##type);
  GVN_FLAG_LIST(DECLARE_DO)
#undef DECLARE_DO
    return result;
  }

  static int DependsFlagsMask() {
    return ConvertChangesToDependsFlags(ChangesFlagsMask());
  }

  static int ConvertChangesToDependsFlags(int flags) {
    return flags << kChangesToDependsFlagsLeftShift;
  }

  // A flag mask to mark an instruction as having arbitrary side effects.
  static int AllSideEffects() {
    return ChangesFlagsMask() & ~(1 << kChangesOsrEntries);
  }

  static HValue* cast(HValue* value) { return value; }

  enum Opcode {
    // Declare a unique enum value for each hydrogen instruction.
  #define DECLARE_DO(type) k##type,
    HYDROGEN_ALL_INSTRUCTION_LIST(DECLARE_DO)
  #undef DECLARE_DO
    kMaxInstructionClass
  };

  HValue() : block_(NULL),
             id_(kNoNumber),
             uses_(2),
             type_(HType::Tagged()),
             range_(NULL),
             flags_(0) {}
  virtual ~HValue() {}

  HBasicBlock* block() const { return block_; }
  void SetBlock(HBasicBlock* block);

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  const ZoneList<HValue*>* uses() const { return &uses_; }

  virtual bool EmitAtUses() const { return false; }
  Representation representation() const { return representation_; }
  void ChangeRepresentation(Representation r) {
    // Representation was already set and is allowed to be changed.
    ASSERT(!representation_.IsNone());
    ASSERT(!r.IsNone());
    ASSERT(CheckFlag(kFlexibleRepresentation));
    RepresentationChanged(r);
    representation_ = r;
  }

  HType type() const { return type_; }
  void set_type(HType type) {
    ASSERT(uses_.length() == 0);
    type_ = type;
  }

  // An operation needs to override this function iff:
  //   1) it can produce an int32 output.
  //   2) the true value of its output can potentially be minus zero.
  // The implementation must set a flag so that it bails out in the case where
  // it would otherwise output what should be a minus zero as an int32 zero.
  // If the operation also exists in a form that takes int32 and outputs int32
  // then the operation should return its input value so that we can propagate
  // back.  There are two operations that need to propagate back to more than
  // one input.  They are phi and binary add.  They always return NULL and
  // expect the caller to take care of things.
  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited) {
    visited->Add(id());
    return NULL;
  }

  bool HasSideEffects() const {
    return (flags_ & AllSideEffects()) != 0;
  }
  bool IsDefinedAfter(HBasicBlock* other) const;

  // Operands.
  virtual int OperandCount() const { return 0; }
  virtual HValue* OperandAt(int index) const {
    UNREACHABLE();
    return NULL;
  }
  void SetOperandAt(int index, HValue* value);

  int LookupOperandIndex(int occurrence_index, HValue* op) const;
  bool UsesMultipleTimes(HValue* op) const;

  void ReplaceAndDelete(HValue* other);
  void ReplaceValue(HValue* other);
  void ReplaceAtUse(HValue* use, HValue* other);
  void ReplaceFirstAtUse(HValue* use, HValue* other, Representation r);
  bool HasNoUses() const { return uses_.is_empty(); }
  void ClearOperands();
  void Delete();

  int flags() const { return flags_; }
  void SetFlagMask(int mask) { flags_ |= mask; }
  void SetFlag(Flag f) { SetFlagMask(1 << f); }
  void ClearFlagMask(int mask) { flags_ &= ~mask; }
  void ClearFlag(Flag f) { ClearFlagMask(1 << f); }
  bool CheckFlag(Flag f) const { return CheckFlagMask(1 << f); }
  bool CheckFlagMask(int mask) const { return (flags_ & mask) != 0; }

  Range* range() const { return range_; }
  bool HasRange() const { return range_ != NULL; }
  void AddNewRange(Range* r);
  void RemoveLastAddedRange();
  void ComputeInitialRange();

  // Representation helpers.
  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }
  virtual Representation InferredRepresentation() const {
    return representation();
  }

  // This gives the instruction an opportunity to replace itself with an
  // instruction that does the same in some better way.  To replace an
  // instruction with a new one, first add the new instruction to the graph,
  // then return it.  Return NULL to have the instruction deleted.
  virtual HValue* Canonicalize() { return this; }

  // Declare virtual type testers.
#define DECLARE_DO(type) virtual bool Is##type() const { return false; }
  HYDROGEN_ALL_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

  bool Equals(HValue* other) const;
  virtual intptr_t Hashcode() const;

  // Printing support.
  virtual void PrintTo(StringStream* stream) const = 0;
  void PrintNameTo(StringStream* stream);
  static void PrintTypeTo(HType type, StringStream* stream);

  virtual const char* Mnemonic() const = 0;
  virtual Opcode opcode() const = 0;

  // Updated the inferred type of this instruction and returns true if
  // it has changed.
  bool UpdateInferredType();

  virtual HType CalculateInferredType() const;

  // Helper for type conversions used by normal and phi instructions.
  void InsertInputConversion(HInstruction* previous, int index, HType type);

#ifdef DEBUG
  virtual void Verify() const = 0;
#endif

 protected:
  virtual bool DataEquals(HValue* other) const { return true; }
  virtual void RepresentationChanged(Representation to) { }
  virtual Range* InferRange();
  virtual void DeleteFromGraph() = 0;
  virtual void InternalSetOperandAt(int index, HValue* value) { UNREACHABLE(); }
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
  void InternalReplaceAtUse(HValue* use, HValue* other);
  void RegisterUse(int index, HValue* new_value);

  HBasicBlock* block_;

  // The id of this instruction in the hydrogen graph, assigned when first
  // added to the graph. Reflects creation order.
  int id_;

  Representation representation_;
  ZoneList<HValue*> uses_;
  HType type_;
  Range* range_;
  int flags_;

  DISALLOW_COPY_AND_ASSIGN(HValue);
};


class HInstruction: public HValue {
 public:
  HInstruction* next() const { return next_; }
  HInstruction* previous() const { return previous_; }

  void PrintTo(StringStream* stream) const;
  virtual void PrintDataTo(StringStream* stream) const {}

  bool IsLinked() const { return block() != NULL; }
  void Unlink();
  void InsertBefore(HInstruction* next);
  void InsertAfter(HInstruction* previous);

  int position() const { return position_; }
  bool has_position() const { return position_ != RelocInfo::kNoPosition; }
  void set_position(int position) { position_ = position; }

  virtual LInstruction* CompileToLithium(LChunkBuilder* builder) = 0;

#ifdef DEBUG
  virtual void Verify() const;
#endif

  DECLARE_INSTRUCTION(Instruction)

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

  HInstruction* next_;
  HInstruction* previous_;
  int position_;

  friend class HBasicBlock;
};


class HBlockEntry: public HInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(BlockEntry, "block_entry")
};


class HControlInstruction: public HInstruction {
 public:
  virtual HBasicBlock* FirstSuccessor() const { return NULL; }
  virtual HBasicBlock* SecondSuccessor() const { return NULL; }

  DECLARE_INSTRUCTION(ControlInstruction)
};


class HDeoptimize: public HControlInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Deoptimize, "deoptimize")
};


class HGoto: public HControlInstruction {
 public:
  explicit HGoto(HBasicBlock* destination)
      : destination_(destination),
        include_stack_check_(false) {}

  virtual HBasicBlock* FirstSuccessor() const { return destination_; }
  void set_include_stack_check(bool include_stack_check) {
    include_stack_check_ = include_stack_check;
  }
  bool include_stack_check() const { return include_stack_check_; }

  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(Goto, "goto")

 private:
  HBasicBlock* destination_;
  bool include_stack_check_;
};


class HUnaryControlInstruction: public HControlInstruction {
 public:
  explicit HUnaryControlInstruction(HValue* value) {
    SetOperandAt(0, value);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  HValue* value() const { return OperandAt(0); }
  virtual int OperandCount() const { return 1; }
  virtual HValue* OperandAt(int index) const { return operands_[index]; }

  DECLARE_INSTRUCTION(UnaryControlInstruction)

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    operands_[index] = value;
  }

 private:
  HOperandVector<1> operands_;
};


class HBranch: public HUnaryControlInstruction {
 public:
  HBranch(HBasicBlock* true_destination,
          HBasicBlock* false_destination,
          HValue* boolean_value)
      : HUnaryControlInstruction(boolean_value),
        true_destination_(true_destination),
        false_destination_(false_destination) {
    ASSERT(true_destination != NULL && false_destination != NULL);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::None();
  }

  virtual HBasicBlock* FirstSuccessor() const { return true_destination_; }
  virtual HBasicBlock* SecondSuccessor() const { return false_destination_; }

  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(Branch, "branch")

 private:
  HBasicBlock* true_destination_;
  HBasicBlock* false_destination_;
};


class HCompareMapAndBranch: public HUnaryControlInstruction {
 public:
  HCompareMapAndBranch(HValue* result,
                       Handle<Map> map,
                       HBasicBlock* true_destination,
                       HBasicBlock* false_destination)
      : HUnaryControlInstruction(result),
        map_(map),
        true_destination_(true_destination),
        false_destination_(false_destination) {
    ASSERT(true_destination != NULL);
    ASSERT(false_destination != NULL);
    ASSERT(!map.is_null());
  }

  virtual HBasicBlock* FirstSuccessor() const { return true_destination_; }
  virtual HBasicBlock* SecondSuccessor() const { return false_destination_; }

  HBasicBlock* true_destination() const { return true_destination_; }
  HBasicBlock* false_destination() const { return false_destination_; }

  virtual void PrintDataTo(StringStream* stream) const;

  Handle<Map> map() const { return map_; }

  DECLARE_CONCRETE_INSTRUCTION(CompareMapAndBranch, "compare_map_and_branch")

 private:
  Handle<Map> map_;
  HBasicBlock* true_destination_;
  HBasicBlock* false_destination_;
};


class HReturn: public HUnaryControlInstruction {
 public:
  explicit HReturn(HValue* result) : HUnaryControlInstruction(result) { }

  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(Return, "return")
};


class HThrow: public HUnaryControlInstruction {
 public:
  explicit HThrow(HValue* value) : HUnaryControlInstruction(value) { }

  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(Throw, "throw")
};


class HUnaryOperation: public HInstruction {
 public:
  explicit HUnaryOperation(HValue* value) {
    SetOperandAt(0, value);
  }

  HValue* value() const { return OperandAt(0); }
  virtual void PrintDataTo(StringStream* stream) const;
  virtual int OperandCount() const { return 1; }
  virtual HValue* OperandAt(int index) const { return operands_[index]; }

  DECLARE_INSTRUCTION(UnaryOperation)

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    operands_[index] = value;
  }

 private:
  HOperandVector<1> operands_;
};


class HChange: public HUnaryOperation {
 public:
  HChange(HValue* value,
          Representation from,
          Representation to)
      : HUnaryOperation(value), from_(from), to_(to) {
    ASSERT(!from.IsNone() && !to.IsNone());
    ASSERT(!from.Equals(to));
    set_representation(to);
    SetFlag(kUseGVN);

    if (from.IsInteger32() && to.IsTagged() && value->range() != NULL &&
        value->range()->IsInSmiRange()) {
      set_type(HType::Smi());
    }
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  Representation from() const { return from_; }
  Representation to() const { return to_; }
  virtual Representation RequiredInputRepresentation(int index) const {
    return from_;
  }

  bool CanTruncateToInt32() const {
    for (int i = 0; i < uses()->length(); ++i) {
      if (!uses()->at(i)->CheckFlag(HValue::kTruncatingToInt32)) return false;
    }
    return true;
  }

  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(Change,
                               CanTruncateToInt32() ? "truncate" : "change")

 protected:
  virtual bool DataEquals(HValue* other) const {
    if (!other->IsChange()) return false;
    HChange* change = HChange::cast(other);
    return value() == change->value()
        && to().Equals(change->to())
        && CanTruncateToInt32() == change->CanTruncateToInt32();
  }

 private:
  Representation from_;
  Representation to_;
};


class HSimulate: public HInstruction {
 public:
  HSimulate(int ast_id, int pop_count, int environment_length)
      : ast_id_(ast_id),
        pop_count_(pop_count),
        environment_length_(environment_length),
        values_(2),
        assigned_indexes_(2) {}
  virtual ~HSimulate() {}

  virtual void PrintDataTo(StringStream* stream) const;

  bool HasAstId() const { return ast_id_ != AstNode::kNoNumber; }
  int ast_id() const { return ast_id_; }
  void set_ast_id(int id) {
    ASSERT(!HasAstId());
    ast_id_ = id;
  }

  int environment_length() const { return environment_length_; }
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
  virtual int OperandCount() const { return values_.length(); }
  virtual HValue* OperandAt(int index) const { return values_[index]; }

  DECLARE_CONCRETE_INSTRUCTION(Simulate, "simulate")

#ifdef DEBUG
  virtual void Verify() const;
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
  int environment_length_;
  ZoneList<HValue*> values_;
  ZoneList<int> assigned_indexes_;
};


class HStackCheck: public HInstruction {
 public:
  HStackCheck() { }

  DECLARE_CONCRETE_INSTRUCTION(Throw, "stack_check")
};


class HEnterInlined: public HInstruction {
 public:
  HEnterInlined(Handle<JSFunction> closure, FunctionLiteral* function)
      : closure_(closure), function_(function) {
  }

  virtual void PrintDataTo(StringStream* stream) const;

  Handle<JSFunction> closure() const { return closure_; }
  FunctionLiteral* function() const { return function_; }

  DECLARE_CONCRETE_INSTRUCTION(EnterInlined, "enter_inlined")

 private:
  Handle<JSFunction> closure_;
  FunctionLiteral* function_;
};


class HLeaveInlined: public HInstruction {
 public:
  HLeaveInlined() {}

  DECLARE_CONCRETE_INSTRUCTION(LeaveInlined, "leave_inlined")
};


class HPushArgument: public HUnaryOperation {
 public:
  explicit HPushArgument(HValue* value)
      : HUnaryOperation(value), argument_index_(-1) {
    set_representation(Representation::Tagged());
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) const;
  HValue* argument() const { return OperandAt(0); }
  int argument_index() const { return argument_index_; }
  void set_argument_index(int index) {
    ASSERT(argument_index_ == -1 || index == argument_index_);
    argument_index_ = index;
  }

  DECLARE_CONCRETE_INSTRUCTION(PushArgument, "push_argument")

 private:
  int argument_index_;
};


class HGlobalObject: public HInstruction {
 public:
  HGlobalObject() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnCalls);
  }

  DECLARE_CONCRETE_INSTRUCTION(GlobalObject, "global_object")
};


class HGlobalReceiver: public HInstruction {
 public:
  HGlobalReceiver() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnCalls);
  }

  DECLARE_CONCRETE_INSTRUCTION(GlobalReceiver, "global_receiver")
};


class HCall: public HInstruction {
 public:
  // Construct a call with uninitialized arguments. The argument count
  // includes the receiver.
  explicit HCall(int count);

  virtual HType CalculateInferredType() const { return HType::Tagged(); }

  // TODO(3190496): This needs a cleanup. We don't want the arguments
  // be operands of the call instruction. This results in bad code quality.
  virtual int argument_count() const { return arguments_.length(); }
  virtual int OperandCount() const { return argument_count(); }
  virtual HValue* OperandAt(int index) const { return arguments_[index]; }
  virtual HPushArgument* PushArgumentAt(int index) const {
    return HPushArgument::cast(OperandAt(index));
  }
  virtual HValue* ArgumentAt(int index) const {
    return PushArgumentAt(index)->argument();
  }
  virtual void SetArgumentAt(int index, HPushArgument* push_argument);

  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_INSTRUCTION(Call)

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    arguments_[index] = value;
  }

  int argument_count_;
  Vector<HValue*> arguments_;
};


class HCallConstantFunction: public HCall {
 public:
  HCallConstantFunction(Handle<JSFunction> function, int argument_count)
      : HCall(argument_count), function_(function) { }

  Handle<JSFunction> function() const { return function_; }
  bool IsApplyFunction() const {
    return function_->code() == Builtins::builtin(Builtins::FunctionApply);
  }

  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(CallConstantFunction, "call_constant_function")

 private:
  Handle<JSFunction> function_;
};


class HCallKeyed: public HCall {
 public:
  HCallKeyed(HValue* key, int argument_count)
      : HCall(argument_count + 1) {
    SetOperandAt(0, key);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  // TODO(3190496): This is a hack to get an additional operand that
  // is not an argument to work with the current setup. This _needs_ a cleanup.
  // (see HCall)
  virtual void PrintDataTo(StringStream* stream) const;
  HValue* key() const { return OperandAt(0); }
  virtual int argument_count() const { return arguments_.length() - 1; }
  virtual int OperandCount() const { return arguments_.length(); }
  virtual HValue* OperandAt(int index) const { return arguments_[index]; }
  virtual HPushArgument* PushArgumentAt(int index) const {
    return HPushArgument::cast(OperandAt(index + 1));
  }
  virtual void SetArgumentAt(int index, HPushArgument* push_argument) {
    HCall::SetArgumentAt(index + 1, push_argument);
  }

  DECLARE_CONCRETE_INSTRUCTION(CallKeyed, "call_keyed")
};


class HCallNamed: public HCall {
 public:
  HCallNamed(Handle<String> name, int argument_count)
      : HCall(argument_count), name_(name) { }
  virtual void PrintDataTo(StringStream* stream) const;

  Handle<String> name() const { return name_; }

  DECLARE_CONCRETE_INSTRUCTION(CallNamed, "call_named")

 private:
  Handle<String> name_;
};


class HCallFunction: public HCall {
 public:
  explicit HCallFunction(int argument_count) : HCall(argument_count) { }

  DECLARE_CONCRETE_INSTRUCTION(CallFunction, "call_function")
};


class HCallGlobal: public HCall {
 public:
  HCallGlobal(Handle<String> name, int argument_count)
      : HCall(argument_count), name_(name) { }

  virtual void PrintDataTo(StringStream* stream) const;

  Handle<String> name() const { return name_; }

  DECLARE_CONCRETE_INSTRUCTION(CallGlobal, "call_global")

 private:
  Handle<String> name_;
};


class HCallKnownGlobal: public HCall {
 public:
  HCallKnownGlobal(Handle<JSFunction> target,
                   int argument_count)
      : HCall(argument_count), target_(target) { }

  Handle<JSFunction> target() const { return target_; }

  DECLARE_CONCRETE_INSTRUCTION(CallKnownGlobal, "call_known_global")

 private:
  Handle<JSFunction> target_;
};


class HCallNew: public HCall {
 public:
  explicit HCallNew(int argument_count) : HCall(argument_count) { }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  HValue* constructor() const { return ArgumentAt(0); }

  DECLARE_CONCRETE_INSTRUCTION(CallNew, "call_new")
};


class HCallRuntime: public HCall {
 public:
  HCallRuntime(Handle<String> name,
               Runtime::Function* c_function,
               int argument_count)
      : HCall(argument_count), c_function_(c_function), name_(name) { }
  virtual void PrintDataTo(StringStream* stream) const;

  Runtime::Function* function() const { return c_function_; }
  Handle<String> name() const { return name_; }

  DECLARE_CONCRETE_INSTRUCTION(CallRuntime, "call_runtime")

 private:
  Runtime::Function* c_function_;
  Handle<String> name_;
};


class HJSArrayLength: public HUnaryOperation {
 public:
  explicit HJSArrayLength(HValue* value) : HUnaryOperation(value) {
    // The length of an array is stored as a tagged value in the array
    // object. It is guaranteed to be 32 bit integer, but it can be
    // represented as either a smi or heap number.
    set_representation(Representation::Tagged());
    SetFlag(kDependsOnArrayLengths);
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(JSArrayLength, "js_array_length")
};


class HFixedArrayLength: public HUnaryOperation {
 public:
  explicit HFixedArrayLength(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kDependsOnArrayLengths);
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(FixedArrayLength, "fixed_array_length")
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
  virtual HType CalculateInferredType() const;

  DECLARE_CONCRETE_INSTRUCTION(BitNot, "bit_not")
};


class HUnaryMathOperation: public HUnaryOperation {
 public:
  HUnaryMathOperation(HValue* value, BuiltinFunctionId op)
      : HUnaryOperation(value), op_(op) {
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

  virtual void PrintDataTo(StringStream* stream) const;

  virtual HType CalculateInferredType() const;

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual Representation RequiredInputRepresentation(int index) const {
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
        break;
      case kMathAbs:
        return representation();
        break;
      default:
        return Representation::None();
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

  DECLARE_CONCRETE_INSTRUCTION(UnaryMathOperation, "unary_math_operation")

 protected:
  virtual bool DataEquals(HValue* other) const {
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

  DECLARE_CONCRETE_INSTRUCTION(LoadElements, "load-elements")
};


class HCheckMap: public HUnaryOperation {
 public:
  HCheckMap(HValue* value, Handle<Map> map)
      : HUnaryOperation(value), map_(map) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream) const;
  virtual HType CalculateInferredType() const;

#ifdef DEBUG
  virtual void Verify() const;
#endif

  Handle<Map> map() const { return map_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckMap, "check_map")

 protected:
  virtual bool DataEquals(HValue* other) const {
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
  virtual void PrintDataTo(StringStream* stream) const;
  virtual HType CalculateInferredType() const;

#ifdef DEBUG
  virtual void Verify() const;
#endif

  Handle<JSFunction> target() const { return target_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckFunction, "check_function")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HCheckFunction* b = HCheckFunction::cast(other);
    return target_.is_identical_to(b->target());
  }

 private:
  Handle<JSFunction> target_;
};


class HCheckInstanceType: public HUnaryOperation {
 public:
  // Check that the instance type is in the range [first, last] where
  // both first and last are included.
  HCheckInstanceType(HValue* value, InstanceType first, InstanceType last)
      : HUnaryOperation(value), first_(first), last_(last) {
    ASSERT(first <= last);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

#ifdef DEBUG
  virtual void Verify() const;
#endif

  static HCheckInstanceType* NewIsJSObjectOrJSFunction(HValue* value);

  InstanceType first() const { return first_; }
  InstanceType last() const { return last_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckInstanceType, "check_instance_type")

 protected:
  // TODO(ager): It could be nice to allow the ommision of instance
  // type checks if we have already performed an instance type check
  // with a larger range.
  virtual bool DataEquals(HValue* other) const {
    HCheckInstanceType* b = HCheckInstanceType::cast(other);
    return (first_ == b->first()) && (last_ == b->last());
  }

 private:
  InstanceType first_;
  InstanceType last_;
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

  virtual HType CalculateInferredType() const;

#ifdef DEBUG
  virtual void Verify() const;
#endif

  DECLARE_CONCRETE_INSTRUCTION(CheckNonSmi, "check_non_smi")
};


class HCheckPrototypeMaps: public HInstruction {
 public:
  HCheckPrototypeMaps(Handle<JSObject> prototype, Handle<JSObject> holder)
      : prototype_(prototype), holder_(holder) {
    SetFlag(kUseGVN);
    SetFlag(kDependsOnMaps);
  }

#ifdef DEBUG
  virtual void Verify() const;
#endif

  Handle<JSObject> prototype() const { return prototype_; }
  Handle<JSObject> holder() const { return holder_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckPrototypeMaps, "check_prototype_maps")

  virtual intptr_t Hashcode() const {
    ASSERT(!Heap::IsAllocationAllowed());
    intptr_t hash = reinterpret_cast<intptr_t>(*prototype());
    hash = 17 * hash + reinterpret_cast<intptr_t>(*holder());
    return hash;
  }

 protected:
  virtual bool DataEquals(HValue* other) const {
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
  virtual HType CalculateInferredType() const;

#ifdef DEBUG
  virtual void Verify() const;
#endif

  DECLARE_CONCRETE_INSTRUCTION(CheckSmi, "check_smi")
};


class HPhi: public HValue {
 public:
  explicit HPhi(int merged_index)
      : inputs_(2),
        merged_index_(merged_index),
        phi_id_(-1) {
    for (int i = 0; i < Representation::kNumRepresentations; i++) {
      non_phi_uses_[i] = 0;
      indirect_uses_[i] = 0;
    }
    ASSERT(merged_index >= 0);
    set_representation(Representation::Tagged());
    SetFlag(kFlexibleRepresentation);
  }

  virtual Representation InferredRepresentation() const {
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
  virtual HType CalculateInferredType() const;
  virtual int OperandCount() const { return inputs_.length(); }
  virtual HValue* OperandAt(int index) const { return inputs_[index]; }
  HValue* GetRedundantReplacement() const;
  void AddInput(HValue* value);

  bool HasReceiverOperand();

  int merged_index() const { return merged_index_; }

  virtual const char* Mnemonic() const { return "phi"; }

  virtual void PrintTo(StringStream* stream) const;

#ifdef DEBUG
  virtual void Verify() const;
#endif

  DECLARE_INSTRUCTION(Phi)

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
};


class HArgumentsObject: public HInstruction {
 public:
  HArgumentsObject() {
    set_representation(Representation::Tagged());
    SetFlag(kIsArguments);
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsObject, "arguments-object")
};


class HConstant: public HInstruction {
 public:
  HConstant(Handle<Object> handle, Representation r);

  Handle<Object> handle() const { return handle_; }

  bool InOldSpace() const { return !Heap::InNewSpace(*handle_); }

  virtual bool EmitAtUses() const { return !representation().IsDouble(); }
  virtual void PrintDataTo(StringStream* stream) const;
  virtual HType CalculateInferredType() const;
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

  virtual intptr_t Hashcode() const {
    ASSERT(!Heap::allow_allocation(false));
    return reinterpret_cast<intptr_t>(*handle());
  }

#ifdef DEBUG
  virtual void Verify() const { }
#endif

  DECLARE_CONCRETE_INSTRUCTION(Constant, "constant")

 protected:
  virtual Range* InferRange();

  virtual bool DataEquals(HValue* other) const {
    HConstant* other_constant = HConstant::cast(other);
    return handle().is_identical_to(other_constant->handle());
  }

 private:
  Handle<Object> handle_;
  HType constant_type_;

  // The following two values represent the int32 and the double value of the
  // given constant if there is a lossless conversion between the constant
  // and the specific representation.
  bool has_int32_value_;
  int32_t int32_value_;
  bool has_double_value_;
  double double_value_;
};


class HBinaryOperation: public HInstruction {
 public:
  HBinaryOperation(HValue* left, HValue* right) {
    ASSERT(left != NULL && right != NULL);
    SetOperandAt(0, left);
    SetOperandAt(1, right);
  }

  HValue* left() const { return OperandAt(0); }
  HValue* right() const { return OperandAt(1); }

  // TODO(kasperl): Move these helpers to the IA-32 Lithium
  // instruction sequence builder.
  HValue* LeastConstantOperand() const {
    if (IsCommutative() && left()->IsConstant()) return right();
    return left();
  }
  HValue* MostConstantOperand() const {
    if (IsCommutative() && left()->IsConstant()) return left();
    return right();
  }

  virtual bool IsCommutative() const { return false; }

  virtual void PrintDataTo(StringStream* stream) const;
  virtual int OperandCount() const { return operands_.length(); }
  virtual HValue* OperandAt(int index) const { return operands_[index]; }

  DECLARE_INSTRUCTION(BinaryOperation)

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    operands_[index] = value;
  }

 private:
  HOperandVector<2> operands_;
};


class HApplyArguments: public HInstruction {
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
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    // The length is untagged, all other inputs are tagged.
    return (index == 2)
        ? Representation::Integer32()
        : Representation::Tagged();
  }

  HValue* function() const { return OperandAt(0); }
  HValue* receiver() const { return OperandAt(1); }
  HValue* length() const { return OperandAt(2); }
  HValue* elements() const { return OperandAt(3); }

  virtual int OperandCount() const { return operands_.length(); }
  virtual HValue* OperandAt(int index) const { return operands_[index]; }

  DECLARE_CONCRETE_INSTRUCTION(ApplyArguments, "apply_arguments")



 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    operands_[index] = value;
  }

 private:
  HOperandVector<4> operands_;
};


class HArgumentsElements: public HInstruction {
 public:
  HArgumentsElements() {
    // The value produced by this instruction is a pointer into the stack
    // that looks as if it was a smi because of alignment.
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements, "arguments_elements")
};


class HArgumentsLength: public HUnaryOperation {
 public:
  explicit HArgumentsLength(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength, "arguments_length")
};


class HAccessArgumentsAt: public HInstruction {
 public:
  HAccessArgumentsAt(HValue* arguments, HValue* length, HValue* index) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetOperandAt(0, arguments);
    SetOperandAt(1, length);
    SetOperandAt(2, index);
  }

  virtual void PrintDataTo(StringStream* stream) const;

  virtual Representation RequiredInputRepresentation(int index) const {
    // The arguments elements is considered tagged.
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }

  HValue* arguments() const { return operands_[0]; }
  HValue* length() const { return operands_[1]; }
  HValue* index() const { return operands_[2]; }

  virtual int OperandCount() const { return operands_.length(); }
  virtual HValue* OperandAt(int index) const { return operands_[index]; }

  DECLARE_CONCRETE_INSTRUCTION(AccessArgumentsAt, "access_arguments_at")

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    operands_[index] = value;
  }

 private:
  HOperandVector<3> operands_;
};


class HBoundsCheck: public HBinaryOperation {
 public:
  HBoundsCheck(HValue* index, HValue* length)
      : HBinaryOperation(index, length) {
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Integer32();
  }

#ifdef DEBUG
  virtual void Verify() const;
#endif

  HValue* index() const { return left(); }
  HValue* length() const { return right(); }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheck, "bounds_check")
};


class HBitwiseBinaryOperation: public HBinaryOperation {
 public:
  HBitwiseBinaryOperation(HValue* left, HValue* right)
      : HBinaryOperation(left, right) {
    // Default to truncating, Integer32, UseGVN.
    set_representation(Representation::Integer32());
    SetFlag(kTruncatingToInt32);
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Integer32();
  }

  DECLARE_INSTRUCTION(BitwiseBinaryOperation)
};


class HArithmeticBinaryOperation: public HBinaryOperation {
 public:
  HArithmeticBinaryOperation(HValue* left, HValue* right)
      : HBinaryOperation(left, right) {
    set_representation(Representation::Tagged());
    SetFlag(kFlexibleRepresentation);
    SetFlagMask(AllSideEffects());
  }

  virtual void RepresentationChanged(Representation to) {
    if (!to.IsTagged()) {
      ClearFlagMask(AllSideEffects());
      SetFlag(kUseGVN);
    }
  }

  virtual HType CalculateInferredType() const;
  virtual Representation RequiredInputRepresentation(int index) const {
    return representation();
  }
  virtual Representation InferredRepresentation() const {
    if (left()->representation().Equals(right()->representation())) {
      return left()->representation();
    }
    return HValue::InferredRepresentation();
  }

  DECLARE_INSTRUCTION(ArithmeticBinaryOperation)
};


class HCompare: public HBinaryOperation {
 public:
  HCompare(HValue* left, HValue* right, Token::Value token)
      : HBinaryOperation(left, right), token_(token) {
    ASSERT(Token::IsCompareOp(token));
    set_representation(Representation::Tagged());
    SetFlagMask(AllSideEffects());
  }

  void SetInputRepresentation(Representation r);
  virtual bool EmitAtUses() const { return uses()->length() <= 1; }
  virtual Representation RequiredInputRepresentation(int index) const {
    return input_representation_;
  }
  Representation GetInputRepresentation() const {
    return input_representation_;
  }
  Token::Value token() const { return token_; }
  virtual void PrintDataTo(StringStream* stream) const;

  virtual HType CalculateInferredType() const;

  virtual intptr_t Hashcode() const {
    return HValue::Hashcode() * 7 + token_;
  }

  DECLARE_CONCRETE_INSTRUCTION(Compare, "compare")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HCompare* comp = HCompare::cast(other);
    return token_ == comp->token();
  }

 private:
  Representation input_representation_;
  Token::Value token_;
};


class HCompareJSObjectEq: public HBinaryOperation {
 public:
  HCompareJSObjectEq(HValue* left, HValue* right)
      : HBinaryOperation(left, right) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  virtual bool EmitAtUses() const { return uses()->length() <= 1; }
  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual HType CalculateInferredType() const;

  DECLARE_CONCRETE_INSTRUCTION(CompareJSObjectEq, "compare-js-object-eq")
};


class HUnaryPredicate: public HUnaryOperation {
 public:
  explicit HUnaryPredicate(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }
  virtual bool EmitAtUses() const { return uses()->length() <= 1; }
  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual HType CalculateInferredType() const;
};


class HIsNull: public HUnaryPredicate {
 public:
  HIsNull(HValue* value, bool is_strict)
      : HUnaryPredicate(value), is_strict_(is_strict) { }

  bool is_strict() const { return is_strict_; }

  DECLARE_CONCRETE_INSTRUCTION(IsNull, "is_null")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HIsNull* b = HIsNull::cast(other);
    return is_strict_ == b->is_strict();
  }

 private:
  bool is_strict_;
};


class HIsObject: public HUnaryPredicate {
 public:
  explicit HIsObject(HValue* value) : HUnaryPredicate(value) { }

  DECLARE_CONCRETE_INSTRUCTION(IsObject, "is_object")
};


class HIsSmi: public HUnaryPredicate {
 public:
  explicit HIsSmi(HValue* value) : HUnaryPredicate(value) { }

  DECLARE_CONCRETE_INSTRUCTION(IsSmi, "is_smi")
};


class HHasInstanceType: public HUnaryPredicate {
 public:
  HHasInstanceType(HValue* value, InstanceType type)
      : HUnaryPredicate(value), from_(type), to_(type) { }
  HHasInstanceType(HValue* value, InstanceType from, InstanceType to)
      : HUnaryPredicate(value), from_(from), to_(to) {
    ASSERT(to == LAST_TYPE);  // Others not implemented yet in backend.
  }

  InstanceType from() { return from_; }
  InstanceType to() { return to_; }

  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceType, "has_instance_type")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HHasInstanceType* b = HHasInstanceType::cast(other);
    return (from_ == b->from()) && (to_ == b->to());
  }

 private:
  InstanceType from_;
  InstanceType to_;  // Inclusive range, not all combinations work.
};


class HHasCachedArrayIndex: public HUnaryPredicate {
 public:
  explicit HHasCachedArrayIndex(HValue* value) : HUnaryPredicate(value) { }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndex, "has_cached_array_index")
};


class HClassOfTest: public HUnaryPredicate {
 public:
  HClassOfTest(HValue* value, Handle<String> class_name)
      : HUnaryPredicate(value), class_name_(class_name) { }

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTest, "class_of_test")

  virtual void PrintDataTo(StringStream* stream) const;

  Handle<String> class_name() const { return class_name_; }

 protected:
  virtual bool DataEquals(HValue* other) const {
    HClassOfTest* b = HClassOfTest::cast(other);
    return class_name_.is_identical_to(b->class_name_);
  }

 private:
  Handle<String> class_name_;
};


class HTypeofIs: public HUnaryPredicate {
 public:
  HTypeofIs(HValue* value, Handle<String> type_literal)
      : HUnaryPredicate(value), type_literal_(type_literal) { }

  Handle<String> type_literal() { return type_literal_; }
  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(TypeofIs, "typeof_is")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HTypeofIs* b = HTypeofIs::cast(other);
    return type_literal_.is_identical_to(b->type_literal_);
  }

 private:
  Handle<String> type_literal_;
};


class HInstanceOf: public HBinaryOperation {
 public:
  HInstanceOf(HValue* left, HValue* right) : HBinaryOperation(left, right) {
    set_representation(Representation::Tagged());
    SetFlagMask(AllSideEffects());
  }

  virtual bool EmitAtUses() const { return uses()->length() <= 1; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOf, "instance_of")
};


class HInstanceOfKnownGlobal: public HUnaryOperation {
 public:
  HInstanceOfKnownGlobal(HValue* left, Handle<JSFunction> right)
      : HUnaryOperation(left), function_(right) {
    set_representation(Representation::Tagged());
    SetFlagMask(AllSideEffects());
  }

  Handle<JSFunction> function() { return function_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfKnownGlobal,
                               "instance_of_known_global")

 private:
  Handle<JSFunction> function_;
};


class HPower: public HBinaryOperation {
 public:
  HPower(HValue* left, HValue* right)
      : HBinaryOperation(left, right) {
    set_representation(Representation::Double());
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return (index == 1) ? Representation::None() : Representation::Double();
  }

  DECLARE_CONCRETE_INSTRUCTION(Power, "power")
};


class HAdd: public HArithmeticBinaryOperation {
 public:
  HAdd(HValue* left, HValue* right) : HArithmeticBinaryOperation(left, right) {
    SetFlag(kCanOverflow);
  }

  // Add is only commutative if two integer values are added and not if two
  // tagged values are added (because it might be a String concatenation).
  virtual bool IsCommutative() const {
    return !representation().IsTagged();
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  virtual HType CalculateInferredType() const;

  DECLARE_CONCRETE_INSTRUCTION(Add, "add")

 protected:
  virtual Range* InferRange();
};


class HSub: public HArithmeticBinaryOperation {
 public:
  HSub(HValue* left, HValue* right) : HArithmeticBinaryOperation(left, right) {
    SetFlag(kCanOverflow);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  DECLARE_CONCRETE_INSTRUCTION(Sub, "sub")

 protected:
  virtual Range* InferRange();
};


class HMul: public HArithmeticBinaryOperation {
 public:
  HMul(HValue* left, HValue* right) : HArithmeticBinaryOperation(left, right) {
    SetFlag(kCanOverflow);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  // Only commutative if it is certain that not two objects are multiplicated.
  virtual bool IsCommutative() const {
    return !representation().IsTagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Mul, "mul")

 protected:
  virtual Range* InferRange();
};


class HMod: public HArithmeticBinaryOperation {
 public:
  HMod(HValue* left, HValue* right) : HArithmeticBinaryOperation(left, right) {
    SetFlag(kCanBeDivByZero);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  DECLARE_CONCRETE_INSTRUCTION(Mod, "mod")

 protected:
  virtual Range* InferRange();
};


class HDiv: public HArithmeticBinaryOperation {
 public:
  HDiv(HValue* left, HValue* right) : HArithmeticBinaryOperation(left, right) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
  }

  virtual HValue* EnsureAndPropagateNotMinusZero(BitVector* visited);

  DECLARE_CONCRETE_INSTRUCTION(Div, "div")

 protected:
  virtual Range* InferRange();
};


class HBitAnd: public HBitwiseBinaryOperation {
 public:
  HBitAnd(HValue* left, HValue* right)
      : HBitwiseBinaryOperation(left, right) { }

  virtual bool IsCommutative() const { return true; }
  virtual HType CalculateInferredType() const;

  DECLARE_CONCRETE_INSTRUCTION(BitAnd, "bit_and")

 protected:
  virtual Range* InferRange();
};


class HBitXor: public HBitwiseBinaryOperation {
 public:
  HBitXor(HValue* left, HValue* right)
      : HBitwiseBinaryOperation(left, right) { }

  virtual bool IsCommutative() const { return true; }
  virtual HType CalculateInferredType() const;

  DECLARE_CONCRETE_INSTRUCTION(BitXor, "bit_xor")
};


class HBitOr: public HBitwiseBinaryOperation {
 public:
  HBitOr(HValue* left, HValue* right)
      : HBitwiseBinaryOperation(left, right) { }

  virtual bool IsCommutative() const { return true; }
  virtual HType CalculateInferredType() const;

  DECLARE_CONCRETE_INSTRUCTION(BitOr, "bit_or")

 protected:
  virtual Range* InferRange();
};


class HShl: public HBitwiseBinaryOperation {
 public:
  HShl(HValue* left, HValue* right)
      : HBitwiseBinaryOperation(left, right) { }

  virtual Range* InferRange();
  virtual HType CalculateInferredType() const;

  DECLARE_CONCRETE_INSTRUCTION(Shl, "shl")
};


class HShr: public HBitwiseBinaryOperation {
 public:
  HShr(HValue* left, HValue* right)
      : HBitwiseBinaryOperation(left, right) { }

  virtual HType CalculateInferredType() const;

  DECLARE_CONCRETE_INSTRUCTION(Shr, "shr")
};


class HSar: public HBitwiseBinaryOperation {
 public:
  HSar(HValue* left, HValue* right)
      : HBitwiseBinaryOperation(left, right) { }

  virtual Range* InferRange();
  virtual HType CalculateInferredType() const;

  DECLARE_CONCRETE_INSTRUCTION(Sar, "sar")
};


class HOsrEntry: public HInstruction {
 public:
  explicit HOsrEntry(int ast_id) : ast_id_(ast_id) {
    SetFlag(kChangesOsrEntries);
  }

  int ast_id() const { return ast_id_; }

  DECLARE_CONCRETE_INSTRUCTION(OsrEntry, "osr_entry")

 private:
  int ast_id_;
};


class HParameter: public HInstruction {
 public:
  explicit HParameter(unsigned index) : index_(index) {
    set_representation(Representation::Tagged());
  }

  unsigned index() const { return index_; }

  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(Parameter, "parameter")

 private:
  unsigned index_;
};


class HCallStub: public HInstruction {
 public:
  HCallStub(CodeStub::Major major_key, int argument_count)
      : major_key_(major_key),
        argument_count_(argument_count),
        transcendental_type_(TranscendentalCache::kNumberOfCaches) {
    set_representation(Representation::Tagged());
    SetFlagMask(AllSideEffects());
  }

  CodeStub::Major major_key() { return major_key_; }
  int argument_count() { return argument_count_; }

  void set_transcendental_type(TranscendentalCache::Type transcendental_type) {
    transcendental_type_ = transcendental_type;
  }
  TranscendentalCache::Type transcendental_type() {
    return transcendental_type_;
  }
  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(CallStub, "call_stub")

 private:
  CodeStub::Major major_key_;
  int argument_count_;
  TranscendentalCache::Type transcendental_type_;
};


class HUnknownOSRValue: public HInstruction {
 public:
  HUnknownOSRValue() { set_representation(Representation::Tagged()); }

  DECLARE_CONCRETE_INSTRUCTION(UnknownOSRValue, "unknown_osr_value")
};


class HLoadGlobal: public HInstruction {
 public:
  HLoadGlobal(Handle<JSGlobalPropertyCell> cell, bool check_hole_value)
      : cell_(cell), check_hole_value_(check_hole_value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnGlobalVars);
  }

  Handle<JSGlobalPropertyCell>  cell() const { return cell_; }
  bool check_hole_value() const { return check_hole_value_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream) const;

  virtual intptr_t Hashcode() const {
    ASSERT(!Heap::allow_allocation(false));
    return reinterpret_cast<intptr_t>(*cell_);
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobal, "load_global")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HLoadGlobal* b = HLoadGlobal::cast(other);
    return cell_.is_identical_to(b->cell());
  }

 private:
  Handle<JSGlobalPropertyCell> cell_;
  bool check_hole_value_;
};


class HStoreGlobal: public HUnaryOperation {
 public:
  HStoreGlobal(HValue* value, Handle<JSGlobalPropertyCell> cell)
      : HUnaryOperation(value), cell_(cell) {
    SetFlag(kChangesGlobalVars);
  }

  Handle<JSGlobalPropertyCell> cell() const { return cell_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobal, "store_global")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HStoreGlobal* b = HStoreGlobal::cast(other);
    return cell_.is_identical_to(b->cell());
  }

 private:
  Handle<JSGlobalPropertyCell> cell_;
};


class HLoadContextSlot: public HInstruction {
 public:
  HLoadContextSlot(int context_chain_length , int slot_index)
      : context_chain_length_(context_chain_length), slot_index_(slot_index) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetFlag(kDependsOnCalls);
  }

  int context_chain_length() const { return context_chain_length_; }
  int slot_index() const { return slot_index_; }

  virtual void PrintDataTo(StringStream* stream) const;

  virtual intptr_t Hashcode() const {
    return context_chain_length() * 29 + slot_index();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadContextSlot, "load_context_slot")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HLoadContextSlot* b = HLoadContextSlot::cast(other);
    return (context_chain_length() == b->context_chain_length())
        && (slot_index() == b->slot_index());
  }

 private:
  int context_chain_length_;
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
    if (is_in_object) {
      SetFlag(kDependsOnInobjectFields);
    } else {
      SetFlag(kDependsOnBackingStoreFields);
    }
  }

  HValue* object() const { return OperandAt(0); }
  bool is_in_object() const { return is_in_object_; }
  int offset() const { return offset_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField, "load_named_field")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HLoadNamedField* b = HLoadNamedField::cast(other);
    return is_in_object_ == b->is_in_object_ && offset_ == b->offset_;
  }

 private:
  bool is_in_object_;
  int offset_;
};


class HLoadNamedGeneric: public HUnaryOperation {
 public:
  HLoadNamedGeneric(HValue* object, Handle<Object> name)
      : HUnaryOperation(object), name_(name) {
    set_representation(Representation::Tagged());
    SetFlagMask(AllSideEffects());
  }

  HValue* object() const { return OperandAt(0); }
  Handle<Object> name() const { return name_; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedGeneric, "load_named_generic")

 protected:
  virtual bool DataEquals(HValue* other) const {
    HLoadNamedGeneric* b = HLoadNamedGeneric::cast(other);
    return name_.is_identical_to(b->name_);
  }

 private:
  Handle<Object> name_;
};


class HLoadFunctionPrototype: public HUnaryOperation {
 public:
  explicit HLoadFunctionPrototype(HValue* function)
      : HUnaryOperation(function) {
    set_representation(Representation::Tagged());
    SetFlagMask(kDependsOnFunctionPrototypes);
  }

  HValue* function() const { return OperandAt(0); }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype, "load_function_prototype")

 protected:
  virtual bool DataEquals(HValue* other) const { return true; }
};


class HLoadKeyed: public HBinaryOperation {
 public:
  HLoadKeyed(HValue* obj, HValue* key) : HBinaryOperation(obj, key) {
    set_representation(Representation::Tagged());
  }

  virtual void PrintDataTo(StringStream* stream) const;

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }
  HValue* object() const { return OperandAt(0); }
  HValue* key() const { return OperandAt(1); }

  DECLARE_INSTRUCTION(LoadKeyed)
};


class HLoadKeyedFastElement: public HLoadKeyed {
 public:
  HLoadKeyedFastElement(HValue* obj, HValue* key) : HLoadKeyed(obj, key) {
    SetFlag(kDependsOnArrayElements);
    SetFlag(kUseGVN);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    // The key is supposed to be Integer32.
    return (index == 1) ? Representation::Integer32()
        : Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedFastElement,
                               "load_keyed_fast_element")
};


class HLoadKeyedGeneric: public HLoadKeyed {
 public:
  HLoadKeyedGeneric(HValue* obj, HValue* key) : HLoadKeyed(obj, key) {
    SetFlagMask(AllSideEffects());
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedGeneric, "load_keyed_generic")
};


static inline bool StoringValueNeedsWriteBarrier(HValue* value) {
  return !value->type().IsSmi() &&
      !(value->IsConstant() && HConstant::cast(value)->InOldSpace());
}


class HStoreNamed: public HBinaryOperation {
 public:
  HStoreNamed(HValue* obj, Handle<Object> name, HValue* val)
      : HBinaryOperation(obj, val), name_(name) {
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  virtual void PrintDataTo(StringStream* stream) const;

  HValue* object() const { return OperandAt(0); }
  Handle<Object> name() const { return name_; }
  HValue* value() const { return OperandAt(1); }
  void set_value(HValue* value) { SetOperandAt(1, value); }

  bool NeedsWriteBarrier() const {
    return StoringValueNeedsWriteBarrier(value());
  }

  DECLARE_INSTRUCTION(StoreNamed)

 protected:
  virtual bool DataEquals(HValue* other) const {
    HStoreNamed* b = HStoreNamed::cast(other);
    return name_.is_identical_to(b->name_);
  }

 private:
  Handle<Object> name_;
};


class HStoreNamedField: public HStoreNamed {
 public:
  HStoreNamedField(HValue* obj,
                   Handle<Object> name,
                   HValue* val,
                   bool in_object,
                   int offset)
      : HStoreNamed(obj, name, val),
        is_in_object_(in_object),
        offset_(offset) {
    if (is_in_object_) {
      SetFlag(kChangesInobjectFields);
    } else {
      SetFlag(kChangesBackingStoreFields);
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField, "store_named_field")

  virtual Representation RequiredInputRepresentation(int index) const {
    return  Representation::Tagged();
  }
  virtual void PrintDataTo(StringStream* stream) const;

  bool is_in_object() const { return is_in_object_; }
  int offset() const { return offset_; }
  Handle<Map> transition() const { return transition_; }
  void set_transition(Handle<Map> map) { transition_ = map; }

 private:
  bool is_in_object_;
  int offset_;
  Handle<Map> transition_;
};


class HStoreNamedGeneric: public HStoreNamed {
 public:
  HStoreNamedGeneric(HValue* obj, Handle<Object> name, HValue* val)
      : HStoreNamed(obj, name, val) {
    SetFlagMask(AllSideEffects());
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedGeneric, "store_named_generic")
};


class HStoreKeyed: public HInstruction {
 public:
  HStoreKeyed(HValue* obj, HValue* key, HValue* val) {
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, val);
  }

  virtual void PrintDataTo(StringStream* stream) const;
  virtual int OperandCount() const { return operands_.length(); }
  virtual HValue* OperandAt(int index) const { return operands_[index]; }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  HValue* object() const { return OperandAt(0); }
  HValue* key() const { return OperandAt(1); }
  HValue* value() const { return OperandAt(2); }

  bool NeedsWriteBarrier() const {
    return StoringValueNeedsWriteBarrier(value());
  }

  DECLARE_INSTRUCTION(StoreKeyed)

 protected:
  virtual void InternalSetOperandAt(int index, HValue* value) {
    operands_[index] = value;
  }

 private:
  HOperandVector<3> operands_;
};


class HStoreKeyedFastElement: public HStoreKeyed {
 public:
  HStoreKeyedFastElement(HValue* obj, HValue* key, HValue* val)
      : HStoreKeyed(obj, key, val) {
    SetFlag(kChangesArrayElements);
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    // The key is supposed to be Integer32.
    return (index == 1) ? Representation::Integer32()
        : Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedFastElement,
                               "store_keyed_fast_element")
};


class HStoreKeyedGeneric: public HStoreKeyed {
 public:
  HStoreKeyedGeneric(HValue* obj, HValue* key, HValue* val)
      : HStoreKeyed(obj, key, val) {
    SetFlagMask(AllSideEffects());
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedGeneric, "store_keyed_generic")
};


class HMaterializedLiteral: public HInstruction {
 public:
  HMaterializedLiteral(int index, int depth)
      : literal_index_(index), depth_(depth) {
    set_representation(Representation::Tagged());
  }

  int literal_index() const { return literal_index_; }
  int depth() const { return depth_; }

  DECLARE_INSTRUCTION(MaterializedLiteral)

 private:
  int literal_index_;
  int depth_;
};


class HArrayLiteral: public HMaterializedLiteral {
 public:
  HArrayLiteral(Handle<FixedArray> constant_elements,
                int length,
                int literal_index,
                int depth)
      : HMaterializedLiteral(literal_index, depth),
        length_(length),
        constant_elements_(constant_elements) {}

  Handle<FixedArray> constant_elements() const { return constant_elements_; }
  int length() const { return length_; }

  bool IsCopyOnWrite() const;

  DECLARE_CONCRETE_INSTRUCTION(ArrayLiteral, "array_literal")

 private:
  int length_;
  Handle<FixedArray> constant_elements_;
};


class HObjectLiteral: public HMaterializedLiteral {
 public:
  HObjectLiteral(Handle<FixedArray> constant_properties,
                 bool fast_elements,
                 int literal_index,
                 int depth)
      : HMaterializedLiteral(literal_index, depth),
        constant_properties_(constant_properties),
        fast_elements_(fast_elements) {}

  Handle<FixedArray> constant_properties() const {
    return constant_properties_;
  }
  bool fast_elements() const { return fast_elements_; }

  DECLARE_CONCRETE_INSTRUCTION(ObjectLiteral, "object_literal")

 private:
  Handle<FixedArray> constant_properties_;
  bool fast_elements_;
};


class HRegExpLiteral: public HMaterializedLiteral {
 public:
  HRegExpLiteral(Handle<String> pattern,
                 Handle<String> flags,
                 int literal_index)
      : HMaterializedLiteral(literal_index, 0),
        pattern_(pattern),
        flags_(flags) { }

  Handle<String> pattern() { return pattern_; }
  Handle<String> flags() { return flags_; }

  DECLARE_CONCRETE_INSTRUCTION(RegExpLiteral, "regexp_literal")

 private:
  Handle<String> pattern_;
  Handle<String> flags_;
};


class HFunctionLiteral: public HInstruction {
 public:
  HFunctionLiteral(Handle<SharedFunctionInfo> shared, bool pretenure)
      : shared_info_(shared), pretenure_(pretenure) {
    set_representation(Representation::Tagged());
  }

  DECLARE_CONCRETE_INSTRUCTION(FunctionLiteral, "function_literal")

  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  bool pretenure() const { return pretenure_; }

 private:
  Handle<SharedFunctionInfo> shared_info_;
  bool pretenure_;
};


class HTypeof: public HUnaryOperation {
 public:
  explicit HTypeof(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
  }

  DECLARE_CONCRETE_INSTRUCTION(Typeof, "typeof")
};


class HValueOf: public HUnaryOperation {
 public:
  explicit HValueOf(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
  }

  DECLARE_CONCRETE_INSTRUCTION(ValueOf, "value_of")
};


class HDeleteProperty: public HBinaryOperation {
 public:
  HDeleteProperty(HValue* obj, HValue* key)
      : HBinaryOperation(obj, key) {
    set_representation(Representation::Tagged());
    SetFlagMask(AllSideEffects());
  }

  virtual Representation RequiredInputRepresentation(int index) const {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(DeleteProperty, "delete_property")

  HValue* object() const { return left(); }
  HValue* key() const { return right(); }
};

#undef DECLARE_INSTRUCTION
#undef DECLARE_CONCRETE_INSTRUCTION

} }  // namespace v8::internal

#endif  // V8_HYDROGEN_INSTRUCTIONS_H_
