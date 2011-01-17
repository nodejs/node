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

#ifndef V8_ARM_LITHIUM_ARM_H_
#define V8_ARM_LITHIUM_ARM_H_

#include "hydrogen.h"
#include "lithium-allocator.h"
#include "lithium.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LCodeGen;


// Type hierarchy:
//
// LInstruction
//   LAccessArgumentsAt
//   LArgumentsElements
//   LArgumentsLength
//   LBinaryOperation
//     LAddI
//     LApplyArguments
//     LArithmeticD
//     LArithmeticT
//     LBitI
//     LBoundsCheck
//     LCmpID
//     LCmpIDAndBranch
//     LCmpJSObjectEq
//     LCmpJSObjectEqAndBranch
//     LCmpT
//     LDivI
//     LInstanceOf
//     LInstanceOfAndBranch
//     LInstanceOfKnownGlobal
//     LLoadKeyedFastElement
//     LLoadKeyedGeneric
//     LModI
//     LMulI
//     LShiftI
//     LSubI
//   LCallConstantFunction
//   LCallFunction
//   LCallGlobal
//   LCallKeyed
//   LCallKnownGlobal
//   LCallNamed
//   LCallRuntime
//   LCallStub
//   LCheckPrototypeMaps
//   LConstant
//     LConstantD
//     LConstantI
//     LConstantT
//   LDeoptimize
//   LFunctionLiteral
//   LGlobalObject
//   LGlobalReceiver
//   LLabel
//   LLazyBailout
//   LLoadContextSlot
//   LLoadGlobal
//   LMaterializedLiteral
//     LArrayLiteral
//     LObjectLiteral
//     LRegExpLiteral
//   LOsrEntry
//   LParameter
//   LStackCheck
//   LStoreKeyed
//     LStoreKeyedFastElement
//     LStoreKeyedGeneric
//   LStoreNamed
//     LStoreNamedField
//     LStoreNamedGeneric
//   LUnaryOperation
//     LJSArrayLength
//     LFixedArrayLength
//     LBitNotI
//     LBranch
//     LCallNew
//     LCheckFunction
//     LCheckInstanceType
//     LCheckMap
//     LCheckSmi
//     LClassOfTest
//     LClassOfTestAndBranch
//     LDeleteProperty
//     LDoubleToI
//     LHasCachedArrayIndex
//     LHasCachedArrayIndexAndBranch
//     LHasInstanceType
//     LHasInstanceTypeAndBranch
//     LInteger32ToDouble
//     LIsNull
//     LIsNullAndBranch
//     LIsObject
//     LIsObjectAndBranch
//     LIsSmi
//     LIsSmiAndBranch
//     LLoadNamedField
//     LLoadNamedGeneric
//     LLoadFunctionPrototype
//     LNumberTagD
//     LNumberTagI
//     LPushArgument
//     LReturn
//     LSmiTag
//     LStoreGlobal
//     LTaggedToI
//     LThrow
//     LTypeof
//     LTypeofIs
//     LTypeofIsAndBranch
//     LUnaryMathOperation
//     LValueOf
//   LUnknownOSRValue

#define LITHIUM_ALL_INSTRUCTION_LIST(V)         \
  V(BinaryOperation)                            \
  V(Constant)                                   \
  V(Call)                                       \
  V(MaterializedLiteral)                        \
  V(StoreKeyed)                                 \
  V(StoreNamed)                                 \
  V(UnaryOperation)                             \
  LITHIUM_CONCRETE_INSTRUCTION_LIST(V)


#define LITHIUM_CONCRETE_INSTRUCTION_LIST(V)    \
  V(AccessArgumentsAt)                          \
  V(AddI)                                       \
  V(ApplyArguments)                             \
  V(ArgumentsElements)                          \
  V(ArgumentsLength)                            \
  V(ArithmeticD)                                \
  V(ArithmeticT)                                \
  V(ArrayLiteral)                               \
  V(BitI)                                       \
  V(BitNotI)                                    \
  V(BoundsCheck)                                \
  V(Branch)                                     \
  V(CallConstantFunction)                       \
  V(CallFunction)                               \
  V(CallGlobal)                                 \
  V(CallKeyed)                                  \
  V(CallKnownGlobal)                            \
  V(CallNamed)                                  \
  V(CallNew)                                    \
  V(CallRuntime)                                \
  V(CallStub)                                   \
  V(CheckFunction)                              \
  V(CheckInstanceType)                          \
  V(CheckMap)                                   \
  V(CheckPrototypeMaps)                         \
  V(CheckSmi)                                   \
  V(CmpID)                                      \
  V(CmpIDAndBranch)                             \
  V(CmpJSObjectEq)                              \
  V(CmpJSObjectEqAndBranch)                     \
  V(CmpMapAndBranch)                            \
  V(CmpT)                                       \
  V(CmpTAndBranch)                              \
  V(ConstantD)                                  \
  V(ConstantI)                                  \
  V(ConstantT)                                  \
  V(DeleteProperty)                             \
  V(Deoptimize)                                 \
  V(DivI)                                       \
  V(DoubleToI)                                  \
  V(FixedArrayLength)                           \
  V(FunctionLiteral)                            \
  V(Gap)                                        \
  V(GlobalObject)                               \
  V(GlobalReceiver)                             \
  V(Goto)                                       \
  V(InstanceOf)                                 \
  V(InstanceOfAndBranch)                        \
  V(InstanceOfKnownGlobal)                      \
  V(Integer32ToDouble)                          \
  V(IsNull)                                     \
  V(IsNullAndBranch)                            \
  V(IsObject)                                   \
  V(IsObjectAndBranch)                          \
  V(IsSmi)                                      \
  V(IsSmiAndBranch)                             \
  V(JSArrayLength)                              \
  V(HasInstanceType)                            \
  V(HasInstanceTypeAndBranch)                   \
  V(HasCachedArrayIndex)                        \
  V(HasCachedArrayIndexAndBranch)               \
  V(ClassOfTest)                                \
  V(ClassOfTestAndBranch)                       \
  V(Label)                                      \
  V(LazyBailout)                                \
  V(LoadContextSlot)                            \
  V(LoadElements)                               \
  V(LoadGlobal)                                 \
  V(LoadKeyedFastElement)                       \
  V(LoadKeyedGeneric)                           \
  V(LoadNamedField)                             \
  V(LoadNamedGeneric)                           \
  V(LoadFunctionPrototype)                      \
  V(ModI)                                       \
  V(MulI)                                       \
  V(NumberTagD)                                 \
  V(NumberTagI)                                 \
  V(NumberUntagD)                               \
  V(ObjectLiteral)                              \
  V(OsrEntry)                                   \
  V(Parameter)                                  \
  V(PushArgument)                               \
  V(RegExpLiteral)                              \
  V(Return)                                     \
  V(ShiftI)                                     \
  V(SmiTag)                                     \
  V(SmiUntag)                                   \
  V(StackCheck)                                 \
  V(StoreGlobal)                                \
  V(StoreKeyedFastElement)                      \
  V(StoreKeyedGeneric)                          \
  V(StoreNamedField)                            \
  V(StoreNamedGeneric)                          \
  V(SubI)                                       \
  V(TaggedToI)                                  \
  V(Throw)                                      \
  V(Typeof)                                     \
  V(TypeofIs)                                   \
  V(TypeofIsAndBranch)                          \
  V(UnaryMathOperation)                         \
  V(UnknownOSRValue)                            \
  V(ValueOf)


