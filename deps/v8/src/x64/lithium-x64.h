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

#ifndef V8_X64_LITHIUM_X64_H_
#define V8_X64_LITHIUM_X64_H_

#include "hydrogen.h"
#include "lithium-allocator.h"
#include "lithium.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LCodeGen;

#define LITHIUM_ALL_INSTRUCTION_LIST(V)         \
  V(ControlInstruction)                         \
  V(Call)                                       \
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
  V(Context)                                    \
  V(DeleteProperty)                             \
  V(Deoptimize)                                 \
  V(DivI)                                       \
  V(DoubleToI)                                  \
  V(FunctionLiteral)                            \
  V(Gap)                                        \
  V(GlobalObject)                               \
  V(GlobalReceiver)                             \
  V(Goto)                                       \
  V(FixedArrayLength)                           \
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
  V(LoadPixelArrayElement)                      \
  V(LoadPixelArrayExternalPointer)              \
  V(ModI)                                       \
  V(MulI)                                       \
  V(NumberTagD)                                 \
  V(NumberTagI)                                 \
  V(NumberUntagD)                               \
  V(ObjectLiteral)                              \
  V(OsrEntry)                                   \
  V(OuterContext)                               \
  V(Parameter)                                  \
  V(PixelArrayLength)                           \
  V(Power)                                      \
  V(PushArgument)                               \
  V(RegExpLiteral)                              \
  V(Return)                                     \
  V(ShiftI)                                     \
  V(SmiTag)                                     \
  V(SmiUntag)                                   \
  V(StackCheck)                                 \
  V(StoreContextSlot)                           \
  V(StoreGlobal)                                \
  V(StoreKeyedFastElement)                      \
  V(StoreKeyedGeneric)                          \
  V(StoreNamedField)                            \
  V(StoreNamedGeneric)                          \
  V(StorePixelArrayElement)                     \
  V(StringCharCodeAt)                           \
  V(StringLength)                               \
  V(SubI)                                       \
  V(TaggedToI)                                  \
  V(Throw)                                      \
  V(Typeof)                                     \
  V(TypeofIs)                                   \
  V(TypeofIsAndBranch)                          \
  V(IsConstructCall)                            \
  V(IsConstructCallAndBranch)                   \
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
      :  environment_(NULL),
         hydrogen_value_(NULL),
         is_call_(false),
         is_save_doubles_(false) { }

  virtual ~LInstruction() { }

  virtual void CompileToNative(LCodeGen* generator) = 0;
  virtual const char* Mnemonic() const = 0;
  virtual void PrintTo(StringStream* stream);
  virtual void PrintDataTo(StringStream* stream) = 0;
  virtual void PrintOutputOperandTo(StringStream* stream) = 0;

  // Declare virtual type testers.
#define DECLARE_DO(type) virtual bool Is##type() const { return false; }
  LITHIUM_ALL_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

  virtual bool IsControl() const { return false; }
  virtual void SetBranchTargets(int true_block_id, int false_block_id) { }

  void set_environment(LEnvironment* env) { environment_ = env; }
  LEnvironment* environment() const { return environment_; }
  bool HasEnvironment() const { return environment_ != NULL; }

  void set_pointer_map(LPointerMap* p) { pointer_map_.set(p); }
  LPointerMap* pointer_map() const { return pointer_map_.get(); }
  bool HasPointerMap() const { return pointer_map_.is_set(); }

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

  void MarkAsCall() { is_call_ = true; }
  void MarkAsSaveDoubles() { is_save_doubles_ = true; }

  // Interface to the register allocator and iterators.
  bool IsMarkedAsCall() const { return is_call_; }
  bool IsMarkedAsSaveDoubles() const { return is_save_doubles_; }

  virtual bool HasResult() const = 0;
  virtual LOperand* result() = 0;

  virtual int InputCount() = 0;
  virtual LOperand* InputAt(int i) = 0;
  virtual int TempCount() = 0;
  virtual LOperand* TempAt(int i) = 0;

  LOperand* FirstInput() { return InputAt(0); }
  LOperand* Output() { return HasResult() ? result() : NULL; }

#ifdef DEBUG
  void VerifyCall();
#endif

 private:
  LEnvironment* environment_;
  SetOncePointer<LPointerMap> pointer_map_;
  HValue* hydrogen_value_;
  SetOncePointer<LEnvironment> deoptimization_environment_;
  bool is_call_;
  bool is_save_doubles_;
};


template<typename ElementType, int NumElements>
class OperandContainer {
 public:
  OperandContainer() {
    for (int i = 0; i < NumElements; i++) elems_[i] = NULL;
  }
  int length() { return NumElements; }
  ElementType& operator[](int i) {
    ASSERT(i < length());
    return elems_[i];
  }
  void PrintOperandsTo(StringStream* stream);

 private:
  ElementType elems_[NumElements];
};


template<typename ElementType>
class OperandContainer<ElementType, 0> {
 public:
  int length() { return 0; }
  void PrintOperandsTo(StringStream* stream) { }
  ElementType& operator[](int i) {
    UNREACHABLE();
    static ElementType t = 0;
    return t;
  }
};


