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

#ifndef V8_ARM_LITHIUM_ARM_H_
#define V8_ARM_LITHIUM_ARM_H_

#include "hydrogen.h"
#include "lithium-allocator.h"
#include "lithium.h"
#include "safepoint-table.h"
#include "utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class LCodeGen;

#define LITHIUM_CONCRETE_INSTRUCTION_LIST(V)    \
  V(AccessArgumentsAt)                          \
  V(AddI)                                       \
  V(Allocate)                                   \
  V(ApplyArguments)                             \
  V(ArgumentsElements)                          \
  V(ArgumentsLength)                            \
  V(ArithmeticD)                                \
  V(ArithmeticT)                                \
  V(BitI)                                       \
  V(BoundsCheck)                                \
  V(Branch)                                     \
  V(CallConstantFunction)                       \
  V(CallFunction)                               \
  V(CallGlobal)                                 \
  V(CallKeyed)                                  \
  V(CallKnownGlobal)                            \
  V(CallNamed)                                  \
  V(CallNew)                                    \
  V(CallNewArray)                               \
  V(CallRuntime)                                \
  V(CallStub)                                   \
  V(CheckInstanceType)                          \
  V(CheckNonSmi)                                \
  V(CheckMaps)                                  \
  V(CheckMapValue)                              \
  V(CheckSmi)                                   \
  V(CheckValue)                                 \
  V(ClampDToUint8)                              \
  V(ClampIToUint8)                              \
  V(ClampTToUint8)                              \
  V(ClassOfTestAndBranch)                       \
  V(CompareNumericAndBranch)                    \
  V(CmpObjectEqAndBranch)                       \
  V(CmpHoleAndBranch)                           \
  V(CmpMapAndBranch)                            \
  V(CmpT)                                       \
  V(ConstantD)                                  \
  V(ConstantE)                                  \
  V(ConstantI)                                  \
  V(ConstantS)                                  \
  V(ConstantT)                                  \
  V(Context)                                    \
  V(DateField)                                  \
  V(DebugBreak)                                 \
  V(DeclareGlobals)                             \
  V(Deoptimize)                                 \
  V(DivI)                                       \
  V(DoubleToI)                                  \
  V(DoubleToSmi)                                \
  V(Drop)                                       \
  V(DummyUse)                                   \
  V(ElementsKind)                               \
  V(ForInCacheArray)                            \
  V(ForInPrepareMap)                            \
  V(FunctionLiteral)                            \
  V(GetCachedArrayIndex)                        \
  V(GlobalObject)                               \
  V(GlobalReceiver)                             \
  V(Goto)                                       \
  V(HasCachedArrayIndexAndBranch)               \
  V(HasInstanceTypeAndBranch)                   \
  V(InnerAllocatedObject)                       \
  V(InstanceOf)                                 \
  V(InstanceOfKnownGlobal)                      \
  V(InstructionGap)                             \
  V(Integer32ToDouble)                          \
  V(Integer32ToSmi)                             \
  V(InvokeFunction)                             \
  V(IsConstructCallAndBranch)                   \
  V(IsObjectAndBranch)                          \
  V(IsStringAndBranch)                          \
  V(IsSmiAndBranch)                             \
  V(IsUndetectableAndBranch)                    \
  V(Label)                                      \
  V(LazyBailout)                                \
  V(LoadContextSlot)                            \
  V(LoadExternalArrayPointer)                   \
  V(LoadRoot)                                   \
  V(LoadFieldByIndex)                           \
  V(LoadFunctionPrototype)                      \
  V(LoadGlobalCell)                             \
  V(LoadGlobalGeneric)                          \
  V(LoadKeyed)                                  \
  V(LoadKeyedGeneric)                           \
  V(LoadNamedField)                             \
  V(LoadNamedGeneric)                           \
  V(MapEnumLength)                              \
  V(MathAbs)                                    \
  V(MathCos)                                    \
  V(MathExp)                                    \
  V(MathFloor)                                  \
  V(MathFloorOfDiv)                             \
  V(MathLog)                                    \
  V(MathMinMax)                                 \
  V(MathPowHalf)                                \
  V(MathRound)                                  \
  V(MathSin)                                    \
  V(MathSqrt)                                   \
  V(MathTan)                                    \
  V(ModI)                                       \
  V(MulI)                                       \
  V(MultiplyAddD)                               \
  V(MultiplySubD)                               \
  V(NumberTagD)                                 \
  V(NumberTagI)                                 \
  V(NumberTagU)                                 \
  V(NumberUntagD)                               \
  V(OsrEntry)                                   \
  V(OuterContext)                               \
  V(Parameter)                                  \
  V(Power)                                      \
  V(PushArgument)                               \
  V(Random)                                     \
  V(RegExpLiteral)                              \
  V(Return)                                     \
  V(SeqStringSetChar)                           \
  V(ShiftI)                                     \
  V(SmiTag)                                     \
  V(SmiUntag)                                   \
  V(StackCheck)                                 \
  V(StoreCodeEntry)                             \
  V(StoreContextSlot)                           \
  V(StoreGlobalCell)                            \
  V(StoreGlobalGeneric)                         \
  V(StoreKeyed)                                 \
  V(StoreKeyedGeneric)                          \
  V(StoreNamedField)                            \
  V(StoreNamedGeneric)                          \
  V(StringAdd)                                  \
  V(StringCharCodeAt)                           \
  V(StringCharFromCode)                         \
  V(StringCompareAndBranch)                     \
  V(SubI)                                       \
  V(RSubI)                                      \
  V(TaggedToI)                                  \
  V(ThisFunction)                               \
  V(Throw)                                      \
  V(ToFastProperties)                           \
  V(TransitionElementsKind)                     \
  V(TrapAllocationMemento)                      \
  V(Typeof)                                     \
  V(TypeofIsAndBranch)                          \
  V(Uint32ToDouble)                             \
  V(Uint32ToSmi)                                \
  V(UnknownOSRValue)                            \
  V(ValueOf)                                    \
  V(WrapReceiver)


#define DECLARE_CONCRETE_INSTRUCTION(type, mnemonic)                        \
  virtual Opcode opcode() const V8_FINAL V8_OVERRIDE {                      \
    return LInstruction::k##type;                                           \
  }                                                                         \
  virtual void CompileToNative(LCodeGen* generator) V8_FINAL V8_OVERRIDE;   \
  virtual const char* Mnemonic() const V8_FINAL V8_OVERRIDE {               \
    return mnemonic;                                                        \
  }                                                                         \
  static L##type* cast(LInstruction* instr) {                               \
    ASSERT(instr->Is##type());                                              \
    return reinterpret_cast<L##type*>(instr);                               \
  }


#define DECLARE_HYDROGEN_ACCESSOR(type)     \
  H##type* hydrogen() const {               \
    return H##type::cast(hydrogen_value()); \
  }


class LInstruction : public ZoneObject {
 public:
  LInstruction()
      : environment_(NULL),
        hydrogen_value_(NULL),
        bit_field_(IsCallBits::encode(false)) {
  }

  virtual ~LInstruction() {}

  virtual void CompileToNative(LCodeGen* generator) = 0;
  virtual const char* Mnemonic() const = 0;
  virtual void PrintTo(StringStream* stream);
  virtual void PrintDataTo(StringStream* stream);
  virtual void PrintOutputOperandTo(StringStream* stream);

  enum Opcode {
    // Declare a unique enum value for each instruction.
#define DECLARE_OPCODE(type) k##type,
    LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_OPCODE)
    kNumberOfInstructions
#undef DECLARE_OPCODE
  };

  virtual Opcode opcode() const = 0;

  // Declare non-virtual type testers for all leaf IR classes.
#define DECLARE_PREDICATE(type) \
  bool Is##type() const { return opcode() == k##type; }
  LITHIUM_CONCRETE_INSTRUCTION_LIST(DECLARE_PREDICATE)