#define DECLARE_INSTRUCTION(type)                \
  virtual bool Is##type() const { return true; } \
  static L##type* cast(LInstruction* instr) {    \
    ASSERT(instr->Is##type());                   \
    return reinterpret_cast<L##type*>(instr);    \
  }


#define DECLARE_CONCRETE_INSTRUCTION(type, mnemonic)        \
  virtual void CompileToNative(LCodeGen* generator);        \
  virtual const char* Mnemonic() const { return mnemonic; } \
  DECLARE_INSTRUCTION(type)


#define DECLARE_HYDROGEN_ACCESSOR(type)     \
  H##type* hydrogen() const {               \
    return H##type::cast(hydrogen_value()); \
  }


class LInstruction: public ZoneObject {
 public:
  LInstruction()
      : hydrogen_value_(NULL) { }
  virtual ~LInstruction() { }

  virtual void CompileToNative(LCodeGen* generator) = 0;
  virtual const char* Mnemonic() const = 0;
  virtual void PrintTo(StringStream* stream) const;
  virtual void PrintDataTo(StringStream* stream) const { }

  // Declare virtual type testers.
#define DECLARE_DO(type) virtual bool Is##type() const { return false; }
  LITHIUM_ALL_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO
  virtual bool IsControl() const { return false; }

  void set_environment(LEnvironment* env) { environment_.set(env); }
  LEnvironment* environment() const { return environment_.get(); }
  bool HasEnvironment() const { return environment_.is_set(); }

  void set_pointer_map(LPointerMap* p) { pointer_map_.set(p); }
  LPointerMap* pointer_map() const { return pointer_map_.get(); }
  bool HasPointerMap() const { return pointer_map_.is_set(); }

  void set_result(LOperand* operand) { result_.set(operand); }
  LOperand* result() const { return result_.get(); }
  bool HasResult() const { return result_.is_set(); }

  void set_hydrogen_value(HValue* value) { hydrogen_value_ = value; }
  HValue* hydrogen_value() const { return hydrogen_value_; }

  void set_deoptimization_environment(LEnvironment* env) {
    deoptimization_environment_.set(env);
  }
  LEnvironment* deoptimization_environment() const {
    return deoptimization_environment_.get();
  }
  bool HasDeoptimizationEnvironment() const {
    return deoptimization_environment_.is_set();
  }

 private:
  SetOncePointer<LEnvironment> environment_;
  SetOncePointer<LPointerMap> pointer_map_;
  SetOncePointer<LOperand> result_;
  HValue* hydrogen_value_;
  SetOncePointer<LEnvironment> deoptimization_environment_;
};


class LGap: public LInstruction {
 public:
  explicit LGap(HBasicBlock* block)
      : block_(block) {
    parallel_moves_[BEFORE] = NULL;
    parallel_moves_[START] = NULL;
    parallel_moves_[END] = NULL;
    parallel_moves_[AFTER] = NULL;
  }

  DECLARE_CONCRETE_INSTRUCTION(Gap, "gap")
  virtual void PrintDataTo(StringStream* stream) const;

  bool IsRedundant() const;

  HBasicBlock* block() const { return block_; }

  enum InnerPosition {
    BEFORE,
    START,
    END,
    AFTER,
    FIRST_INNER_POSITION = BEFORE,
    LAST_INNER_POSITION = AFTER
  };

  LParallelMove* GetOrCreateParallelMove(InnerPosition pos)  {
    if (parallel_moves_[pos] == NULL) parallel_moves_[pos] = new LParallelMove;
    return parallel_moves_[pos];
  }

  LParallelMove* GetParallelMove(InnerPosition pos)  {
    return parallel_moves_[pos];
  }

 private:
  LParallelMove* parallel_moves_[LAST_INNER_POSITION + 1];
  HBasicBlock* block_;
};


class LGoto: public LInstruction {
 public:
  LGoto(int block_id, bool include_stack_check = false)
    : block_id_(block_id), include_stack_check_(include_stack_check) { }

  DECLARE_CONCRETE_INSTRUCTION(Goto, "goto")
  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int block_id() const { return block_id_; }
  bool include_stack_check() const { return include_stack_check_; }

 private:
  int block_id_;
  bool include_stack_check_;
};


class LLazyBailout: public LInstruction {
 public:
  LLazyBailout() : gap_instructions_size_(0) { }

  DECLARE_CONCRETE_INSTRUCTION(LazyBailout, "lazy-bailout")

  void set_gap_instructions_size(int gap_instructions_size) {
    gap_instructions_size_ = gap_instructions_size;
  }
  int gap_instructions_size() { return gap_instructions_size_; }

 private:
  int gap_instructions_size_;
};


class LDeoptimize: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Deoptimize, "deoptimize")
};


class LLabel: public LGap {
 public:
  explicit LLabel(HBasicBlock* block)
      : LGap(block), replacement_(NULL) { }

  DECLARE_CONCRETE_INSTRUCTION(Label, "label")

  virtual void PrintDataTo(StringStream* stream) const;

  int block_id() const { return block()->block_id(); }
  bool is_loop_header() const { return block()->IsLoopHeader(); }
  Label* label() { return &label_; }
  LLabel* replacement() const { return replacement_; }
  void set_replacement(LLabel* label) { replacement_ = label; }
  bool HasReplacement() const { return replacement_ != NULL; }

 private:
  Label label_;
  LLabel* replacement_;
};


class LParameter: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Parameter, "parameter")
};


class LCallStub: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallStub, "call-stub")
  DECLARE_HYDROGEN_ACCESSOR(CallStub)

  TranscendentalCache::Type transcendental_type() {
    return hydrogen()->transcendental_type();
  }
};


class LUnknownOSRValue: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(UnknownOSRValue, "unknown-osr-value")
};


class LUnaryOperation: public LInstruction {
 public:
  explicit LUnaryOperation(LOperand* input) : input_(input) { }

  DECLARE_INSTRUCTION(UnaryOperation)

  LOperand* input() const { return input_; }

  virtual void PrintDataTo(StringStream* stream) const;

 private:
  LOperand* input_;
};