// R = number of result operands (0 or 1).
// I = number of input operands.
// T = number of temporary operands.
template<int R, int I, int T>
class LTemplateInstruction: public LInstruction {
 public:
  // Allow 0 or 1 output operands.
  STATIC_ASSERT(R == 0 || R == 1);
  virtual bool HasResult() const { return R != 0; }
  void set_result(LOperand* operand) { results_[0] = operand; }
  LOperand* result() { return results_[0]; }

  int InputCount() { return I; }
  LOperand* InputAt(int i) { return inputs_[i]; }

  int TempCount() { return T; }
  LOperand* TempAt(int i) { return temps_[i]; }

  virtual void PrintDataTo(StringStream* stream);
  virtual void PrintOutputOperandTo(StringStream* stream);

 protected:
  OperandContainer<LOperand*, R> results_;
  OperandContainer<LOperand*, I> inputs_;
  OperandContainer<LOperand*, T> temps_;
};


class LGap: public LTemplateInstruction<0, 0, 0> {
 public:
  explicit LGap(HBasicBlock* block)
      : block_(block) {
    parallel_moves_[BEFORE] = NULL;
    parallel_moves_[START] = NULL;
    parallel_moves_[END] = NULL;
    parallel_moves_[AFTER] = NULL;
  }

  DECLARE_CONCRETE_INSTRUCTION(Gap, "gap")
  virtual void PrintDataTo(StringStream* stream);

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


class LGoto: public LTemplateInstruction<0, 0, 0> {
 public:
  LGoto(int block_id, bool include_stack_check = false)
    : block_id_(block_id), include_stack_check_(include_stack_check) { }

  DECLARE_CONCRETE_INSTRUCTION(Goto, "goto")
  virtual void PrintDataTo(StringStream* stream);
  virtual bool IsControl() const { return true; }

  int block_id() const { return block_id_; }
  bool include_stack_check() const { return include_stack_check_; }

 private:
  int block_id_;
  bool include_stack_check_;
};


class LLazyBailout: public LTemplateInstruction<0, 0, 0> {
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


class LDeoptimize: public LTemplateInstruction<0, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Deoptimize, "deoptimize")
};


class LLabel: public LGap {
 public:
  explicit LLabel(HBasicBlock* block)
      : LGap(block), replacement_(NULL) { }

  DECLARE_CONCRETE_INSTRUCTION(Label, "label")

  virtual void PrintDataTo(StringStream* stream);

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


class LParameter: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Parameter, "parameter")
};


class LCallStub: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallStub, "call-stub")
  DECLARE_HYDROGEN_ACCESSOR(CallStub)

  TranscendentalCache::Type transcendental_type() {
    return hydrogen()->transcendental_type();
  }
};


class LUnknownOSRValue: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(UnknownOSRValue, "unknown-osr-value")
};


template<int I, int T>
class LControlInstruction: public LTemplateInstruction<0, I, T> {
 public:
  DECLARE_INSTRUCTION(ControlInstruction)
  virtual bool IsControl() const { return true; }

  int true_block_id() const { return true_block_id_; }
  int false_block_id() const { return false_block_id_; }
  void SetBranchTargets(int true_block_id, int false_block_id) {
    true_block_id_ = true_block_id;
    false_block_id_ = false_block_id;
  }

 private:
  int true_block_id_;
  int false_block_id_;
};


class LApplyArguments: public LTemplateInstruction<1, 4, 0> {
 public:
  LApplyArguments(LOperand* function,
                  LOperand* receiver,
                  LOperand* length,
                  LOperand* elements) {
    inputs_[0] = function;
    inputs_[1] = receiver;
    inputs_[2] = length;
    inputs_[3] = elements;
  }

  DECLARE_CONCRETE_INSTRUCTION(ApplyArguments, "apply-arguments")

  LOperand* function() { return inputs_[0]; }
  LOperand* receiver() { return inputs_[1]; }
  LOperand* length() { return inputs_[2]; }
  LOperand* elements() { return inputs_[3]; }
};


class LAccessArgumentsAt: public LTemplateInstruction<1, 3, 0> {
 public:
  LAccessArgumentsAt(LOperand* arguments, LOperand* length, LOperand* index) {
    inputs_[0] = arguments;
    inputs_[1] = length;
    inputs_[2] = index;
  }

  DECLARE_CONCRETE_INSTRUCTION(AccessArgumentsAt, "access-arguments-at")

  LOperand* arguments() { return inputs_[0]; }
  LOperand* length() { return inputs_[1]; }
  LOperand* index() { return inputs_[2]; }

  virtual void PrintDataTo(StringStream* stream);
};


class LArgumentsLength: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LArgumentsLength(LOperand* elements) {
    inputs_[0] = elements;
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength, "arguments-length")
};


class LArgumentsElements: public LTemplateInstruction<1, 0, 0> {
 public:
  LArgumentsElements() { }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements, "arguments-elements")
};