#undef DECLARE_PREDICATE

  // Declare virtual predicates for instructions that don't have
  // an opcode.
  virtual bool IsGap() const { return false; }

  virtual bool IsControl() const { return false; }

  void set_environment(LEnvironment* env) { environment_ = env; }
  LEnvironment* environment() const { return environment_; }
  bool HasEnvironment() const { return environment_ != NULL; }

  void set_pointer_map(LPointerMap* p) { pointer_map_.set(p); }
  LPointerMap* pointer_map() const { return pointer_map_.get(); }
  bool HasPointerMap() const { return pointer_map_.is_set(); }

  void set_hydrogen_value(HValue* value) { hydrogen_value_ = value; }
  HValue* hydrogen_value() const { return hydrogen_value_; }

  virtual void SetDeferredLazyDeoptimizationEnvironment(LEnvironment* env) { }

  void MarkAsCall() { bit_field_ = IsCallBits::update(bit_field_, true); }
  bool IsCall() const { return IsCallBits::decode(bit_field_); }

  // Interface to the register allocator and iterators.
  bool ClobbersTemps() const { return IsCall(); }
  bool ClobbersRegisters() const { return IsCall(); }
  virtual bool ClobbersDoubleRegisters() const { return IsCall(); }

  // Interface to the register allocator and iterators.
  bool IsMarkedAsCall() const { return IsCall(); }

  virtual bool HasResult() const = 0;
  virtual LOperand* result() const = 0;

  LOperand* FirstInput() { return InputAt(0); }
  LOperand* Output() { return HasResult() ? result() : NULL; }

  virtual bool HasInterestingComment(LCodeGen* gen) const { return true; }

#ifdef DEBUG
  void VerifyCall();
#endif

 private:
  // Iterator support.
  friend class InputIterator;
  virtual int InputCount() = 0;
  virtual LOperand* InputAt(int i) = 0;

  friend class TempIterator;
  virtual int TempCount() = 0;
  virtual LOperand* TempAt(int i) = 0;

  class IsCallBits: public BitField<bool, 0, 1> {};

  LEnvironment* environment_;
  SetOncePointer<LPointerMap> pointer_map_;
  HValue* hydrogen_value_;
  int bit_field_;
};


// R = number of result operands (0 or 1).
// I = number of input operands.
// T = number of temporary operands.
template<int R, int I, int T>
class LTemplateInstruction : public LInstruction {
 public:
  // Allow 0 or 1 output operands.
  STATIC_ASSERT(R == 0 || R == 1);
  virtual bool HasResult() const V8_FINAL V8_OVERRIDE {
    return R != 0 && result() != NULL;
  }
  void set_result(LOperand* operand) { results_[0] = operand; }
  LOperand* result() const { return results_[0]; }

 protected:
  EmbeddedContainer<LOperand*, R> results_;
  EmbeddedContainer<LOperand*, I> inputs_;
  EmbeddedContainer<LOperand*, T> temps_;

 private:
  virtual int InputCount() V8_FINAL V8_OVERRIDE { return I; }
  virtual LOperand* InputAt(int i) V8_FINAL V8_OVERRIDE { return inputs_[i]; }

  virtual int TempCount() V8_FINAL V8_OVERRIDE { return T; }
  virtual LOperand* TempAt(int i) V8_FINAL V8_OVERRIDE { return temps_[i]; }
};


class LGap : public LTemplateInstruction<0, 0, 0> {
 public:
  explicit LGap(HBasicBlock* block)
      : block_(block) {
    parallel_moves_[BEFORE] = NULL;
    parallel_moves_[START] = NULL;
    parallel_moves_[END] = NULL;
    parallel_moves_[AFTER] = NULL;
  }

  // Can't use the DECLARE-macro here because of sub-classes.
  virtual bool IsGap() const V8_OVERRIDE { return true; }
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
  static LGap* cast(LInstruction* instr) {
    ASSERT(instr->IsGap());
    return reinterpret_cast<LGap*>(instr);
  }

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

  LParallelMove* GetOrCreateParallelMove(InnerPosition pos, Zone* zone)  {
    if (parallel_moves_[pos] == NULL) {
      parallel_moves_[pos] = new(zone) LParallelMove(zone);
    }
    return parallel_moves_[pos];
  }

  LParallelMove* GetParallelMove(InnerPosition pos)  {
    return parallel_moves_[pos];
  }

 private:
  LParallelMove* parallel_moves_[LAST_INNER_POSITION + 1];
  HBasicBlock* block_;
};


class LInstructionGap V8_FINAL : public LGap {
 public:
  explicit LInstructionGap(HBasicBlock* block) : LGap(block) { }

  virtual bool HasInterestingComment(LCodeGen* gen) const V8_OVERRIDE {
    return !IsRedundant();
  }

  DECLARE_CONCRETE_INSTRUCTION(InstructionGap, "gap")
};


class LGoto V8_FINAL : public LTemplateInstruction<0, 0, 0> {
 public:
  explicit LGoto(HBasicBlock* block) : block_(block) { }

  virtual bool HasInterestingComment(LCodeGen* gen) const V8_OVERRIDE;
  DECLARE_CONCRETE_INSTRUCTION(Goto, "goto")
  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
  virtual bool IsControl() const V8_OVERRIDE { return true; }

  int block_id() const { return block_->block_id(); }

 private:
  HBasicBlock* block_;
};


class LLazyBailout V8_FINAL : public LTemplateInstruction<0, 0, 0> {
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


class LDummyUse V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LDummyUse(LOperand* value) {
    inputs_[0] = value;
  }
  DECLARE_CONCRETE_INSTRUCTION(DummyUse, "dummy-use")
};


class LDeoptimize V8_FINAL : public LTemplateInstruction<0, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Deoptimize, "deoptimize")
  DECLARE_HYDROGEN_ACCESSOR(Deoptimize)
};


class LLabel V8_FINAL : public LGap {
 public:
  explicit LLabel(HBasicBlock* block)
      : LGap(block), replacement_(NULL) { }

  virtual bool HasInterestingComment(LCodeGen* gen) const V8_OVERRIDE {
    return false;
  }
  DECLARE_CONCRETE_INSTRUCTION(Label, "label")

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  int block_id() const { return block()->block_id(); }
  bool is_loop_header() const { return block()->IsLoopHeader(); }
  bool is_osr_entry() const { return block()->is_osr_entry(); }
  Label* label() { return &label_; }
  LLabel* replacement() const { return replacement_; }
  void set_replacement(LLabel* label) { replacement_ = label; }
  bool HasReplacement() const { return replacement_ != NULL; }

 private:
  Label label_;
  LLabel* replacement_;
};


class LParameter V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  virtual bool HasInterestingComment(LCodeGen* gen) const { return false; }
  DECLARE_CONCRETE_INSTRUCTION(Parameter, "parameter")
};


class LCallStub V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCallStub(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CallStub, "call-stub")
  DECLARE_HYDROGEN_ACCESSOR(CallStub)

  TranscendentalCache::Type transcendental_type() {
    return hydrogen()->transcendental_type();
  }
};


class LUnknownOSRValue V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  virtual bool HasInterestingComment(LCodeGen* gen) const V8_OVERRIDE {
    return false;
  }
  DECLARE_CONCRETE_INSTRUCTION(UnknownOSRValue, "unknown-osr-value")
};


template<int I, int T>
class LControlInstruction : public LTemplateInstruction<0, I, T> {
 public:
  LControlInstruction() : false_label_(NULL), true_label_(NULL) { }

  virtual bool IsControl() const V8_FINAL V8_OVERRIDE { return true; }

  int SuccessorCount() { return hydrogen()->SuccessorCount(); }
  HBasicBlock* SuccessorAt(int i) { return hydrogen()->SuccessorAt(i); }