class LBinaryOperation: public LInstruction {
 public:
  LBinaryOperation(LOperand* left, LOperand* right)
      : left_(left), right_(right) { }

  DECLARE_INSTRUCTION(BinaryOperation)

  LOperand* left() const { return left_; }
  LOperand* right() const { return right_; }
  virtual void PrintDataTo(StringStream* stream) const;

 private:
  LOperand* left_;
  LOperand* right_;
};


class LApplyArguments: public LBinaryOperation {
 public:
  LApplyArguments(LOperand* function,
                  LOperand* receiver,
                  LOperand* length,
                  LOperand* elements)
      : LBinaryOperation(function, receiver),
        length_(length),
        elements_(elements) { }

  DECLARE_CONCRETE_INSTRUCTION(ApplyArguments, "apply-arguments")

  LOperand* function() const { return left(); }
  LOperand* receiver() const { return right(); }
  LOperand* length() const { return length_; }
  LOperand* elements() const { return elements_; }

 private:
  LOperand* length_;
  LOperand* elements_;
};


class LAccessArgumentsAt: public LInstruction {
 public:
  LAccessArgumentsAt(LOperand* arguments, LOperand* length, LOperand* index)
      : arguments_(arguments), length_(length), index_(index) { }

  DECLARE_CONCRETE_INSTRUCTION(AccessArgumentsAt, "access-arguments-at")

  LOperand* arguments() const { return arguments_; }
  LOperand* length() const { return length_; }
  LOperand* index() const { return index_; }

  virtual void PrintDataTo(StringStream* stream) const;

 private:
  LOperand* arguments_;
  LOperand* length_;
  LOperand* index_;
};


class LArgumentsLength: public LUnaryOperation {
 public:
  explicit LArgumentsLength(LOperand* elements) : LUnaryOperation(elements) {}

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength, "arguments-length")
};


class LArgumentsElements: public LInstruction {
 public:
  LArgumentsElements() { }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements, "arguments-elements")
};


class LModI: public LBinaryOperation {
 public:
  LModI(LOperand* left, LOperand* right) : LBinaryOperation(left, right) { }

  DECLARE_CONCRETE_INSTRUCTION(ModI, "mod-i")
  DECLARE_HYDROGEN_ACCESSOR(Mod)
};


class LDivI: public LBinaryOperation {
 public:
  LDivI(LOperand* left, LOperand* right)
      : LBinaryOperation(left, right) { }

  DECLARE_CONCRETE_INSTRUCTION(DivI, "div-i")
  DECLARE_HYDROGEN_ACCESSOR(Div)
};


class LMulI: public LBinaryOperation {
 public:
  LMulI(LOperand* left, LOperand* right, LOperand* temp)
      : LBinaryOperation(left, right), temp_(temp) { }

  DECLARE_CONCRETE_INSTRUCTION(MulI, "mul-i")
  DECLARE_HYDROGEN_ACCESSOR(Mul)

  LOperand* temp() const { return temp_; }

 private:
  LOperand* temp_;
};


class LCmpID: public LBinaryOperation {
 public:
  LCmpID(LOperand* left, LOperand* right)
      : LBinaryOperation(left, right) { }

  Token::Value op() const { return hydrogen()->token(); }
  bool is_double() const {
    return hydrogen()->GetInputRepresentation().IsDouble();
  }

  DECLARE_CONCRETE_INSTRUCTION(CmpID, "cmp-id")
  DECLARE_HYDROGEN_ACCESSOR(Compare)
};


class LCmpIDAndBranch: public LCmpID {
 public:
  LCmpIDAndBranch(LOperand* left,
                  LOperand* right,
                  int true_block_id,
                  int false_block_id)
      : LCmpID(left, right),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(CmpIDAndBranch, "cmp-id-and-branch")
  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LUnaryMathOperation: public LUnaryOperation {
 public:
  explicit LUnaryMathOperation(LOperand* value, LOperand* temp)
      : LUnaryOperation(value), temp_(temp) { }

  DECLARE_CONCRETE_INSTRUCTION(UnaryMathOperation, "unary-math-operation")
  DECLARE_HYDROGEN_ACCESSOR(UnaryMathOperation)

  virtual void PrintDataTo(StringStream* stream) const;
  BuiltinFunctionId op() const { return hydrogen()->op(); }
  LOperand* temp() const { return temp_; }

 private:
  LOperand* temp_;
};


class LCmpJSObjectEq: public LBinaryOperation {
 public:
  LCmpJSObjectEq(LOperand* left, LOperand* right)
      : LBinaryOperation(left, right) {}

  DECLARE_CONCRETE_INSTRUCTION(CmpJSObjectEq, "cmp-jsobject-eq")
};


class LCmpJSObjectEqAndBranch: public LCmpJSObjectEq {
 public:
  LCmpJSObjectEqAndBranch(LOperand* left,
                          LOperand* right,
                          int true_block_id,
                          int false_block_id)
      : LCmpJSObjectEq(left, right),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(CmpJSObjectEqAndBranch,
                               "cmp-jsobject-eq-and-branch")

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LIsNull: public LUnaryOperation {
 public:
  explicit LIsNull(LOperand* value) : LUnaryOperation(value) {}

  DECLARE_CONCRETE_INSTRUCTION(IsNull, "is-null")
  DECLARE_HYDROGEN_ACCESSOR(IsNull);

  bool is_strict() const { return hydrogen()->is_strict(); }
};


class LIsNullAndBranch: public LIsNull {
 public:
  LIsNullAndBranch(LOperand* value,
                   int true_block_id,
                   int false_block_id)
      : LIsNull(value),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(IsNullAndBranch, "is-null-and-branch")
  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LIsObject: public LUnaryOperation {
 public:
  LIsObject(LOperand* value, LOperand* temp)
      : LUnaryOperation(value), temp_(temp) {}

  DECLARE_CONCRETE_INSTRUCTION(IsObject, "is-object")

  LOperand* temp() const { return temp_; }

 private:
  LOperand* temp_;
};


class LIsObjectAndBranch: public LIsObject {
 public:
  LIsObjectAndBranch(LOperand* value,
                     LOperand* temp,
                     LOperand* temp2,
                     int true_block_id,
                     int false_block_id)
      : LIsObject(value, temp),
        temp2_(temp2),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(IsObjectAndBranch, "is-object-and-branch")
  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

  LOperand* temp2() const { return temp2_; }

 private:
  LOperand* temp2_;
  int true_block_id_;
  int false_block_id_;
};


class LIsSmi: public LUnaryOperation {
 public:
  explicit LIsSmi(LOperand* value) : LUnaryOperation(value) {}