class LModI: public LTemplateInstruction<1, 2, 1> {
 public:
  LModI(LOperand* left, LOperand* right, LOperand* temp) {
    inputs_[0] = left;
    inputs_[1] = right;
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(ModI, "mod-i")
  DECLARE_HYDROGEN_ACCESSOR(Mod)
};


class LDivI: public LTemplateInstruction<1, 2, 1> {
 public:
  LDivI(LOperand* left, LOperand* right, LOperand* temp) {
    inputs_[0] = left;
    inputs_[1] = right;
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(DivI, "div-i")
  DECLARE_HYDROGEN_ACCESSOR(Div)
};


class LMulI: public LTemplateInstruction<1, 2, 0> {
 public:
  LMulI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(MulI, "mul-i")
  DECLARE_HYDROGEN_ACCESSOR(Mul)
};


class LCmpID: public LTemplateInstruction<1, 2, 0> {
 public:
  LCmpID(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(CmpID, "cmp-id")
  DECLARE_HYDROGEN_ACCESSOR(Compare)

  Token::Value op() const { return hydrogen()->token(); }
  bool is_double() const {
    return hydrogen()->GetInputRepresentation().IsDouble();
  }
};


class LCmpIDAndBranch: public LControlInstruction<2, 0> {
 public:
  LCmpIDAndBranch(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(CmpIDAndBranch, "cmp-id-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(Compare)

  Token::Value op() const { return hydrogen()->token(); }
  bool is_double() const {
    return hydrogen()->GetInputRepresentation().IsDouble();
  }

  virtual void PrintDataTo(StringStream* stream);
};


class LUnaryMathOperation: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LUnaryMathOperation(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(UnaryMathOperation, "unary-math-operation")
  DECLARE_HYDROGEN_ACCESSOR(UnaryMathOperation)

  virtual void PrintDataTo(StringStream* stream);
  BuiltinFunctionId op() const { return hydrogen()->op(); }
};


class LCmpJSObjectEq: public LTemplateInstruction<1, 2, 0> {
 public:
  LCmpJSObjectEq(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(CmpJSObjectEq, "cmp-jsobject-eq")
};


class LCmpJSObjectEqAndBranch: public LControlInstruction<2, 0> {
 public:
  LCmpJSObjectEqAndBranch(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(CmpJSObjectEqAndBranch,
                               "cmp-jsobject-eq-and-branch")
};


class LIsNull: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LIsNull(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(IsNull, "is-null")
  DECLARE_HYDROGEN_ACCESSOR(IsNull)

  bool is_strict() const { return hydrogen()->is_strict(); }
};


class LIsNullAndBranch: public LControlInstruction<1, 1> {
 public:
  LIsNullAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(IsNullAndBranch, "is-null-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(IsNull)

  bool is_strict() const { return hydrogen()->is_strict(); }

  virtual void PrintDataTo(StringStream* stream);
};


class LIsObject: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LIsObject(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(IsObject, "is-object")
};


class LIsObjectAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LIsObjectAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(IsObjectAndBranch, "is-object-and-branch")

  virtual void PrintDataTo(StringStream* stream);
};


class LIsSmi: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LIsSmi(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(IsSmi, "is-smi")
  DECLARE_HYDROGEN_ACCESSOR(IsSmi)
};


class LIsSmiAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LIsSmiAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch, "is-smi-and-branch")

  virtual void PrintDataTo(StringStream* stream);
};


class LHasInstanceType: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LHasInstanceType(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceType, "has-instance-type")
  DECLARE_HYDROGEN_ACCESSOR(HasInstanceType)
};


class LHasInstanceTypeAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LHasInstanceTypeAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceTypeAndBranch,
                               "has-instance-type-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(HasInstanceType)

  virtual void PrintDataTo(StringStream* stream);
};


class LHasCachedArrayIndex: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LHasCachedArrayIndex(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndex, "has-cached-array-index")
  DECLARE_HYDROGEN_ACCESSOR(HasCachedArrayIndex)
};


class LHasCachedArrayIndexAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LHasCachedArrayIndexAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndexAndBranch,
                               "has-cached-array-index-and-branch")
  virtual void PrintDataTo(StringStream* stream);
};


class LClassOfTest: public LTemplateInstruction<1, 1, 1> {
 public:
  LClassOfTest(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTest, "class-of-test")
  DECLARE_HYDROGEN_ACCESSOR(ClassOfTest)

  virtual void PrintDataTo(StringStream* stream);
};


class LClassOfTestAndBranch: public LControlInstruction<1, 1> {
 public:
  LClassOfTestAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch,
                               "class-of-test-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(ClassOfTest)

  virtual void PrintDataTo(StringStream* stream);
};