  int TrueDestination(LChunk* chunk) {
    return chunk->LookupDestination(true_block_id());
  }
  int FalseDestination(LChunk* chunk) {
    return chunk->LookupDestination(false_block_id());
  }

  Label* TrueLabel(LChunk* chunk) {
    if (true_label_ == NULL) {
      true_label_ = chunk->GetAssemblyLabel(TrueDestination(chunk));
    }
    return true_label_;
  }
  Label* FalseLabel(LChunk* chunk) {
    if (false_label_ == NULL) {
      false_label_ = chunk->GetAssemblyLabel(FalseDestination(chunk));
    }
    return false_label_;
  }

 protected:
  int true_block_id() { return SuccessorAt(0)->block_id(); }
  int false_block_id() { return SuccessorAt(1)->block_id(); }

 private:
  HControlInstruction* hydrogen() {
    return HControlInstruction::cast(this->hydrogen_value());
  }

  Label* false_label_;
  Label* true_label_;
};


class LWrapReceiver V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LWrapReceiver(LOperand* receiver, LOperand* function) {
    inputs_[0] = receiver;
    inputs_[1] = function;
  }

  DECLARE_CONCRETE_INSTRUCTION(WrapReceiver, "wrap-receiver")

  LOperand* receiver() { return inputs_[0]; }
  LOperand* function() { return inputs_[1]; }
};


class LApplyArguments V8_FINAL : public LTemplateInstruction<1, 4, 0> {
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


class LAccessArgumentsAt V8_FINAL : public LTemplateInstruction<1, 3, 0> {
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

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LArgumentsLength V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LArgumentsLength(LOperand* elements) {
    inputs_[0] = elements;
  }

  LOperand* elements() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength, "arguments-length")
};


class LArgumentsElements V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements, "arguments-elements")
  DECLARE_HYDROGEN_ACCESSOR(ArgumentsElements)
};


class LModI V8_FINAL : public LTemplateInstruction<1, 2, 2> {
 public:
  LModI(LOperand* left,
        LOperand* right,
        LOperand* temp = NULL,
        LOperand* temp2 = NULL) {
    inputs_[0] = left;
    inputs_[1] = right;
    temps_[0] = temp;
    temps_[1] = temp2;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }
  LOperand* temp2() { return temps_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(ModI, "mod-i")
  DECLARE_HYDROGEN_ACCESSOR(Mod)
};


class LDivI V8_FINAL : public LTemplateInstruction<1, 2, 1> {
 public:
  LDivI(LOperand* left, LOperand* right, LOperand* temp) {
    inputs_[0] = left;
    inputs_[1] = right;
    temps_[0] = temp;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(DivI, "div-i")
  DECLARE_HYDROGEN_ACCESSOR(Div)
};


class LMathFloorOfDiv V8_FINAL : public LTemplateInstruction<1, 2, 1> {
 public:
  LMathFloorOfDiv(LOperand* left,
                  LOperand* right,
                  LOperand* temp = NULL) {
    inputs_[0] = left;
    inputs_[1] = right;
    temps_[0] = temp;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathFloorOfDiv, "math-floor-of-div")
  DECLARE_HYDROGEN_ACCESSOR(MathFloorOfDiv)
};


class LMulI V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LMulI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(MulI, "mul-i")
  DECLARE_HYDROGEN_ACCESSOR(Mul)
};


// Instruction for computing multiplier * multiplicand + addend.
class LMultiplyAddD V8_FINAL : public LTemplateInstruction<1, 3, 0> {
 public:
  LMultiplyAddD(LOperand* addend, LOperand* multiplier,
                LOperand* multiplicand) {
    inputs_[0] = addend;
    inputs_[1] = multiplier;
    inputs_[2] = multiplicand;
  }

  LOperand* addend() { return inputs_[0]; }
  LOperand* multiplier() { return inputs_[1]; }
  LOperand* multiplicand() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(MultiplyAddD, "multiply-add-d")
};


// Instruction for computing minuend - multiplier * multiplicand.
class LMultiplySubD V8_FINAL : public LTemplateInstruction<1, 3, 0> {
 public:
  LMultiplySubD(LOperand* minuend, LOperand* multiplier,
                LOperand* multiplicand) {
    inputs_[0] = minuend;
    inputs_[1] = multiplier;
    inputs_[2] = multiplicand;
  }

  LOperand* minuend() { return inputs_[0]; }
  LOperand* multiplier() { return inputs_[1]; }
  LOperand* multiplicand() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(MultiplySubD, "multiply-sub-d")
};


class LDebugBreak V8_FINAL : public LTemplateInstruction<0, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(DebugBreak, "break")
};


class LCompareNumericAndBranch V8_FINAL : public LControlInstruction<2, 0> {
 public:
  LCompareNumericAndBranch(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CompareNumericAndBranch,
                               "compare-numeric-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(CompareNumericAndBranch)

  Token::Value op() const { return hydrogen()->token(); }
  bool is_double() const {
    return hydrogen()->representation().IsDouble();
  }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LMathFloor V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathFloor(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathFloor, "math-floor")
  DECLARE_HYDROGEN_ACCESSOR(UnaryMathOperation)
};


class LMathRound V8_FINAL : public LTemplateInstruction<1, 1, 1> {
 public:
  LMathRound(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathRound, "math-round")
  DECLARE_HYDROGEN_ACCESSOR(UnaryMathOperation)
};


class LMathAbs V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LMathAbs(LOperand* context, LOperand* value) {
    inputs_[1] = context;
    inputs_[0] = value;
  }

  LOperand* context() { return inputs_[1]; }
  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathAbs, "math-abs")
  DECLARE_HYDROGEN_ACCESSOR(UnaryMathOperation)
};


class LMathLog V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathLog(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathLog, "math-log")
};


class LMathSin V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathSin(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathSin, "math-sin")
};


class LMathCos V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathCos(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathCos, "math-cos")
};


class LMathTan V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathTan(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathTan, "math-tan")
};


class LMathExp V8_FINAL : public LTemplateInstruction<1, 1, 3> {
 public:
  LMathExp(LOperand* value,
           LOperand* double_temp,
           LOperand* temp1,
           LOperand* temp2) {
    inputs_[0] = value;
    temps_[0] = temp1;
    temps_[1] = temp2;
    temps_[2] = double_temp;
    ExternalReference::InitializeMathExpData();
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp1() { return temps_[0]; }
  LOperand* temp2() { return temps_[1]; }
  LOperand* double_temp() { return temps_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(MathExp, "math-exp")
};


class LMathSqrt V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathSqrt(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathSqrt, "math-sqrt")
};


class LMathPowHalf V8_FINAL : public LTemplateInstruction<1, 1, 1> {
 public:
  LMathPowHalf(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathPowHalf, "math-pow-half")
};


class LCmpObjectEqAndBranch V8_FINAL : public LControlInstruction<2, 0> {
 public:
  LCmpObjectEqAndBranch(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CmpObjectEqAndBranch, "cmp-object-eq-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(CompareObjectEqAndBranch)
};


class LCmpHoleAndBranch V8_FINAL : public LControlInstruction<1, 0> {
 public:
  explicit LCmpHoleAndBranch(LOperand* object) {
    inputs_[0] = object;
  }