  DECLARE_CONCRETE_INSTRUCTION(IsSmi, "is-smi")
  DECLARE_HYDROGEN_ACCESSOR(IsSmi)
};


class LIsSmiAndBranch: public LIsSmi {
 public:
  LIsSmiAndBranch(LOperand* value,
                  int true_block_id,
                  int false_block_id)
      : LIsSmi(value),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch, "is-smi-and-branch")
  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LHasInstanceType: public LUnaryOperation {
 public:
  explicit LHasInstanceType(LOperand* value)
      : LUnaryOperation(value) { }

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceType, "has-instance-type")
  DECLARE_HYDROGEN_ACCESSOR(HasInstanceType)

  InstanceType TestType();  // The type to test against when generating code.
  Condition BranchCondition();  // The branch condition for 'true'.
};


class LHasInstanceTypeAndBranch: public LHasInstanceType {
 public:
  LHasInstanceTypeAndBranch(LOperand* value,
                            int true_block_id,
                            int false_block_id)
      : LHasInstanceType(value),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceTypeAndBranch,
                               "has-instance-type-and-branch")
  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LHasCachedArrayIndex: public LUnaryOperation {
 public:
  explicit LHasCachedArrayIndex(LOperand* value) : LUnaryOperation(value) {}

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndex, "has-cached-array-index")
  DECLARE_HYDROGEN_ACCESSOR(HasCachedArrayIndex)
};


class LHasCachedArrayIndexAndBranch: public LHasCachedArrayIndex {
 public:
  LHasCachedArrayIndexAndBranch(LOperand* value,
                                int true_block_id,
                                int false_block_id)
      : LHasCachedArrayIndex(value),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndexAndBranch,
                               "has-cached-array-index-and-branch")
  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LClassOfTest: public LUnaryOperation {
 public:
  explicit LClassOfTest(LOperand* value) : LUnaryOperation(value) {}

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTest, "class-of-test")
  DECLARE_HYDROGEN_ACCESSOR(ClassOfTest)

  virtual void PrintDataTo(StringStream* stream) const;
};


class LClassOfTestAndBranch: public LClassOfTest {
 public:
  LClassOfTestAndBranch(LOperand* value,
                        LOperand* temporary,
                        int true_block_id,
                        int false_block_id)
      : LClassOfTest(value),
        temporary_(temporary),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch,
                               "class-of-test-and-branch")
  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }
  LOperand* temporary() { return temporary_; }

 private:
  LOperand* temporary_;
  int true_block_id_;
  int false_block_id_;
};


class LCmpT: public LBinaryOperation {
 public:
  LCmpT(LOperand* left, LOperand* right) : LBinaryOperation(left, right) {}

  DECLARE_CONCRETE_INSTRUCTION(CmpT, "cmp-t")
  DECLARE_HYDROGEN_ACCESSOR(Compare)

  Token::Value op() const { return hydrogen()->token(); }
};


class LCmpTAndBranch: public LCmpT {
 public:
  LCmpTAndBranch(LOperand* left,
                 LOperand* right,
                 int true_block_id,
                 int false_block_id)
      : LCmpT(left, right),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(CmpTAndBranch, "cmp-t-and-branch")

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LInstanceOf: public LBinaryOperation {
 public:
  LInstanceOf(LOperand* left, LOperand* right)
      : LBinaryOperation(left, right) { }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOf, "instance-of")
};


class LInstanceOfAndBranch: public LInstanceOf {
 public:
  LInstanceOfAndBranch(LOperand* left,
                       LOperand* right,
                       int true_block_id,
                       int false_block_id)
      : LInstanceOf(left, right),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfAndBranch, "instance-of-and-branch")

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LInstanceOfKnownGlobal: public LUnaryOperation {
 public:
  explicit LInstanceOfKnownGlobal(LOperand* left)
      : LUnaryOperation(left) { }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfKnownGlobal,
                               "instance-of-known-global")
  DECLARE_HYDROGEN_ACCESSOR(InstanceOfKnownGlobal)

  Handle<JSFunction> function() const { return hydrogen()->function(); }
};


class LBoundsCheck: public LBinaryOperation {
 public:
  LBoundsCheck(LOperand* index, LOperand* length)
      : LBinaryOperation(index, length) { }

  LOperand* index() const { return left(); }
  LOperand* length() const { return right(); }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheck, "bounds-check")
};


class LBitI: public LBinaryOperation {
 public:
  LBitI(Token::Value op, LOperand* left, LOperand* right)
      : LBinaryOperation(left, right), op_(op) { }

  Token::Value op() const { return op_; }

  DECLARE_CONCRETE_INSTRUCTION(BitI, "bit-i")

 private:
  Token::Value op_;
};


class LShiftI: public LBinaryOperation {
 public:
  LShiftI(Token::Value op, LOperand* left, LOperand* right, bool can_deopt)
      : LBinaryOperation(left, right), op_(op), can_deopt_(can_deopt) { }

  Token::Value op() const { return op_; }

  bool can_deopt() const { return can_deopt_; }

  DECLARE_CONCRETE_INSTRUCTION(ShiftI, "shift-i")

 private:
  Token::Value op_;
  bool can_deopt_;
};


class LSubI: public LBinaryOperation {
 public:
  LSubI(LOperand* left, LOperand* right)
      : LBinaryOperation(left, right) { }

  DECLARE_CONCRETE_INSTRUCTION(SubI, "sub-i")
  DECLARE_HYDROGEN_ACCESSOR(Sub)
};


class LConstant: public LInstruction {
  DECLARE_INSTRUCTION(Constant)
};


class LConstantI: public LConstant {
 public:
  explicit LConstantI(int32_t value) : value_(value) { }
  int32_t value() const { return value_; }

  DECLARE_CONCRETE_INSTRUCTION(ConstantI, "constant-i")

 private:
  int32_t value_;
};


class LConstantD: public LConstant {
 public:
  explicit LConstantD(double value) : value_(value) { }
  double value() const { return value_; }

  DECLARE_CONCRETE_INSTRUCTION(ConstantD, "constant-d")

 private:
  double value_;
};


class LConstantT: public LConstant {
 public:
  explicit LConstantT(Handle<Object> value) : value_(value) { }
  Handle<Object> value() const { return value_; }

  DECLARE_CONCRETE_INSTRUCTION(ConstantT, "constant-t")

 private:
  Handle<Object> value_;
};