class LCmpT: public LTemplateInstruction<1, 2, 0> {
 public:
  LCmpT(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(CmpT, "cmp-t")
  DECLARE_HYDROGEN_ACCESSOR(Compare)

  Token::Value op() const { return hydrogen()->token(); }
};


class LCmpTAndBranch: public LControlInstruction<2, 0> {
 public:
  LCmpTAndBranch(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(CmpTAndBranch, "cmp-t-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(Compare)

  Token::Value op() const { return hydrogen()->token(); }
};


class LInstanceOf: public LTemplateInstruction<1, 2, 0> {
 public:
  LInstanceOf(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOf, "instance-of")
};


class LInstanceOfAndBranch: public LControlInstruction<2, 0> {
 public:
  LInstanceOfAndBranch(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfAndBranch, "instance-of-and-branch")
};


class LInstanceOfKnownGlobal: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LInstanceOfKnownGlobal(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfKnownGlobal,
                               "instance-of-known-global")
  DECLARE_HYDROGEN_ACCESSOR(InstanceOfKnownGlobal)

  Handle<JSFunction> function() const { return hydrogen()->function(); }
};


class LBoundsCheck: public LTemplateInstruction<0, 2, 0> {
 public:
  LBoundsCheck(LOperand* index, LOperand* length) {
    inputs_[0] = index;
    inputs_[1] = length;
  }

  LOperand* index() { return inputs_[0]; }
  LOperand* length() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheck, "bounds-check")
};


class LBitI: public LTemplateInstruction<1, 2, 0> {
 public:
  LBitI(Token::Value op, LOperand* left, LOperand* right)
      : op_(op) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  Token::Value op() const { return op_; }

  DECLARE_CONCRETE_INSTRUCTION(BitI, "bit-i")

 private:
  Token::Value op_;
};


class LShiftI: public LTemplateInstruction<1, 2, 0> {
 public:
  LShiftI(Token::Value op, LOperand* left, LOperand* right, bool can_deopt)
      : op_(op), can_deopt_(can_deopt) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  Token::Value op() const { return op_; }

  bool can_deopt() const { return can_deopt_; }

  DECLARE_CONCRETE_INSTRUCTION(ShiftI, "shift-i")

 private:
  Token::Value op_;
  bool can_deopt_;
};


class LSubI: public LTemplateInstruction<1, 2, 0> {
 public:
  LSubI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(SubI, "sub-i")
  DECLARE_HYDROGEN_ACCESSOR(Sub)
};


class LConstantI: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ConstantI, "constant-i")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  int32_t value() const { return hydrogen()->Integer32Value(); }
};


class LConstantD: public LTemplateInstruction<1, 0, 1> {
 public:
  explicit LConstantD(LOperand* temp) {
    temps_[0] = temp;
  }
  DECLARE_CONCRETE_INSTRUCTION(ConstantD, "constant-d")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  double value() const { return hydrogen()->DoubleValue(); }
};


class LConstantT: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ConstantT, "constant-t")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  Handle<Object> value() const { return hydrogen()->handle(); }
};


class LBranch: public LControlInstruction<1, 0> {
 public:
  explicit LBranch(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(Branch, "branch")
  DECLARE_HYDROGEN_ACCESSOR(Value)

  virtual void PrintDataTo(StringStream* stream);
};


class LCmpMapAndBranch: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCmpMapAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(CmpMapAndBranch, "cmp-map-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(CompareMap)

  virtual bool IsControl() const { return true; }

  Handle<Map> map() const { return hydrogen()->map(); }
  int true_block_id() const {
    return hydrogen()->FirstSuccessor()->block_id();
  }
  int false_block_id() const {
    return hydrogen()->SecondSuccessor()->block_id();
  }
};


class LJSArrayLength: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LJSArrayLength(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(JSArrayLength, "js-array-length")
  DECLARE_HYDROGEN_ACCESSOR(JSArrayLength)
};


class LPixelArrayLength: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LPixelArrayLength(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(PixelArrayLength, "pixel-array-length")
  DECLARE_HYDROGEN_ACCESSOR(PixelArrayLength)
};


class LFixedArrayLength: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LFixedArrayLength(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(FixedArrayLength, "fixed-array-length")
  DECLARE_HYDROGEN_ACCESSOR(FixedArrayLength)
};


class LValueOf: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LValueOf(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(ValueOf, "value-of")
  DECLARE_HYDROGEN_ACCESSOR(ValueOf)
};


class LThrow: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LThrow(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(Throw, "throw")
};


class LBitNotI: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LBitNotI(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(BitNotI, "bit-not-i")
};


class LAddI: public LTemplateInstruction<1, 2, 0> {
 public:
  LAddI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(AddI, "add-i")
  DECLARE_HYDROGEN_ACCESSOR(Add)
};


class LPower: public LTemplateInstruction<1, 2, 0> {
 public:
  LPower(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(Power, "power")
  DECLARE_HYDROGEN_ACCESSOR(Power)
};


class LArithmeticD: public LTemplateInstruction<1, 2, 0> {
 public:
  LArithmeticD(Token::Value op, LOperand* left, LOperand* right)
      : op_(op) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  Token::Value op() const { return op_; }

  virtual void CompileToNative(LCodeGen* generator);
  virtual const char* Mnemonic() const;

 private:
  Token::Value op_;
};


class LArithmeticT: public LTemplateInstruction<1, 2, 0> {
 public:
  LArithmeticT(Token::Value op, LOperand* left, LOperand* right)
      : op_(op) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  virtual void CompileToNative(LCodeGen* generator);
  virtual const char* Mnemonic() const;