  LOperand* object() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CmpHoleAndBranch, "cmp-hole-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(CompareHoleAndBranch)
};


class LIsObjectAndBranch V8_FINAL : public LControlInstruction<1, 1> {
 public:
  LIsObjectAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsObjectAndBranch, "is-object-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(IsObjectAndBranch)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LIsStringAndBranch V8_FINAL : public LControlInstruction<1, 1> {
 public:
  LIsStringAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsStringAndBranch, "is-string-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(IsStringAndBranch)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LIsSmiAndBranch V8_FINAL : public LControlInstruction<1, 0> {
 public:
  explicit LIsSmiAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch, "is-smi-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(IsSmiAndBranch)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LIsUndetectableAndBranch V8_FINAL : public LControlInstruction<1, 1> {
 public:
  explicit LIsUndetectableAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsUndetectableAndBranch,
                               "is-undetectable-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(IsUndetectableAndBranch)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LStringCompareAndBranch V8_FINAL : public LControlInstruction<3, 0> {
 public:
  LStringCompareAndBranch(LOperand* context, LOperand* left, LOperand* right) {
    inputs_[0] = context;
    inputs_[1] = left;
    inputs_[2] = right;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* left() { return inputs_[1]; }
  LOperand* right() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(StringCompareAndBranch,
                               "string-compare-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(StringCompareAndBranch)

  Token::Value op() const { return hydrogen()->token(); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LHasInstanceTypeAndBranch V8_FINAL : public LControlInstruction<1, 0> {
 public:
  explicit LHasInstanceTypeAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceTypeAndBranch,
                               "has-instance-type-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(HasInstanceTypeAndBranch)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LGetCachedArrayIndex V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LGetCachedArrayIndex(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(GetCachedArrayIndex, "get-cached-array-index")
  DECLARE_HYDROGEN_ACCESSOR(GetCachedArrayIndex)
};


class LHasCachedArrayIndexAndBranch V8_FINAL
    : public LControlInstruction<1, 0> {
 public:
  explicit LHasCachedArrayIndexAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndexAndBranch,
                               "has-cached-array-index-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(HasCachedArrayIndexAndBranch)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LClassOfTestAndBranch V8_FINAL : public LControlInstruction<1, 1> {
 public:
  LClassOfTestAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch,
                               "class-of-test-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(ClassOfTestAndBranch)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LCmpT V8_FINAL : public LTemplateInstruction<1, 3, 0> {
 public:
  LCmpT(LOperand* context, LOperand* left, LOperand* right) {
    inputs_[0] = context;
    inputs_[1] = left;
    inputs_[2] = right;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* left() { return inputs_[1]; }
  LOperand* right() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(CmpT, "cmp-t")
  DECLARE_HYDROGEN_ACCESSOR(CompareGeneric)

  Token::Value op() const { return hydrogen()->token(); }
};


class LInstanceOf V8_FINAL : public LTemplateInstruction<1, 3, 0> {
 public:
  LInstanceOf(LOperand* context, LOperand* left, LOperand* right) {
    inputs_[0] = context;
    inputs_[1] = left;
    inputs_[2] = right;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* left() { return inputs_[1]; }
  LOperand* right() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOf, "instance-of")
};


class LInstanceOfKnownGlobal V8_FINAL : public LTemplateInstruction<1, 2, 1> {
 public:
  LInstanceOfKnownGlobal(LOperand* context, LOperand* value, LOperand* temp) {
    inputs_[0] = context;
    inputs_[1] = value;
    temps_[0] = temp;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfKnownGlobal,
                               "instance-of-known-global")
  DECLARE_HYDROGEN_ACCESSOR(InstanceOfKnownGlobal)

  Handle<JSFunction> function() const { return hydrogen()->function(); }
  LEnvironment* GetDeferredLazyDeoptimizationEnvironment() {
    return lazy_deopt_env_;
  }
  virtual void SetDeferredLazyDeoptimizationEnvironment(
      LEnvironment* env) V8_OVERRIDE {
    lazy_deopt_env_ = env;
  }

 private:
  LEnvironment* lazy_deopt_env_;
};


class LBoundsCheck V8_FINAL : public LTemplateInstruction<0, 2, 0> {
 public:
  LBoundsCheck(LOperand* index, LOperand* length) {
    inputs_[0] = index;
    inputs_[1] = length;
  }

  LOperand* index() { return inputs_[0]; }
  LOperand* length() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheck, "bounds-check")
  DECLARE_HYDROGEN_ACCESSOR(BoundsCheck)
};


class LBitI V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LBitI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  Token::Value op() const { return hydrogen()->op(); }

  DECLARE_CONCRETE_INSTRUCTION(BitI, "bit-i")
  DECLARE_HYDROGEN_ACCESSOR(Bitwise)
};


class LShiftI V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LShiftI(Token::Value op, LOperand* left, LOperand* right, bool can_deopt)
      : op_(op), can_deopt_(can_deopt) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  Token::Value op() const { return op_; }
  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }
  bool can_deopt() const { return can_deopt_; }

  DECLARE_CONCRETE_INSTRUCTION(ShiftI, "shift-i")

 private:
  Token::Value op_;
  bool can_deopt_;
};


class LSubI V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LSubI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(SubI, "sub-i")
  DECLARE_HYDROGEN_ACCESSOR(Sub)
};


class LRSubI V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LRSubI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(RSubI, "rsub-i")
  DECLARE_HYDROGEN_ACCESSOR(Sub)
};


class LConstantI V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ConstantI, "constant-i")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  int32_t value() const { return hydrogen()->Integer32Value(); }
};


class LConstantS V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ConstantS, "constant-s")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  Smi* value() const { return Smi::FromInt(hydrogen()->Integer32Value()); }
};


class LConstantD V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ConstantD, "constant-d")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  double value() const { return hydrogen()->DoubleValue(); }
};


class LConstantE V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ConstantE, "constant-e")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  ExternalReference value() const {
    return hydrogen()->ExternalReferenceValue();
  }
};


class LConstantT V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ConstantT, "constant-t")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  Handle<Object> value(Isolate* isolate) const {
    return hydrogen()->handle(isolate);
  }
};


class LBranch V8_FINAL : public LControlInstruction<1, 0> {
 public:
  explicit LBranch(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Branch, "branch")
  DECLARE_HYDROGEN_ACCESSOR(Branch)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LCmpMapAndBranch V8_FINAL : public LControlInstruction<1, 1> {
 public:
  LCmpMapAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CmpMapAndBranch, "cmp-map-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(CompareMap)

  Handle<Map> map() const { return hydrogen()->map().handle(); }
};


class LMapEnumLength V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMapEnumLength(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MapEnumLength, "map-enum-length")
};


class LElementsKind V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LElementsKind(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ElementsKind, "elements-kind")
  DECLARE_HYDROGEN_ACCESSOR(ElementsKind)
};


class LValueOf V8_FINAL : public LTemplateInstruction<1, 1, 1> {
 public:
  LValueOf(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ValueOf, "value-of")
  DECLARE_HYDROGEN_ACCESSOR(ValueOf)
};


class LDateField V8_FINAL : public LTemplateInstruction<1, 1, 1> {
 public:
  LDateField(LOperand* date, LOperand* temp, Smi* index) : index_(index) {
    inputs_[0] = date;
    temps_[0] = temp;
  }

  LOperand* date() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }
  Smi* index() const { return index_; }

  DECLARE_CONCRETE_INSTRUCTION(DateField, "date-field")
  DECLARE_HYDROGEN_ACCESSOR(DateField)

 private:
  Smi* index_;
};


class LSeqStringSetChar V8_FINAL : public LTemplateInstruction<1, 3, 0> {
 public:
  LSeqStringSetChar(String::Encoding encoding,
                    LOperand* string,
                    LOperand* index,
                    LOperand* value) : encoding_(encoding) {
    inputs_[0] = string;
    inputs_[1] = index;
    inputs_[2] = value;
  }

  String::Encoding encoding() { return encoding_; }
  LOperand* string() { return inputs_[0]; }
  LOperand* index() { return inputs_[1]; }
  LOperand* value() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(SeqStringSetChar, "seq-string-set-char")
  DECLARE_HYDROGEN_ACCESSOR(SeqStringSetChar)

 private:
  String::Encoding encoding_;
};