class LBranch: public LUnaryOperation {
 public:
  LBranch(LOperand* input, int true_block_id, int false_block_id)
      : LUnaryOperation(input),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(Branch, "branch")
  DECLARE_HYDROGEN_ACCESSOR(Value)

  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LCmpMapAndBranch: public LUnaryOperation {
 public:
  LCmpMapAndBranch(LOperand* value, LOperand* temp)
      : LUnaryOperation(value), temp_(temp) { }

  DECLARE_CONCRETE_INSTRUCTION(CmpMapAndBranch, "cmp-map-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(CompareMapAndBranch)

  virtual bool IsControl() const { return true; }

  LOperand* temp() const { return temp_; }
  Handle<Map> map() const { return hydrogen()->map(); }
  int true_block_id() const {
    return hydrogen()->true_destination()->block_id();
  }
  int false_block_id() const {
    return hydrogen()->false_destination()->block_id();
  }

 private:
  LOperand* temp_;
};


class LJSArrayLength: public LUnaryOperation {
 public:
  explicit LJSArrayLength(LOperand* input) : LUnaryOperation(input) { }

  DECLARE_CONCRETE_INSTRUCTION(JSArrayLength, "js-array-length")
  DECLARE_HYDROGEN_ACCESSOR(JSArrayLength)
};


class LFixedArrayLength: public LUnaryOperation {
 public:
  explicit LFixedArrayLength(LOperand* input) : LUnaryOperation(input) { }

  DECLARE_CONCRETE_INSTRUCTION(FixedArrayLength, "fixed-array-length")
  DECLARE_HYDROGEN_ACCESSOR(FixedArrayLength)
};


class LValueOf: public LUnaryOperation {
 public:
  LValueOf(LOperand* input, LOperand* temporary)
      : LUnaryOperation(input), temporary_(temporary) { }

  LOperand* temporary() const { return temporary_; }

  DECLARE_CONCRETE_INSTRUCTION(ValueOf, "value-of")
  DECLARE_HYDROGEN_ACCESSOR(ValueOf)

 private:
  LOperand* temporary_;
};


class LThrow: public LUnaryOperation {
 public:
  explicit LThrow(LOperand* value) : LUnaryOperation(value) { }

  DECLARE_CONCRETE_INSTRUCTION(Throw, "throw")
};


class LBitNotI: public LUnaryOperation {
 public:
  explicit LBitNotI(LOperand* use) : LUnaryOperation(use) { }

  DECLARE_CONCRETE_INSTRUCTION(BitNotI, "bit-not-i")
};


class LAddI: public LBinaryOperation {
 public:
  LAddI(LOperand* left, LOperand* right)
      : LBinaryOperation(left, right) { }

  DECLARE_CONCRETE_INSTRUCTION(AddI, "add-i")
  DECLARE_HYDROGEN_ACCESSOR(Add)
};


class LArithmeticD: public LBinaryOperation {
 public:
  LArithmeticD(Token::Value op, LOperand* left, LOperand* right)
      : LBinaryOperation(left, right), op_(op) { }

  Token::Value op() const { return op_; }

  virtual void CompileToNative(LCodeGen* generator);
  virtual const char* Mnemonic() const;

 private:
  Token::Value op_;
};


class LArithmeticT: public LBinaryOperation {
 public:
  LArithmeticT(Token::Value op, LOperand* left, LOperand* right)
      : LBinaryOperation(left, right), op_(op) { }

  virtual void CompileToNative(LCodeGen* generator);
  virtual const char* Mnemonic() const;

  Token::Value op() const { return op_; }

 private:
  Token::Value op_;
};


class LReturn: public LUnaryOperation {
 public:
  explicit LReturn(LOperand* use) : LUnaryOperation(use) { }

  DECLARE_CONCRETE_INSTRUCTION(Return, "return")
};


class LLoadNamedField: public LUnaryOperation {
 public:
  explicit LLoadNamedField(LOperand* object) : LUnaryOperation(object) { }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField, "load-named-field")
  DECLARE_HYDROGEN_ACCESSOR(LoadNamedField)
};


class LLoadNamedGeneric: public LUnaryOperation {
 public:
  explicit LLoadNamedGeneric(LOperand* object) : LUnaryOperation(object) { }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedGeneric, "load-named-generic")
  DECLARE_HYDROGEN_ACCESSOR(LoadNamedGeneric)

  LOperand* object() const { return input(); }
  Handle<Object> name() const { return hydrogen()->name(); }
};


class LLoadFunctionPrototype: public LUnaryOperation {
 public:
  explicit LLoadFunctionPrototype(LOperand* function)
      : LUnaryOperation(function) { }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype, "load-function-prototype")
  DECLARE_HYDROGEN_ACCESSOR(LoadFunctionPrototype)

  LOperand* function() const { return input(); }
};


class LLoadElements: public LUnaryOperation {
 public:
  explicit LLoadElements(LOperand* obj) : LUnaryOperation(obj) { }

  DECLARE_CONCRETE_INSTRUCTION(LoadElements, "load-elements")
};


class LLoadKeyedFastElement: public LBinaryOperation {
 public:
  LLoadKeyedFastElement(LOperand* elements, LOperand* key)
      : LBinaryOperation(elements, key) { }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedFastElement, "load-keyed-fast-element")
  DECLARE_HYDROGEN_ACCESSOR(LoadKeyedFastElement)

  LOperand* elements() const { return left(); }
  LOperand* key() const { return right(); }
};


class LLoadKeyedGeneric: public LBinaryOperation {
 public:
  LLoadKeyedGeneric(LOperand* obj, LOperand* key)
      : LBinaryOperation(obj, key) { }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedGeneric, "load-keyed-generic")

  LOperand* object() const { return left(); }
  LOperand* key() const { return right(); }
};


class LLoadGlobal: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(LoadGlobal, "load-global")
  DECLARE_HYDROGEN_ACCESSOR(LoadGlobal)
};


class LStoreGlobal: public LUnaryOperation {
 public:
  explicit LStoreGlobal(LOperand* value) : LUnaryOperation(value) {}

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobal, "store-global")
  DECLARE_HYDROGEN_ACCESSOR(StoreGlobal)
};


class LLoadContextSlot: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(LoadContextSlot, "load-context-slot")
  DECLARE_HYDROGEN_ACCESSOR(LoadContextSlot)

  int context_chain_length() const {
    return hydrogen()->context_chain_length();
  }
  int slot_index() const { return hydrogen()->slot_index(); }

  virtual void PrintDataTo(StringStream* stream);
};


class LPushArgument: public LUnaryOperation {
 public:
  explicit LPushArgument(LOperand* argument) : LUnaryOperation(argument) {}

  DECLARE_CONCRETE_INSTRUCTION(PushArgument, "push-argument")
};


class LGlobalObject: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(GlobalObject, "global-object")
};


class LGlobalReceiver: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(GlobalReceiver, "global-receiver")
};


class LCallConstantFunction: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallConstantFunction, "call-constant-function")
  DECLARE_HYDROGEN_ACCESSOR(CallConstantFunction)

  virtual void PrintDataTo(StringStream* stream) const;

  Handle<JSFunction> function() const { return hydrogen()->function(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallKeyed: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallKeyed, "call-keyed")
  DECLARE_HYDROGEN_ACCESSOR(CallKeyed)

  virtual void PrintDataTo(StringStream* stream) const;

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallNamed: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallNamed, "call-named")
  DECLARE_HYDROGEN_ACCESSOR(CallNamed)

  virtual void PrintDataTo(StringStream* stream) const;

  Handle<String> name() const { return hydrogen()->name(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallFunction: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallFunction, "call-function")
  DECLARE_HYDROGEN_ACCESSOR(CallFunction)

  int arity() const { return hydrogen()->argument_count() - 2; }
};