  Token::Value op() const { return op_; }

 private:
  Token::Value op_;
};


class LReturn: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LReturn(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(Return, "return")
};


class LLoadNamedField: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadNamedField(LOperand* object) {
    inputs_[0] = object;
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField, "load-named-field")
  DECLARE_HYDROGEN_ACCESSOR(LoadNamedField)
};


class LLoadNamedGeneric: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadNamedGeneric(LOperand* object) {
    inputs_[0] = object;
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedGeneric, "load-named-generic")
  DECLARE_HYDROGEN_ACCESSOR(LoadNamedGeneric)

  LOperand* object() { return inputs_[0]; }
  Handle<Object> name() const { return hydrogen()->name(); }
};


class LLoadFunctionPrototype: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadFunctionPrototype(LOperand* function) {
    inputs_[0] = function;
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype, "load-function-prototype")
  DECLARE_HYDROGEN_ACCESSOR(LoadFunctionPrototype)

  LOperand* function() { return inputs_[0]; }
};


class LLoadElements: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadElements(LOperand* object) {
    inputs_[0] = object;
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadElements, "load-elements")
};


class LLoadPixelArrayExternalPointer: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadPixelArrayExternalPointer(LOperand* object) {
    inputs_[0] = object;
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadPixelArrayExternalPointer,
                               "load-pixel-array-external-pointer")
};


class LLoadKeyedFastElement: public LTemplateInstruction<1, 2, 0> {
 public:
  LLoadKeyedFastElement(LOperand* elements, LOperand* key) {
    inputs_[0] = elements;
    inputs_[1] = key;
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedFastElement, "load-keyed-fast-element")
  DECLARE_HYDROGEN_ACCESSOR(LoadKeyedFastElement)

  LOperand* elements() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
};


class LLoadPixelArrayElement: public LTemplateInstruction<1, 2, 0> {
 public:
  LLoadPixelArrayElement(LOperand* external_pointer, LOperand* key) {
    inputs_[0] = external_pointer;
    inputs_[1] = key;
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadPixelArrayElement,
                               "load-pixel-array-element")
  DECLARE_HYDROGEN_ACCESSOR(LoadPixelArrayElement)

  LOperand* external_pointer() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
};


class LLoadKeyedGeneric: public LTemplateInstruction<1, 2, 0> {
 public:
  LLoadKeyedGeneric(LOperand* obj, LOperand* key) {
    inputs_[0] = obj;
    inputs_[1] = key;
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedGeneric, "load-keyed-generic")

  LOperand* object() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
};


class LLoadGlobal: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(LoadGlobal, "load-global")
  DECLARE_HYDROGEN_ACCESSOR(LoadGlobal)
};


class LStoreGlobal: public LTemplateInstruction<0, 1, 1> {
 public:
  explicit LStoreGlobal(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobal, "store-global")
  DECLARE_HYDROGEN_ACCESSOR(StoreGlobal)
};


class LLoadContextSlot: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadContextSlot(LOperand* context) {
    inputs_[0] = context;
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadContextSlot, "load-context-slot")
  DECLARE_HYDROGEN_ACCESSOR(LoadContextSlot)

  LOperand* context() { return InputAt(0); }
  int slot_index() { return hydrogen()->slot_index(); }

  virtual void PrintDataTo(StringStream* stream);
};


class LStoreContextSlot: public LTemplateInstruction<0, 2, 0> {
 public:
  LStoreContextSlot(LOperand* context, LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreContextSlot, "store-context-slot")
  DECLARE_HYDROGEN_ACCESSOR(StoreContextSlot)

  LOperand* context() { return InputAt(0); }
  LOperand* value() { return InputAt(1); }
  int slot_index() { return hydrogen()->slot_index(); }
  int needs_write_barrier() { return hydrogen()->NeedsWriteBarrier(); }

  virtual void PrintDataTo(StringStream* stream);
};


class LPushArgument: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LPushArgument(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(PushArgument, "push-argument")
};


class LContext: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Context, "context")
};


class LOuterContext: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LOuterContext(LOperand* context) {
    inputs_[0] = context;
  }

  DECLARE_CONCRETE_INSTRUCTION(OuterContext, "outer-context")

  LOperand* context() { return InputAt(0); }
};


class LGlobalObject: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(GlobalObject, "global-object")
};


class LGlobalReceiver: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(GlobalReceiver, "global-receiver")
};