class LThrow V8_FINAL : public LTemplateInstruction<0, 2, 0> {
 public:
  LThrow(LOperand* context, LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = value;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(Throw, "throw")
};


class LAddI V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LAddI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(AddI, "add-i")
  DECLARE_HYDROGEN_ACCESSOR(Add)
};


class LMathMinMax V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LMathMinMax(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(MathMinMax, "math-min-max")
  DECLARE_HYDROGEN_ACCESSOR(MathMinMax)
};


class LPower V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LPower(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(Power, "power")
  DECLARE_HYDROGEN_ACCESSOR(Power)
};


class LRandom V8_FINAL : public LTemplateInstruction<1, 1, 3> {
 public:
  LRandom(LOperand* global_object,
          LOperand* scratch,
          LOperand* scratch2,
          LOperand* scratch3) {
    inputs_[0] = global_object;
    temps_[0] = scratch;
    temps_[1] = scratch2;
    temps_[2] = scratch3;
  }

  LOperand* global_object() const { return inputs_[0]; }
  LOperand* scratch() const { return temps_[0]; }
  LOperand* scratch2() const { return temps_[1]; }
  LOperand* scratch3() const { return temps_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(Random, "random")
  DECLARE_HYDROGEN_ACCESSOR(Random)
};


class LArithmeticD V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LArithmeticD(Token::Value op, LOperand* left, LOperand* right)
      : op_(op) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  Token::Value op() const { return op_; }
  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  virtual Opcode opcode() const V8_OVERRIDE {
    return LInstruction::kArithmeticD;
  }
  virtual void CompileToNative(LCodeGen* generator) V8_OVERRIDE;
  virtual const char* Mnemonic() const V8_OVERRIDE;

 private:
  Token::Value op_;
};


class LArithmeticT V8_FINAL : public LTemplateInstruction<1, 3, 0> {
 public:
  LArithmeticT(Token::Value op,
               LOperand* context,
               LOperand* left,
               LOperand* right)
      : op_(op) {
    inputs_[0] = context;
    inputs_[1] = left;
    inputs_[2] = right;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* left() { return inputs_[1]; }
  LOperand* right() { return inputs_[2]; }
  Token::Value op() const { return op_; }

  virtual Opcode opcode() const V8_OVERRIDE {
    return LInstruction::kArithmeticT;
  }
  virtual void CompileToNative(LCodeGen* generator) V8_OVERRIDE;
  virtual const char* Mnemonic() const V8_OVERRIDE;

 private:
  Token::Value op_;
};


class LReturn V8_FINAL : public LTemplateInstruction<0, 3, 0> {
 public:
  LReturn(LOperand* value, LOperand* context, LOperand* parameter_count) {
    inputs_[0] = value;
    inputs_[1] = context;
    inputs_[2] = parameter_count;
  }

  LOperand* value() { return inputs_[0]; }

  bool has_constant_parameter_count() {
    return parameter_count()->IsConstantOperand();
  }
  LConstantOperand* constant_parameter_count() {
    ASSERT(has_constant_parameter_count());
    return LConstantOperand::cast(parameter_count());
  }
  LOperand* parameter_count() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(Return, "return")
};


class LLoadNamedField V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadNamedField(LOperand* object) {
    inputs_[0] = object;
  }

  LOperand* object() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField, "load-named-field")
  DECLARE_HYDROGEN_ACCESSOR(LoadNamedField)
};


class LLoadNamedGeneric V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LLoadNamedGeneric(LOperand* context, LOperand* object) {
    inputs_[0] = context;
    inputs_[1] = object;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedGeneric, "load-named-generic")
  DECLARE_HYDROGEN_ACCESSOR(LoadNamedGeneric)

  Handle<Object> name() const { return hydrogen()->name(); }
};


class LLoadFunctionPrototype V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadFunctionPrototype(LOperand* function) {
    inputs_[0] = function;
  }

  LOperand* function() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype, "load-function-prototype")
  DECLARE_HYDROGEN_ACCESSOR(LoadFunctionPrototype)
};


class LLoadRoot V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(LoadRoot, "load-root")
  DECLARE_HYDROGEN_ACCESSOR(LoadRoot)

  Heap::RootListIndex index() const { return hydrogen()->index(); }
};


class LLoadExternalArrayPointer V8_FINAL
    : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadExternalArrayPointer(LOperand* object) {
    inputs_[0] = object;
  }

  LOperand* object() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadExternalArrayPointer,
                               "load-external-array-pointer")
};


class LLoadKeyed V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LLoadKeyed(LOperand* elements, LOperand* key) {
    inputs_[0] = elements;
    inputs_[1] = key;
  }

  LOperand* elements() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
  ElementsKind elements_kind() const {
    return hydrogen()->elements_kind();
  }
  bool is_external() const {
    return hydrogen()->is_external();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyed, "load-keyed")
  DECLARE_HYDROGEN_ACCESSOR(LoadKeyed)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
  uint32_t additional_index() const { return hydrogen()->index_offset(); }
};


class LLoadKeyedGeneric V8_FINAL : public LTemplateInstruction<1, 3, 0> {
 public:
  LLoadKeyedGeneric(LOperand* context, LOperand* object, LOperand* key) {
    inputs_[0] = context;
    inputs_[1] = object;
    inputs_[2] = key;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }
  LOperand* key() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedGeneric, "load-keyed-generic")
};


class LLoadGlobalCell V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalCell, "load-global-cell")
  DECLARE_HYDROGEN_ACCESSOR(LoadGlobalCell)
};


class LLoadGlobalGeneric V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LLoadGlobalGeneric(LOperand* context, LOperand* global_object) {
    inputs_[0] = context;
    inputs_[1] = global_object;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* global_object() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalGeneric, "load-global-generic")
  DECLARE_HYDROGEN_ACCESSOR(LoadGlobalGeneric)

  Handle<Object> name() const { return hydrogen()->name(); }
  bool for_typeof() const { return hydrogen()->for_typeof(); }
};


class LStoreGlobalCell V8_FINAL : public LTemplateInstruction<0, 1, 1> {
 public:
  LStoreGlobalCell(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobalCell, "store-global-cell")
  DECLARE_HYDROGEN_ACCESSOR(StoreGlobalCell)
};


class LStoreGlobalGeneric V8_FINAL : public LTemplateInstruction<0, 3, 0> {
 public:
  LStoreGlobalGeneric(LOperand* context,
                      LOperand* global_object,
                      LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = global_object;
    inputs_[2] = value;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* global_object() { return inputs_[1]; }
  LOperand* value() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobalGeneric, "store-global-generic")
  DECLARE_HYDROGEN_ACCESSOR(StoreGlobalGeneric)

  Handle<Object> name() const { return hydrogen()->name(); }
  StrictModeFlag strict_mode_flag() { return hydrogen()->strict_mode_flag(); }
};