class LCallGlobal: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallGlobal, "call-global")
  DECLARE_HYDROGEN_ACCESSOR(CallGlobal)

  virtual void PrintDataTo(StringStream* stream) const;

  Handle<String> name() const {return hydrogen()->name(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallKnownGlobal: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallKnownGlobal, "call-known-global")
  DECLARE_HYDROGEN_ACCESSOR(CallKnownGlobal)

  virtual void PrintDataTo(StringStream* stream) const;

  Handle<JSFunction> target() const { return hydrogen()->target();  }
  int arity() const { return hydrogen()->argument_count() - 1;  }
};


class LCallNew: public LUnaryOperation {
 public:
  explicit LCallNew(LOperand* constructor) : LUnaryOperation(constructor) { }

  DECLARE_CONCRETE_INSTRUCTION(CallNew, "call-new")
  DECLARE_HYDROGEN_ACCESSOR(CallNew)

  virtual void PrintDataTo(StringStream* stream) const;

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallRuntime: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallRuntime, "call-runtime")
  DECLARE_HYDROGEN_ACCESSOR(CallRuntime)

  Runtime::Function* function() const { return hydrogen()->function(); }
  int arity() const { return hydrogen()->argument_count(); }
};


class LInteger32ToDouble: public LUnaryOperation {
 public:
  explicit LInteger32ToDouble(LOperand* use) : LUnaryOperation(use) { }

  DECLARE_CONCRETE_INSTRUCTION(Integer32ToDouble, "int32-to-double")
};


class LNumberTagI: public LUnaryOperation {
 public:
  explicit LNumberTagI(LOperand* use) : LUnaryOperation(use) { }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagI, "number-tag-i")
};


class LNumberTagD: public LUnaryOperation {
 public:
  LNumberTagD(LOperand* value, LOperand* temp1, LOperand* temp2)
      : LUnaryOperation(value), temp1_(temp1), temp2_(temp2) { }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagD, "number-tag-d")

  LOperand* temp1() const { return temp1_; }
  LOperand* temp2() const { return temp2_; }

 private:
  LOperand* temp1_;
  LOperand* temp2_;
};


// Sometimes truncating conversion from a tagged value to an int32.
class LDoubleToI: public LUnaryOperation {
 public:
  explicit LDoubleToI(LOperand* value) : LUnaryOperation(value) { }

  DECLARE_CONCRETE_INSTRUCTION(DoubleToI, "double-to-i")
  DECLARE_HYDROGEN_ACCESSOR(Change)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
};


// Truncating conversion from a tagged value to an int32.
class LTaggedToI: public LUnaryOperation {
 public:
  LTaggedToI(LOperand* value, LOperand* temp)
      : LUnaryOperation(value), temp_(temp) { }

  DECLARE_CONCRETE_INSTRUCTION(TaggedToI, "tagged-to-i")
  DECLARE_HYDROGEN_ACCESSOR(Change)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
  LOperand* temp() const { return temp_; }

 private:
  LOperand* temp_;
};


class LSmiTag: public LUnaryOperation {
 public:
  explicit LSmiTag(LOperand* use) : LUnaryOperation(use) { }

  DECLARE_CONCRETE_INSTRUCTION(SmiTag, "smi-tag")
};


class LNumberUntagD: public LUnaryOperation {
 public:
  explicit LNumberUntagD(LOperand* value) : LUnaryOperation(value) { }

  DECLARE_CONCRETE_INSTRUCTION(NumberUntagD, "double-untag")
};


class LSmiUntag: public LUnaryOperation {
 public:
  LSmiUntag(LOperand* use, bool needs_check)
      : LUnaryOperation(use), needs_check_(needs_check) { }

  DECLARE_CONCRETE_INSTRUCTION(SmiUntag, "smi-untag")

  bool needs_check() const { return needs_check_; }

 private:
  bool needs_check_;
};


class LStoreNamed: public LInstruction {
 public:
  LStoreNamed(LOperand* obj, LOperand* val)
      : object_(obj), value_(val) { }

  DECLARE_INSTRUCTION(StoreNamed)
  DECLARE_HYDROGEN_ACCESSOR(StoreNamed)

  virtual void PrintDataTo(StringStream* stream) const;

  LOperand* object() const { return object_; }
  Handle<Object> name() const { return hydrogen()->name(); }
  LOperand* value() const { return value_; }

 private:
  LOperand* object_;
  LOperand* value_;
};


class LStoreNamedField: public LStoreNamed {
 public:
  LStoreNamedField(LOperand* obj, LOperand* val)
      : LStoreNamed(obj, val) { }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField, "store-named-field")
  DECLARE_HYDROGEN_ACCESSOR(StoreNamedField)

  bool is_in_object() { return hydrogen()->is_in_object(); }
  int offset() { return hydrogen()->offset(); }
  bool needs_write_barrier() { return hydrogen()->NeedsWriteBarrier(); }
  Handle<Map> transition() { return hydrogen()->transition(); }
};


class LStoreNamedGeneric: public LStoreNamed {
 public:
  LStoreNamedGeneric(LOperand* obj, LOperand* val)
      : LStoreNamed(obj, val) { }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedGeneric, "store-named-generic")
  DECLARE_HYDROGEN_ACCESSOR(StoreNamedGeneric)
};


class LStoreKeyed: public LInstruction {
 public:
  LStoreKeyed(LOperand* obj, LOperand* key, LOperand* val)
      : object_(obj), key_(key), value_(val) { }

  DECLARE_INSTRUCTION(StoreKeyed)

  virtual void PrintDataTo(StringStream* stream) const;

  LOperand* object() const { return object_; }
  LOperand* key() const { return key_; }
  LOperand* value() const { return value_; }

 private:
  LOperand* object_;
  LOperand* key_;
  LOperand* value_;
};


class LStoreKeyedFastElement: public LStoreKeyed {
 public:
  LStoreKeyedFastElement(LOperand* obj, LOperand* key, LOperand* val)
      : LStoreKeyed(obj, key, val) {}

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedFastElement,
                               "store-keyed-fast-element")
  DECLARE_HYDROGEN_ACCESSOR(StoreKeyedFastElement)
};


class LStoreKeyedGeneric: public LStoreKeyed {
 public:
  LStoreKeyedGeneric(LOperand* obj, LOperand* key, LOperand* val)
      : LStoreKeyed(obj, key, val) { }

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedGeneric, "store-keyed-generic")
};