class LCallConstantFunction: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallConstantFunction, "call-constant-function")
  DECLARE_HYDROGEN_ACCESSOR(CallConstantFunction)

  virtual void PrintDataTo(StringStream* stream);

  Handle<JSFunction> function() { return hydrogen()->function(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallKeyed: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCallKeyed(LOperand* key) {
    inputs_[0] = key;
  }

  DECLARE_CONCRETE_INSTRUCTION(CallKeyed, "call-keyed")
  DECLARE_HYDROGEN_ACCESSOR(CallKeyed)

  LOperand* key() { return inputs_[0]; }

  virtual void PrintDataTo(StringStream* stream);

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallNamed: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallNamed, "call-named")
  DECLARE_HYDROGEN_ACCESSOR(CallNamed)

  virtual void PrintDataTo(StringStream* stream);

  Handle<String> name() const { return hydrogen()->name(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallFunction: public LTemplateInstruction<1, 0, 0> {
 public:
  LCallFunction() {}

  DECLARE_CONCRETE_INSTRUCTION(CallFunction, "call-function")
  DECLARE_HYDROGEN_ACCESSOR(CallFunction)

  int arity() const { return hydrogen()->argument_count() - 2; }
};


class LCallGlobal: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallGlobal, "call-global")
  DECLARE_HYDROGEN_ACCESSOR(CallGlobal)

  virtual void PrintDataTo(StringStream* stream);

  Handle<String> name() const {return hydrogen()->name(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallKnownGlobal: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallKnownGlobal, "call-known-global")
  DECLARE_HYDROGEN_ACCESSOR(CallKnownGlobal)

  virtual void PrintDataTo(StringStream* stream);

  Handle<JSFunction> target() const { return hydrogen()->target();  }
  int arity() const { return hydrogen()->argument_count() - 1;  }
};


class LCallNew: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCallNew(LOperand* constructor) {
    inputs_[0] = constructor;
  }

  DECLARE_CONCRETE_INSTRUCTION(CallNew, "call-new")
  DECLARE_HYDROGEN_ACCESSOR(CallNew)

  virtual void PrintDataTo(StringStream* stream);

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallRuntime: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallRuntime, "call-runtime")
  DECLARE_HYDROGEN_ACCESSOR(CallRuntime)

  Runtime::Function* function() const { return hydrogen()->function(); }
  int arity() const { return hydrogen()->argument_count(); }
};


class LInteger32ToDouble: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LInteger32ToDouble(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(Integer32ToDouble, "int32-to-double")
};


class LNumberTagI: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LNumberTagI(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagI, "number-tag-i")
};


class LNumberTagD: public LTemplateInstruction<1, 1, 1> {
 public:
  explicit LNumberTagD(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagD, "number-tag-d")
};


// Sometimes truncating conversion from a tagged value to an int32.
class LDoubleToI: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LDoubleToI(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(DoubleToI, "double-to-i")
  DECLARE_HYDROGEN_ACCESSOR(Change)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
};


// Truncating conversion from a tagged value to an int32.
class LTaggedToI: public LTemplateInstruction<1, 1, 1> {
 public:
  LTaggedToI(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(TaggedToI, "tagged-to-i")
  DECLARE_HYDROGEN_ACCESSOR(Change)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
};


class LSmiTag: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LSmiTag(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(SmiTag, "smi-tag")
};


class LNumberUntagD: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LNumberUntagD(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(NumberUntagD, "double-untag")
};


class LSmiUntag: public LTemplateInstruction<1, 1, 0> {
 public:
  LSmiUntag(LOperand* value, bool needs_check)
      : needs_check_(needs_check) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(SmiUntag, "smi-untag")

  bool needs_check() const { return needs_check_; }

 private:
  bool needs_check_;
};


class LStoreNamedField: public LTemplateInstruction<0, 2, 1> {
 public:
  LStoreNamedField(LOperand* object, LOperand* value, LOperand* temp) {
    inputs_[0] = object;
    inputs_[1] = value;
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField, "store-named-field")
  DECLARE_HYDROGEN_ACCESSOR(StoreNamedField)

  virtual void PrintDataTo(StringStream* stream);

  LOperand* object() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }

  Handle<Object> name() const { return hydrogen()->name(); }
  bool is_in_object() { return hydrogen()->is_in_object(); }
  int offset() { return hydrogen()->offset(); }
  bool needs_write_barrier() { return hydrogen()->NeedsWriteBarrier(); }
  Handle<Map> transition() const { return hydrogen()->transition(); }
};


class LStoreNamedGeneric: public LTemplateInstruction<0, 2, 0> {
 public:
  LStoreNamedGeneric(LOperand* object, LOperand* value) {
    inputs_[0] = object;
    inputs_[1] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedGeneric, "store-named-generic")
  DECLARE_HYDROGEN_ACCESSOR(StoreNamedGeneric)

  virtual void PrintDataTo(StringStream* stream);

  LOperand* object() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }
  Handle<Object> name() const { return hydrogen()->name(); }
};


class LStoreKeyedFastElement: public LTemplateInstruction<0, 3, 0> {
 public:
  LStoreKeyedFastElement(LOperand* obj, LOperand* key, LOperand* val) {
    inputs_[0] = obj;
    inputs_[1] = key;
    inputs_[2] = val;
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedFastElement,
                               "store-keyed-fast-element")
  DECLARE_HYDROGEN_ACCESSOR(StoreKeyedFastElement)

  virtual void PrintDataTo(StringStream* stream);

  LOperand* object() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
  LOperand* value() { return inputs_[2]; }
};