class LLoadContextSlot V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadContextSlot(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadContextSlot, "load-context-slot")
  DECLARE_HYDROGEN_ACCESSOR(LoadContextSlot)

  int slot_index() { return hydrogen()->slot_index(); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LStoreContextSlot V8_FINAL : public LTemplateInstruction<0, 2, 0> {
 public:
  LStoreContextSlot(LOperand* context, LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = value;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreContextSlot, "store-context-slot")
  DECLARE_HYDROGEN_ACCESSOR(StoreContextSlot)

  int slot_index() { return hydrogen()->slot_index(); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LPushArgument V8_FINAL : public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LPushArgument(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(PushArgument, "push-argument")
};


class LDrop V8_FINAL : public LTemplateInstruction<0, 0, 0> {
 public:
  explicit LDrop(int count) : count_(count) { }

  int count() const { return count_; }

  DECLARE_CONCRETE_INSTRUCTION(Drop, "drop")

 private:
  int count_;
};


class LStoreCodeEntry V8_FINAL: public LTemplateInstruction<0, 1, 1> {
 public:
  LStoreCodeEntry(LOperand* function, LOperand* code_object) {
    inputs_[0] = function;
    temps_[0] = code_object;
  }

  LOperand* function() { return inputs_[0]; }
  LOperand* code_object() { return temps_[0]; }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(StoreCodeEntry, "store-code-entry")
  DECLARE_HYDROGEN_ACCESSOR(StoreCodeEntry)
};


class LInnerAllocatedObject V8_FINAL: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LInnerAllocatedObject(LOperand* base_object) {
    inputs_[0] = base_object;
  }

  LOperand* base_object() { return inputs_[0]; }
  int offset() { return hydrogen()->offset(); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  DECLARE_CONCRETE_INSTRUCTION(InnerAllocatedObject, "sub-allocated-object")
  DECLARE_HYDROGEN_ACCESSOR(InnerAllocatedObject)
};


class LThisFunction V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ThisFunction, "this-function")
  DECLARE_HYDROGEN_ACCESSOR(ThisFunction)
};


class LContext V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Context, "context")
  DECLARE_HYDROGEN_ACCESSOR(Context)
};


class LOuterContext V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LOuterContext(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(OuterContext, "outer-context")
};


class LDeclareGlobals V8_FINAL : public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LDeclareGlobals(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(DeclareGlobals, "declare-globals")
  DECLARE_HYDROGEN_ACCESSOR(DeclareGlobals)
};


class LGlobalObject V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LGlobalObject(LOperand* context) {
    inputs_[0] = context;
  }

  DECLARE_CONCRETE_INSTRUCTION(GlobalObject, "global-object")

  LOperand* context() { return inputs_[0]; }
};


class LGlobalReceiver V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LGlobalReceiver(LOperand* global_object) {
    inputs_[0] = global_object;
  }

  LOperand* global_object() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(GlobalReceiver, "global-receiver")
};


class LCallConstantFunction V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallConstantFunction, "call-constant-function")
  DECLARE_HYDROGEN_ACCESSOR(CallConstantFunction)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<JSFunction> function() { return hydrogen()->function(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LInvokeFunction V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LInvokeFunction(LOperand* context, LOperand* function) {
    inputs_[0] = context;
    inputs_[1] = function;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* function() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(InvokeFunction, "invoke-function")
  DECLARE_HYDROGEN_ACCESSOR(InvokeFunction)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallKeyed V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LCallKeyed(LOperand* context, LOperand* key) {
    inputs_[0] = context;
    inputs_[1] = key;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CallKeyed, "call-keyed")
  DECLARE_HYDROGEN_ACCESSOR(CallKeyed)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  int arity() const { return hydrogen()->argument_count() - 1; }
};



class LCallNamed V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCallNamed(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CallNamed, "call-named")
  DECLARE_HYDROGEN_ACCESSOR(CallNamed)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<String> name() const { return hydrogen()->name(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallFunction V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LCallFunction(LOperand* context, LOperand* function) {
    inputs_[0] = context;
    inputs_[1] = function;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* function() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CallFunction, "call-function")
  DECLARE_HYDROGEN_ACCESSOR(CallFunction)

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallGlobal V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCallGlobal(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CallGlobal, "call-global")
  DECLARE_HYDROGEN_ACCESSOR(CallGlobal)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<String> name() const {return hydrogen()->name(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallKnownGlobal V8_FINAL : public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(CallKnownGlobal, "call-known-global")
  DECLARE_HYDROGEN_ACCESSOR(CallKnownGlobal)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  int arity() const { return hydrogen()->argument_count() - 1;  }
};


class LCallNew V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LCallNew(LOperand* context, LOperand* constructor) {
    inputs_[0] = context;
    inputs_[1] = constructor;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* constructor() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CallNew, "call-new")
  DECLARE_HYDROGEN_ACCESSOR(CallNew)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallNewArray V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LCallNewArray(LOperand* context, LOperand* constructor) {
    inputs_[0] = context;
    inputs_[1] = constructor;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* constructor() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CallNewArray, "call-new-array")
  DECLARE_HYDROGEN_ACCESSOR(CallNewArray)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallRuntime V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCallRuntime(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CallRuntime, "call-runtime")
  DECLARE_HYDROGEN_ACCESSOR(CallRuntime)

  virtual bool ClobbersDoubleRegisters() const V8_OVERRIDE {
    return save_doubles() == kDontSaveFPRegs;
  }

  const Runtime::Function* function() const { return hydrogen()->function(); }
  int arity() const { return hydrogen()->argument_count(); }
  SaveFPRegsMode save_doubles() const { return hydrogen()->save_doubles(); }
};


class LInteger32ToDouble V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LInteger32ToDouble(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Integer32ToDouble, "int32-to-double")
};


class LInteger32ToSmi V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LInteger32ToSmi(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Integer32ToSmi, "int32-to-smi")
  DECLARE_HYDROGEN_ACCESSOR(Change)
};


class LUint32ToDouble V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LUint32ToDouble(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Uint32ToDouble, "uint32-to-double")
};


class LUint32ToSmi V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LUint32ToSmi(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Uint32ToSmi, "uint32-to-smi")
  DECLARE_HYDROGEN_ACCESSOR(Change)
};


class LNumberTagI V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LNumberTagI(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagI, "number-tag-i")
};


class LNumberTagU V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LNumberTagU(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagU, "number-tag-u")
};


class LNumberTagD V8_FINAL : public LTemplateInstruction<1, 1, 2> {
 public:
  LNumberTagD(LOperand* value, LOperand* temp, LOperand* temp2) {
    inputs_[0] = value;
    temps_[0] = temp;
    temps_[1] = temp2;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }
  LOperand* temp2() { return temps_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagD, "number-tag-d")
  DECLARE_HYDROGEN_ACCESSOR(Change)
};


class LDoubleToSmi V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LDoubleToSmi(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(DoubleToSmi, "double-to-smi")
  DECLARE_HYDROGEN_ACCESSOR(UnaryOperation)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
};


// Sometimes truncating conversion from a tagged value to an int32.
class LDoubleToI V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LDoubleToI(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(DoubleToI, "double-to-i")
  DECLARE_HYDROGEN_ACCESSOR(UnaryOperation)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
};


// Truncating conversion from a tagged value to an int32.
class LTaggedToI V8_FINAL : public LTemplateInstruction<1, 1, 2> {
 public:
  LTaggedToI(LOperand* value,
             LOperand* temp,
             LOperand* temp2) {
    inputs_[0] = value;
    temps_[0] = temp;
    temps_[1] = temp2;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }
  LOperand* temp2() { return temps_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(TaggedToI, "tagged-to-i")
  DECLARE_HYDROGEN_ACCESSOR(Change)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
};


class LSmiTag V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LSmiTag(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(SmiTag, "smi-tag")
};


class LNumberUntagD V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LNumberUntagD(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(NumberUntagD, "double-untag")
  DECLARE_HYDROGEN_ACCESSOR(Change)
};


class LSmiUntag V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  LSmiUntag(LOperand* value, bool needs_check)
      : needs_check_(needs_check) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }
  bool needs_check() const { return needs_check_; }

  DECLARE_CONCRETE_INSTRUCTION(SmiUntag, "smi-untag")

 private:
  bool needs_check_;
};


class LStoreNamedField V8_FINAL : public LTemplateInstruction<0, 2, 1> {
 public:
  LStoreNamedField(LOperand* object, LOperand* value, LOperand* temp) {
    inputs_[0] = object;
    inputs_[1] = value;
    temps_[0] = temp;
  }

  LOperand* object() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField, "store-named-field")
  DECLARE_HYDROGEN_ACCESSOR(StoreNamedField)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<Map> transition() const { return hydrogen()->transition_map(); }
  Representation representation() const {
    return hydrogen()->field_representation();
  }
};