class LCheckFunction: public LUnaryOperation {
 public:
  explicit LCheckFunction(LOperand* use) : LUnaryOperation(use) { }

  DECLARE_CONCRETE_INSTRUCTION(CheckFunction, "check-function")
  DECLARE_HYDROGEN_ACCESSOR(CheckFunction)
};


class LCheckInstanceType: public LUnaryOperation {
 public:
  explicit LCheckInstanceType(LOperand* use) : LUnaryOperation(use) { }

  DECLARE_CONCRETE_INSTRUCTION(CheckInstanceType, "check-instance-type")
  DECLARE_HYDROGEN_ACCESSOR(CheckInstanceType)

  LOperand* temp() const { return temp_; }

 private:
  LOperand* temp_;
};


class LCheckMap: public LUnaryOperation {
 public:
  explicit LCheckMap(LOperand* use) : LUnaryOperation(use) { }

  DECLARE_CONCRETE_INSTRUCTION(CheckMap, "check-map")
  DECLARE_HYDROGEN_ACCESSOR(CheckMap)
};


class LCheckPrototypeMaps: public LInstruction {
 public:
  LCheckPrototypeMaps(LOperand* temp1, LOperand* temp2)
      : temp1_(temp1), temp2_(temp2) { }

  DECLARE_CONCRETE_INSTRUCTION(CheckPrototypeMaps, "check-prototype-maps")
  DECLARE_HYDROGEN_ACCESSOR(CheckPrototypeMaps)

  Handle<JSObject> prototype() const { return hydrogen()->prototype(); }
  Handle<JSObject> holder() const { return hydrogen()->holder(); }

  LOperand* temp1() const { return temp1_; }
  LOperand* temp2() const { return temp2_; }

 private:
  LOperand* temp1_;
  LOperand* temp2_;
};


class LCheckSmi: public LUnaryOperation {
 public:
  LCheckSmi(LOperand* use, Condition condition)
      : LUnaryOperation(use), condition_(condition) { }

  Condition condition() const { return condition_; }

  virtual void CompileToNative(LCodeGen* generator);
  virtual const char* Mnemonic() const {
    return (condition_ == eq) ? "check-non-smi" : "check-smi";
  }

 private:
  Condition condition_;
};


class LMaterializedLiteral: public LInstruction {
 public:
  DECLARE_INSTRUCTION(MaterializedLiteral)
};


class LArrayLiteral: public LMaterializedLiteral {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ArrayLiteral, "array-literal")
  DECLARE_HYDROGEN_ACCESSOR(ArrayLiteral)
};


class LObjectLiteral: public LMaterializedLiteral {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ObjectLiteral, "object-literal")
  DECLARE_HYDROGEN_ACCESSOR(ObjectLiteral)
};


class LRegExpLiteral: public LMaterializedLiteral {
 public:
  DECLARE_CONCRETE_INSTRUCTION(RegExpLiteral, "regexp-literal")
  DECLARE_HYDROGEN_ACCESSOR(RegExpLiteral)
};


class LFunctionLiteral: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(FunctionLiteral, "function-literal")
  DECLARE_HYDROGEN_ACCESSOR(FunctionLiteral)

  Handle<SharedFunctionInfo> shared_info() { return hydrogen()->shared_info(); }
};


class LTypeof: public LUnaryOperation {
 public:
  explicit LTypeof(LOperand* input) : LUnaryOperation(input) { }

  DECLARE_CONCRETE_INSTRUCTION(Typeof, "typeof")
};


class LTypeofIs: public LUnaryOperation {
 public:
  explicit LTypeofIs(LOperand* input) : LUnaryOperation(input) { }
  virtual void PrintDataTo(StringStream* stream) const;

  DECLARE_CONCRETE_INSTRUCTION(TypeofIs, "typeof-is")
  DECLARE_HYDROGEN_ACCESSOR(TypeofIs)

  Handle<String> type_literal() { return hydrogen()->type_literal(); }
};


class LTypeofIsAndBranch: public LTypeofIs {
 public:
  LTypeofIsAndBranch(LOperand* value,
                     int true_block_id,
                     int false_block_id)
      : LTypeofIs(value),
        true_block_id_(true_block_id),
        false_block_id_(false_block_id) { }

  DECLARE_CONCRETE_INSTRUCTION(TypeofIsAndBranch, "typeof-is-and-branch")

  virtual void PrintDataTo(StringStream* stream) const;
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LDeleteProperty: public LBinaryOperation {
 public:
  LDeleteProperty(LOperand* obj, LOperand* key) : LBinaryOperation(obj, key) {}

  DECLARE_CONCRETE_INSTRUCTION(DeleteProperty, "delete-property")

  LOperand* object() const { return left(); }
  LOperand* key() const { return right(); }
};


class LOsrEntry: public LInstruction {
 public:
  LOsrEntry();

  DECLARE_CONCRETE_INSTRUCTION(OsrEntry, "osr-entry")

  LOperand** SpilledRegisterArray() { return register_spills_; }
  LOperand** SpilledDoubleRegisterArray() { return double_register_spills_; }

  void MarkSpilledRegister(int allocation_index, LOperand* spill_operand);
  void MarkSpilledDoubleRegister(int allocation_index,
                                 LOperand* spill_operand);

 private:
  // Arrays of spill slot operands for registers with an assigned spill
  // slot, i.e., that must also be restored to the spill slot on OSR entry.
  // NULL if the register has no assigned spill slot.  Indexed by allocation
  // index.
  LOperand* register_spills_[Register::kNumAllocatableRegisters];
  LOperand* double_register_spills_[DoubleRegister::kNumAllocatableRegisters];
};


class LStackCheck: public LInstruction {
 public:
  DECLARE_CONCRETE_INSTRUCTION(StackCheck, "stack-check")
};


class LChunkBuilder;
class LChunk: public ZoneObject {
 public:
  explicit LChunk(HGraph* graph);

  int AddInstruction(LInstruction* instruction, HBasicBlock* block);
  LConstantOperand* DefineConstantOperand(HConstant* constant);
  Handle<Object> LookupLiteral(LConstantOperand* operand) const;
  Representation LookupLiteralRepresentation(LConstantOperand* operand) const;

  int GetNextSpillIndex(bool is_double);
  LOperand* GetNextSpillSlot(bool is_double);