class LStorePixelArrayElement: public LTemplateInstruction<0, 3, 0> {
 public:
  LStorePixelArrayElement(LOperand* external_pointer,
                          LOperand* key,
                          LOperand* val) {
    inputs_[0] = external_pointer;
    inputs_[1] = key;
    inputs_[2] = val;
  }

  DECLARE_CONCRETE_INSTRUCTION(StorePixelArrayElement,
                               "store-pixel-array-element")
  DECLARE_HYDROGEN_ACCESSOR(StorePixelArrayElement)

  LOperand* external_pointer() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
  LOperand* value() { return inputs_[2]; }
};


class LStoreKeyedGeneric: public LTemplateInstruction<0, 3, 0> {
 public:
  LStoreKeyedGeneric(LOperand* object, LOperand* key, LOperand* value) {
    inputs_[0] = object;
    inputs_[1] = key;
    inputs_[2] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedGeneric, "store-keyed-generic")

  virtual void PrintDataTo(StringStream* stream);

  LOperand* object() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
  LOperand* value() { return inputs_[2]; }
};


class LStringCharCodeAt: public LTemplateInstruction<1, 2, 0> {
 public:
  LStringCharCodeAt(LOperand* string, LOperand* index) {
    inputs_[0] = string;
    inputs_[1] = index;
  }

  DECLARE_CONCRETE_INSTRUCTION(StringCharCodeAt, "string-char-code-at")
  DECLARE_HYDROGEN_ACCESSOR(StringCharCodeAt)

  LOperand* string() { return inputs_[0]; }
  LOperand* index() { return inputs_[1]; }
};


class LStringLength: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LStringLength(LOperand* string) {
    inputs_[0] = string;
  }

  DECLARE_CONCRETE_INSTRUCTION(StringLength, "string-length")
  DECLARE_HYDROGEN_ACCESSOR(StringLength)

  LOperand* string() { return inputs_[0]; }
};


class LCheckFunction: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckFunction(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckFunction, "check-function")
  DECLARE_HYDROGEN_ACCESSOR(CheckFunction)
};


class LCheckInstanceType: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckInstanceType(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckInstanceType, "check-instance-type")
  DECLARE_HYDROGEN_ACCESSOR(CheckInstanceType)
};


class LCheckMap: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckMap(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckMap, "check-map")
  DECLARE_HYDROGEN_ACCESSOR(CheckMap)
};


class LCheckPrototypeMaps: public LTemplateInstruction<0, 0, 1> {
 public:
  explicit LCheckPrototypeMaps(LOperand* temp)  {
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckPrototypeMaps, "check-prototype-maps")
  DECLARE_HYDROGEN_ACCESSOR(CheckPrototypeMaps)

  Handle<JSObject> prototype() const { return hydrogen()->prototype(); }
  Handle<JSObject> holder() const { return hydrogen()->holder(); }
};


class LCheckSmi: public LTemplateInstruction<0, 1, 0> {
 public:
  LCheckSmi(LOperand* value, Condition condition)
      : condition_(condition) {
    inputs_[0] = value;
  }

  Condition condition() const { return condition_; }

  virtual void CompileToNative(LCodeGen* generator);
  virtual const char* Mnemonic() const {
    return (condition_ == zero) ? "check-non-smi" : "check-smi";
  }

 private:
  Condition condition_;
};


class LArrayLiteral: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ArrayLiteral, "array-literal")
  DECLARE_HYDROGEN_ACCESSOR(ArrayLiteral)
};


class LObjectLiteral: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ObjectLiteral, "object-literal")
  DECLARE_HYDROGEN_ACCESSOR(ObjectLiteral)
};


class LRegExpLiteral: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(RegExpLiteral, "regexp-literal")
  DECLARE_HYDROGEN_ACCESSOR(RegExpLiteral)
};


class LFunctionLiteral: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(FunctionLiteral, "function-literal")
  DECLARE_HYDROGEN_ACCESSOR(FunctionLiteral)

  Handle<SharedFunctionInfo> shared_info() { return hydrogen()->shared_info(); }
};


class LTypeof: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LTypeof(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(Typeof, "typeof")
};


class LTypeofIs: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LTypeofIs(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(TypeofIs, "typeof-is")
  DECLARE_HYDROGEN_ACCESSOR(TypeofIs)

  Handle<String> type_literal() { return hydrogen()->type_literal(); }

  virtual void PrintDataTo(StringStream* stream);
};


class LTypeofIsAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LTypeofIsAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  DECLARE_CONCRETE_INSTRUCTION(TypeofIsAndBranch, "typeof-is-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(TypeofIs)

  Handle<String> type_literal() { return hydrogen()->type_literal(); }

  virtual void PrintDataTo(StringStream* stream);
};


class LIsConstructCall: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(IsConstructCall, "is-construct-call")
  DECLARE_HYDROGEN_ACCESSOR(IsConstructCall)
};


class LIsConstructCallAndBranch: public LControlInstruction<0, 1> {
 public:
  explicit LIsConstructCallAndBranch(LOperand* temp) {
    temps_[0] = temp;
  }