class LStoreNamedGeneric V8_FINAL : public LTemplateInstruction<0, 3, 0> {
 public:
  LStoreNamedGeneric(LOperand* context, LOperand* object, LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = object;
    inputs_[2] = value;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }
  LOperand* value() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedGeneric, "store-named-generic")
  DECLARE_HYDROGEN_ACCESSOR(StoreNamedGeneric)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<Object> name() const { return hydrogen()->name(); }
  StrictModeFlag strict_mode_flag() { return hydrogen()->strict_mode_flag(); }
};


class LStoreKeyed V8_FINAL : public LTemplateInstruction<0, 3, 0> {
 public:
  LStoreKeyed(LOperand* object, LOperand* key, LOperand* value) {
    inputs_[0] = object;
    inputs_[1] = key;
    inputs_[2] = value;
  }

  bool is_external() const { return hydrogen()->is_external(); }
  LOperand* elements() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
  LOperand* value() { return inputs_[2]; }
  ElementsKind elements_kind() const {
    return hydrogen()->elements_kind();
  }

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyed, "store-keyed")
  DECLARE_HYDROGEN_ACCESSOR(StoreKeyed)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
  bool NeedsCanonicalization() {
    if (hydrogen()->value()->IsAdd() || hydrogen()->value()->IsSub() ||
        hydrogen()->value()->IsMul() || hydrogen()->value()->IsDiv()) {
      return false;
    }
    return hydrogen()->NeedsCanonicalization();
  }
  uint32_t additional_index() const { return hydrogen()->index_offset(); }
};


class LStoreKeyedGeneric V8_FINAL : public LTemplateInstruction<0, 4, 0> {
 public:
  LStoreKeyedGeneric(LOperand* context,
                     LOperand* obj,
                     LOperand* key,
                     LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = obj;
    inputs_[2] = key;
    inputs_[3] = value;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }
  LOperand* key() { return inputs_[2]; }
  LOperand* value() { return inputs_[3]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedGeneric, "store-keyed-generic")
  DECLARE_HYDROGEN_ACCESSOR(StoreKeyedGeneric)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  StrictModeFlag strict_mode_flag() { return hydrogen()->strict_mode_flag(); }
};


class LTransitionElementsKind V8_FINAL : public LTemplateInstruction<0, 2, 1> {
 public:
  LTransitionElementsKind(LOperand* object,
                          LOperand* context,
                          LOperand* new_map_temp) {
    inputs_[0] = object;
    inputs_[1] = context;
    temps_[0] = new_map_temp;
  }

  LOperand* context() { return inputs_[1]; }
  LOperand* object() { return inputs_[0]; }
  LOperand* new_map_temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(TransitionElementsKind,
                               "transition-elements-kind")
  DECLARE_HYDROGEN_ACCESSOR(TransitionElementsKind)

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;

  Handle<Map> original_map() { return hydrogen()->original_map().handle(); }
  Handle<Map> transitioned_map() {
    return hydrogen()->transitioned_map().handle();
  }
  ElementsKind from_kind() { return hydrogen()->from_kind(); }
  ElementsKind to_kind() { return hydrogen()->to_kind(); }
};


class LTrapAllocationMemento V8_FINAL : public LTemplateInstruction<0, 1, 1> {
 public:
  LTrapAllocationMemento(LOperand* object,
                         LOperand* temp) {
    inputs_[0] = object;
    temps_[0] = temp;
  }

  LOperand* object() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(TrapAllocationMemento,
                               "trap-allocation-memento")
};


class LStringAdd V8_FINAL : public LTemplateInstruction<1, 3, 0> {
 public:
  LStringAdd(LOperand* context, LOperand* left, LOperand* right) {
    inputs_[0] = context;
    inputs_[1] = left;
    inputs_[2] = right;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* left() { return inputs_[1]; }
  LOperand* right() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(StringAdd, "string-add")
  DECLARE_HYDROGEN_ACCESSOR(StringAdd)
};



class LStringCharCodeAt V8_FINAL : public LTemplateInstruction<1, 3, 0> {
 public:
  LStringCharCodeAt(LOperand* context, LOperand* string, LOperand* index) {
    inputs_[0] = context;
    inputs_[1] = string;
    inputs_[2] = index;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* string() { return inputs_[1]; }
  LOperand* index() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(StringCharCodeAt, "string-char-code-at")
  DECLARE_HYDROGEN_ACCESSOR(StringCharCodeAt)
};


class LStringCharFromCode V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  explicit LStringCharFromCode(LOperand* context, LOperand* char_code) {
    inputs_[0] = context;
    inputs_[1] = char_code;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* char_code() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(StringCharFromCode, "string-char-from-code")
  DECLARE_HYDROGEN_ACCESSOR(StringCharFromCode)
};


class LCheckValue V8_FINAL : public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckValue(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckValue, "check-value")
  DECLARE_HYDROGEN_ACCESSOR(CheckValue)
};


class LCheckInstanceType V8_FINAL : public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckInstanceType(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckInstanceType, "check-instance-type")
  DECLARE_HYDROGEN_ACCESSOR(CheckInstanceType)
};


class LCheckMaps V8_FINAL : public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckMaps(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckMaps, "check-maps")
  DECLARE_HYDROGEN_ACCESSOR(CheckMaps)
};


class LCheckSmi V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCheckSmi(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckSmi, "check-smi")
};


class LCheckNonSmi V8_FINAL : public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckNonSmi(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckNonSmi, "check-non-smi")
  DECLARE_HYDROGEN_ACCESSOR(CheckHeapObject)
};


class LClampDToUint8 V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LClampDToUint8(LOperand* unclamped) {
    inputs_[0] = unclamped;
  }

  LOperand* unclamped() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ClampDToUint8, "clamp-d-to-uint8")
};


class LClampIToUint8 V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LClampIToUint8(LOperand* unclamped) {
    inputs_[0] = unclamped;
  }

  LOperand* unclamped() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ClampIToUint8, "clamp-i-to-uint8")
};


class LClampTToUint8 V8_FINAL : public LTemplateInstruction<1, 1, 1> {
 public:
  LClampTToUint8(LOperand* unclamped, LOperand* temp) {
    inputs_[0] = unclamped;
    temps_[0] = temp;
  }

  LOperand* unclamped() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ClampTToUint8, "clamp-t-to-uint8")
};


class LAllocate V8_FINAL : public LTemplateInstruction<1, 2, 2> {
 public:
  LAllocate(LOperand* context,
            LOperand* size,
            LOperand* temp1,
            LOperand* temp2) {
    inputs_[0] = context;
    inputs_[1] = size;
    temps_[0] = temp1;
    temps_[1] = temp2;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* size() { return inputs_[1]; }
  LOperand* temp1() { return temps_[0]; }
  LOperand* temp2() { return temps_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(Allocate, "allocate")
  DECLARE_HYDROGEN_ACCESSOR(Allocate)
};


class LRegExpLiteral V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LRegExpLiteral(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(RegExpLiteral, "regexp-literal")
  DECLARE_HYDROGEN_ACCESSOR(RegExpLiteral)
};


class LFunctionLiteral V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LFunctionLiteral(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(FunctionLiteral, "function-literal")
  DECLARE_HYDROGEN_ACCESSOR(FunctionLiteral)
};


class LToFastProperties V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LToFastProperties(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ToFastProperties, "to-fast-properties")
  DECLARE_HYDROGEN_ACCESSOR(ToFastProperties)
};


class LTypeof V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LTypeof(LOperand* context, LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = value;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(Typeof, "typeof")
};


class LTypeofIsAndBranch V8_FINAL : public LControlInstruction<1, 0> {
 public:
  explicit LTypeofIsAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(TypeofIsAndBranch, "typeof-is-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(TypeofIsAndBranch)

  Handle<String> type_literal() { return hydrogen()->type_literal(); }

  virtual void PrintDataTo(StringStream* stream) V8_OVERRIDE;
};


class LIsConstructCallAndBranch V8_FINAL : public LControlInstruction<0, 1> {
 public:
  explicit LIsConstructCallAndBranch(LOperand* temp) {
    temps_[0] = temp;
  }

  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsConstructCallAndBranch,
                               "is-construct-call-and-branch")
};


class LOsrEntry V8_FINAL : public LTemplateInstruction<0, 0, 0> {
 public:
  LOsrEntry() {}