  int ParameterAt(int index);
  int GetParameterStackSlot(int index) const;
  int spill_slot_count() const { return spill_slot_count_; }
  HGraph* graph() const { return graph_; }
  const ZoneList<LInstruction*>* instructions() const { return &instructions_; }
  void AddGapMove(int index, LOperand* from, LOperand* to);
  LGap* GetGapAt(int index) const;
  bool IsGapAt(int index) const;
  int NearestGapPos(int index) const;
  void MarkEmptyBlocks();
  const ZoneList<LPointerMap*>* pointer_maps() const { return &pointer_maps_; }
  LLabel* GetLabel(int block_id) const {
    HBasicBlock* block = graph_->blocks()->at(block_id);
    int first_instruction = block->first_instruction_index();
    return LLabel::cast(instructions_[first_instruction]);
  }
  int LookupDestination(int block_id) const {
    LLabel* cur = GetLabel(block_id);
    while (cur->replacement() != NULL) {
      cur = cur->replacement();
    }
    return cur->block_id();
  }
  Label* GetAssemblyLabel(int block_id) const {
    LLabel* label = GetLabel(block_id);
    ASSERT(!label->HasReplacement());
    return label->label();
  }

  const ZoneList<Handle<JSFunction> >* inlined_closures() const {
    return &inlined_closures_;
  }

  void AddInlinedClosure(Handle<JSFunction> closure) {
    inlined_closures_.Add(closure);
  }

  void Verify() const;

 private:
  int spill_slot_count_;
  HGraph* const graph_;
  ZoneList<LInstruction*> instructions_;
  ZoneList<LPointerMap*> pointer_maps_;
  ZoneList<Handle<JSFunction> > inlined_closures_;
};


class LChunkBuilder BASE_EMBEDDED {
 public:
  LChunkBuilder(HGraph* graph, LAllocator* allocator)
      : chunk_(NULL),
        graph_(graph),
        status_(UNUSED),
        current_instruction_(NULL),
        current_block_(NULL),
        next_block_(NULL),
        argument_count_(0),
        allocator_(allocator),
        position_(RelocInfo::kNoPosition),
        instructions_pending_deoptimization_environment_(NULL),
        pending_deoptimization_ast_id_(AstNode::kNoNumber) { }

  // Build the sequence for the graph.
  LChunk* Build();

  // Declare methods that deal with the individual node types.
#define DECLARE_DO(type) LInstruction* Do##type(H##type* node);
  HYDROGEN_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

 private:
  enum Status {
    UNUSED,
    BUILDING,
    DONE,
    ABORTED
  };

  LChunk* chunk() const { return chunk_; }
  HGraph* graph() const { return graph_; }

  bool is_unused() const { return status_ == UNUSED; }
  bool is_building() const { return status_ == BUILDING; }
  bool is_done() const { return status_ == DONE; }
  bool is_aborted() const { return status_ == ABORTED; }

  void Abort(const char* format, ...);

  // Methods for getting operands for Use / Define / Temp.
  LRegister* ToOperand(Register reg);
  LUnallocated* ToUnallocated(Register reg);
  LUnallocated* ToUnallocated(DoubleRegister reg);

  // Methods for setting up define-use relationships.
  LOperand* Use(HValue* value, LUnallocated* operand);
  LOperand* UseFixed(HValue* value, Register fixed_register);
  LOperand* UseFixedDouble(HValue* value, DoubleRegister fixed_register);

  // A value that is guaranteed to be allocated to a register.
  // Operand created by UseRegister is guaranteed to be live until the end of
  // instruction. This means that register allocator will not reuse it's
  // register for any other operand inside instruction.
  // Operand created by UseRegisterAtStart is guaranteed to be live only at
  // instruction start. Register allocator is free to assign the same register
  // to some other operand used inside instruction (i.e. temporary or
  // output).
  LOperand* UseRegister(HValue* value);
  LOperand* UseRegisterAtStart(HValue* value);

  // A value in a register that may be trashed.
  LOperand* UseTempRegister(HValue* value);
  LOperand* Use(HValue* value);
  LOperand* UseAtStart(HValue* value);
  LOperand* UseOrConstant(HValue* value);
  LOperand* UseOrConstantAtStart(HValue* value);
  LOperand* UseRegisterOrConstant(HValue* value);
  LOperand* UseRegisterOrConstantAtStart(HValue* value);

  // Methods for setting up define-use relationships.
  // Return the same instruction that they are passed.
  LInstruction* Define(LInstruction* instr, LUnallocated* result);
  LInstruction* Define(LInstruction* instr);
  LInstruction* DefineAsRegister(LInstruction* instr);
  LInstruction* DefineAsSpilled(LInstruction* instr, int index);
  LInstruction* DefineSameAsFirst(LInstruction* instr);
  LInstruction* DefineFixed(LInstruction* instr, Register reg);
  LInstruction* DefineFixedDouble(LInstruction* instr, DoubleRegister reg);
  LInstruction* AssignEnvironment(LInstruction* instr);
  LInstruction* AssignPointerMap(LInstruction* instr);

  enum CanDeoptimize { CAN_DEOPTIMIZE_EAGERLY, CANNOT_DEOPTIMIZE_EAGERLY };

  // By default we assume that instruction sequences generated for calls
  // cannot deoptimize eagerly and we do not attach environment to this
  // instruction.
  LInstruction* MarkAsCall(
      LInstruction* instr,
      HInstruction* hinstr,
      CanDeoptimize can_deoptimize = CANNOT_DEOPTIMIZE_EAGERLY);

  LInstruction* SetInstructionPendingDeoptimizationEnvironment(
      LInstruction* instr, int ast_id);
  void ClearInstructionPendingDeoptimizationEnvironment();

  LEnvironment* CreateEnvironment(HEnvironment* hydrogen_env);

  // Temporary operand that must be in a register.
  LUnallocated* TempRegister();
  LOperand* FixedTemp(Register reg);
  LOperand* FixedTemp(DoubleRegister reg);

  void VisitInstruction(HInstruction* current);

  void DoBasicBlock(HBasicBlock* block, HBasicBlock* next_block);
  LInstruction* DoBit(Token::Value op, HBitwiseBinaryOperation* instr);
  LInstruction* DoShift(Token::Value op, HBitwiseBinaryOperation* instr);
  LInstruction* DoArithmeticD(Token::Value op,
                              HArithmeticBinaryOperation* instr);
  LInstruction* DoArithmeticT(Token::Value op,
                              HArithmeticBinaryOperation* instr);

  LChunk* chunk_;
  HGraph* const graph_;
  Status status_;
  HInstruction* current_instruction_;
  HBasicBlock* current_block_;
  HBasicBlock* next_block_;
  int argument_count_;
  LAllocator* allocator_;
  int position_;
  LInstruction* instructions_pending_deoptimization_environment_;
  int pending_deoptimization_ast_id_;

  DISALLOW_COPY_AND_ASSIGN(LChunkBuilder);
};

#undef DECLARE_HYDROGEN_ACCESSOR
#undef DECLARE_INSTRUCTION
#undef DECLARE_CONCRETE_INSTRUCTION

} }  // namespace v8::internal

#endif  // V8_ARM_LITHIUM_ARM_H_