  DECLARE_CONCRETE_INSTRUCTION(IsConstructCallAndBranch,
                               "is-construct-call-and-branch")
};


class LDeleteProperty: public LTemplateInstruction<1, 2, 0> {
 public:
  LDeleteProperty(LOperand* obj, LOperand* key) {
    inputs_[0] = obj;
    inputs_[1] = key;
  }

  DECLARE_CONCRETE_INSTRUCTION(DeleteProperty, "delete-property")

  LOperand* object() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
};


class LOsrEntry: public LTemplateInstruction<0, 0, 0> {
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


class LStackCheck: public LTemplateInstruction<0, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(StackCheck, "stack-check")
};


class LChunkBuilder;
class LChunk: public ZoneObject {
 public:
  explicit LChunk(HGraph* graph)
    : spill_slot_count_(0),
      graph_(graph),
      instructions_(32),
      pointer_maps_(8),
      inlined_closures_(1) { }

  void AddInstruction(LInstruction* instruction, HBasicBlock* block);
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
        instruction_pending_deoptimization_environment_(NULL),
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
  LUnallocated* ToUnallocated(XMMRegister reg);

  // Methods for setting up define-use relationships.
  MUST_USE_RESULT LOperand* Use(HValue* value, LUnallocated* operand);
  MUST_USE_RESULT LOperand* UseFixed(HValue* value, Register fixed_register);
  MUST_USE_RESULT LOperand* UseFixedDouble(HValue* value,
                                           XMMRegister fixed_register);

  // A value that is guaranteed to be allocated to a register.
  // Operand created by UseRegister is guaranteed to be live until the end of
  // instruction. This means that register allocator will not reuse it's
  // register for any other operand inside instruction.
  // Operand created by UseRegisterAtStart is guaranteed to be live only at
  // instruction start. Register allocator is free to assign the same register
  // to some other operand used inside instruction (i.e. temporary or
  // output).
  MUST_USE_RESULT LOperand* UseRegister(HValue* value);
  MUST_USE_RESULT LOperand* UseRegisterAtStart(HValue* value);

  // An input operand in a register that may be trashed.
  MUST_USE_RESULT LOperand* UseTempRegister(HValue* value);

  // An input operand in a register or stack slot.
  MUST_USE_RESULT LOperand* Use(HValue* value);
  MUST_USE_RESULT LOperand* UseAtStart(HValue* value);

  // An input operand in a register, stack slot or a constant operand.
  MUST_USE_RESULT LOperand* UseOrConstant(HValue* value);
  MUST_USE_RESULT LOperand* UseOrConstantAtStart(HValue* value);

  // An input operand in a register or a constant operand.
  MUST_USE_RESULT LOperand* UseRegisterOrConstant(HValue* value);
  MUST_USE_RESULT LOperand* UseRegisterOrConstantAtStart(HValue* value);

  // An input operand in register, stack slot or a constant operand.
  // Will not be moved to a register even if one is freely available.
  MUST_USE_RESULT LOperand* UseAny(HValue* value);

  // Temporary operand that must be in a register.
  MUST_USE_RESULT LUnallocated* TempRegister();
  MUST_USE_RESULT LOperand* FixedTemp(Register reg);
  MUST_USE_RESULT LOperand* FixedTemp(XMMRegister reg);

  // Methods for setting up define-use relationships.
  // Return the same instruction that they are passed.
  template<int I, int T>
      LInstruction* Define(LTemplateInstruction<1, I, T>* instr,
                           LUnallocated* result);
  template<int I, int T>
      LInstruction* Define(LTemplateInstruction<1, I, T>* instr);
  template<int I, int T>
      LInstruction* DefineAsRegister(LTemplateInstruction<1, I, T>* instr);
  template<int I, int T>
      LInstruction* DefineAsSpilled(LTemplateInstruction<1, I, T>* instr,
                                    int index);
  template<int I, int T>
      LInstruction* DefineSameAsFirst(LTemplateInstruction<1, I, T>* instr);
  template<int I, int T>
      LInstruction* DefineFixed(LTemplateInstruction<1, I, T>* instr,
                                Register reg);
  template<int I, int T>
      LInstruction* DefineFixedDouble(LTemplateInstruction<1, I, T>* instr,
                                      XMMRegister reg);
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
  LInstruction* MarkAsSaveDoubles(LInstruction* instr);

  LInstruction* SetInstructionPendingDeoptimizationEnvironment(
      LInstruction* instr, int ast_id);
  void ClearInstructionPendingDeoptimizationEnvironment();

  LEnvironment* CreateEnvironment(HEnvironment* hydrogen_env);

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
  LInstruction* instruction_pending_deoptimization_environment_;
  int pending_deoptimization_ast_id_;

  DISALLOW_COPY_AND_ASSIGN(LChunkBuilder);
};

#undef DECLARE_HYDROGEN_ACCESSOR
#undef DECLARE_INSTRUCTION
#undef DECLARE_CONCRETE_INSTRUCTION

} }  // namespace v8::int

#endif  // V8_X64_LITHIUM_X64_H_