  virtual bool HasInterestingComment(LCodeGen* gen) const V8_OVERRIDE {
    return false;
  }
  DECLARE_CONCRETE_INSTRUCTION(OsrEntry, "osr-entry")
};


class LStackCheck V8_FINAL : public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LStackCheck(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(StackCheck, "stack-check")
  DECLARE_HYDROGEN_ACCESSOR(StackCheck)

  Label* done_label() { return &done_label_; }

 private:
  Label done_label_;
};


class LForInPrepareMap V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LForInPrepareMap(LOperand* context, LOperand* object) {
    inputs_[0] = context;
    inputs_[1] = object;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(ForInPrepareMap, "for-in-prepare-map")
};


class LForInCacheArray V8_FINAL : public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LForInCacheArray(LOperand* map) {
    inputs_[0] = map;
  }

  LOperand* map() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ForInCacheArray, "for-in-cache-array")

  int idx() {
    return HForInCacheArray::cast(this->hydrogen_value())->idx();
  }
};


class LCheckMapValue V8_FINAL : public LTemplateInstruction<0, 2, 0> {
 public:
  LCheckMapValue(LOperand* value, LOperand* map) {
    inputs_[0] = value;
    inputs_[1] = map;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* map() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckMapValue, "check-map-value")
};


class LLoadFieldByIndex V8_FINAL : public LTemplateInstruction<1, 2, 0> {
 public:
  LLoadFieldByIndex(LOperand* object, LOperand* index) {
    inputs_[0] = object;
    inputs_[1] = index;
  }

  LOperand* object() { return inputs_[0]; }
  LOperand* index() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadFieldByIndex, "load-field-by-index")
};


class LChunkBuilder;
class LPlatformChunk V8_FINAL : public LChunk {
 public:
  LPlatformChunk(CompilationInfo* info, HGraph* graph)
      : LChunk(info, graph) { }

  int GetNextSpillIndex(RegisterKind kind);
  LOperand* GetNextSpillSlot(RegisterKind kind);
};


class LChunkBuilder V8_FINAL BASE_EMBEDDED {
 public:
  LChunkBuilder(CompilationInfo* info, HGraph* graph, LAllocator* allocator)
      : chunk_(NULL),
        info_(info),
        graph_(graph),
        zone_(graph->zone()),
        status_(UNUSED),
        current_instruction_(NULL),
        current_block_(NULL),
        next_block_(NULL),
        argument_count_(0),
        allocator_(allocator),
        position_(RelocInfo::kNoPosition),
        instruction_pending_deoptimization_environment_(NULL),
        pending_deoptimization_ast_id_(BailoutId::None()) { }

  // Build the sequence for the graph.
  LPlatformChunk* Build();

  LInstruction* CheckElideControlInstruction(HControlInstruction* instr);

  // Declare methods that deal with the individual node types.
#define DECLARE_DO(type) LInstruction* Do##type(H##type* node);
  HYDROGEN_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

  LInstruction* DoMultiplyAdd(HMul* mul, HValue* addend);
  LInstruction* DoMultiplySub(HValue* minuend, HMul* mul);
  LInstruction* DoRSub(HSub* instr);

  static bool HasMagicNumberForDivisor(int32_t divisor);
  static HValue* SimplifiedDivisorForMathFloorOfDiv(HValue* val);

  LInstruction* DoMathFloor(HUnaryMathOperation* instr);
  LInstruction* DoMathRound(HUnaryMathOperation* instr);
  LInstruction* DoMathAbs(HUnaryMathOperation* instr);
  LInstruction* DoMathLog(HUnaryMathOperation* instr);
  LInstruction* DoMathSin(HUnaryMathOperation* instr);
  LInstruction* DoMathCos(HUnaryMathOperation* instr);
  LInstruction* DoMathTan(HUnaryMathOperation* instr);
  LInstruction* DoMathExp(HUnaryMathOperation* instr);
  LInstruction* DoMathSqrt(HUnaryMathOperation* instr);
  LInstruction* DoMathPowHalf(HUnaryMathOperation* instr);

 private:
  enum Status {
    UNUSED,
    BUILDING,
    DONE,
    ABORTED
  };

  LPlatformChunk* chunk() const { return chunk_; }
  CompilationInfo* info() const { return info_; }
  HGraph* graph() const { return graph_; }
  Zone* zone() const { return zone_; }

  bool is_unused() const { return status_ == UNUSED; }
  bool is_building() const { return status_ == BUILDING; }
  bool is_done() const { return status_ == DONE; }
  bool is_aborted() const { return status_ == ABORTED; }

  void Abort(BailoutReason reason);

  // Methods for getting operands for Use / Define / Temp.
  LUnallocated* ToUnallocated(Register reg);
  LUnallocated* ToUnallocated(DoubleRegister reg);

  // Methods for setting up define-use relationships.
  MUST_USE_RESULT LOperand* Use(HValue* value, LUnallocated* operand);
  MUST_USE_RESULT LOperand* UseFixed(HValue* value, Register fixed_register);
  MUST_USE_RESULT LOperand* UseFixedDouble(HValue* value,
                                           DoubleRegister fixed_register);

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

  // An input operand in a constant operand.
  MUST_USE_RESULT LOperand* UseConstant(HValue* value);

  // An input operand in register, stack slot or a constant operand.
  // Will not be moved to a register even if one is freely available.
  MUST_USE_RESULT LOperand* UseAny(HValue* value);

  // Temporary operand that must be in a register.
  MUST_USE_RESULT LUnallocated* TempRegister();
  MUST_USE_RESULT LOperand* FixedTemp(Register reg);
  MUST_USE_RESULT LOperand* FixedTemp(DoubleRegister reg);

  // Methods for setting up define-use relationships.
  // Return the same instruction that they are passed.
  template<int I, int T>
      LInstruction* Define(LTemplateInstruction<1, I, T>* instr,
                           LUnallocated* result);
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
                                      DoubleRegister reg);
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

  LEnvironment* CreateEnvironment(HEnvironment* hydrogen_env,
                                  int* argument_index_accumulator,
                                  ZoneList<HValue*>* objects_to_materialize);

  void VisitInstruction(HInstruction* current);

  void DoBasicBlock(HBasicBlock* block, HBasicBlock* next_block);
  LInstruction* DoShift(Token::Value op, HBitwiseBinaryOperation* instr);
  LInstruction* DoArithmeticD(Token::Value op,
                              HArithmeticBinaryOperation* instr);
  LInstruction* DoArithmeticT(Token::Value op,
                              HBinaryOperation* instr);

  LPlatformChunk* chunk_;
  CompilationInfo* info_;
  HGraph* const graph_;
  Zone* zone_;
  Status status_;
  HInstruction* current_instruction_;
  HBasicBlock* current_block_;
  HBasicBlock* next_block_;
  int argument_count_;
  LAllocator* allocator_;
  int position_;
  LInstruction* instruction_pending_deoptimization_environment_;
  BailoutId pending_deoptimization_ast_id_;

  DISALLOW_COPY_AND_ASSIGN(LChunkBuilder);
};

#undef DECLARE_HYDROGEN_ACCESSOR
#undef DECLARE_CONCRETE_INSTRUCTION

} }  // namespace v8::internal

#endif  // V8_ARM_LITHIUM_ARM_H_
