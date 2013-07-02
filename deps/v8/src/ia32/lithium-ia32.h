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

#ifndef V8_IA32_LITHIUM_IA32_H_
#define V8_IA32_LITHIUM_IA32_H_

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
  V(AllocateObject)                             \
  V(ApplyArguments)                             \
  V(ArgumentsElements)                          \
  V(ArgumentsLength)                            \
  V(ArithmeticD)                                \
  V(ArithmeticT)                                \
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
  V(CallNewArray)                               \
  V(CallRuntime)                                \
  V(CallStub)                                   \
  V(CheckFunction)                              \
  V(CheckInstanceType)                          \
  V(CheckMaps)                                  \
  V(CheckNonSmi)                                \
  V(CheckPrototypeMaps)                         \
  V(CheckSmi)                                   \
  V(ClampDToUint8)                              \
  V(ClampIToUint8)                              \
  V(ClampTToUint8)                              \
  V(ClampTToUint8NoSSE2)                        \
  V(ClassOfTestAndBranch)                       \
  V(CmpIDAndBranch)                             \
  V(CmpObjectEqAndBranch)                       \
  V(CmpMapAndBranch)                            \
  V(CmpT)                                       \
  V(CmpConstantEqAndBranch)                     \
  V(ConstantD)                                  \
  V(ConstantI)                                  \
  V(ConstantS)                                  \
  V(ConstantT)                                  \
  V(Context)                                    \
  V(DebugBreak)                                 \
  V(DeclareGlobals)                             \
  V(DeleteProperty)                             \
  V(Deoptimize)                                 \
  V(DivI)                                       \
  V(DoubleToI)                                  \
  V(DoubleToSmi)                                \
  V(DummyUse)                                   \
  V(ElementsKind)                               \
  V(FunctionLiteral)                            \
  V(GetCachedArrayIndex)                        \
  V(GlobalObject)                               \
  V(GlobalReceiver)                             \
  V(Goto)                                       \
  V(HasCachedArrayIndexAndBranch)               \
  V(HasInstanceTypeAndBranch)                   \
  V(In)                                         \
  V(InstanceOf)                                 \
  V(InstanceOfKnownGlobal)                      \
  V(InstanceSize)                               \
  V(InstructionGap)                             \
  V(Integer32ToDouble)                          \
  V(Integer32ToSmi)                             \
  V(Uint32ToDouble)                             \
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
  V(LoadFunctionPrototype)                      \
  V(LoadGlobalCell)                             \
  V(LoadGlobalGeneric)                          \
  V(LoadKeyed)                                  \
  V(LoadKeyedGeneric)                           \
  V(LoadNamedField)                             \
  V(LoadNamedFieldPolymorphic)                  \
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
  V(NumberTagD)                                 \
  V(NumberTagI)                                 \
  V(NumberTagU)                                 \
  V(NumberUntagD)                               \
  V(OsrEntry)                                   \
  V(OuterContext)                               \
  V(Parameter)                                  \
  V(Power)                                      \
  V(Random)                                     \
  V(PushArgument)                               \
  V(RegExpLiteral)                              \
  V(Return)                                     \
  V(SeqStringSetChar)                           \
  V(ShiftI)                                     \
  V(SmiTag)                                     \
  V(SmiUntag)                                   \
  V(StackCheck)                                 \
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
  V(StringLength)                               \
  V(SubI)                                       \
  V(TaggedToI)                                  \
  V(TaggedToINoSSE2)                            \
  V(ThisFunction)                               \
  V(Throw)                                      \
  V(ToFastProperties)                           \
  V(TransitionElementsKind)                     \
  V(TrapAllocationMemento)                      \
  V(Typeof)                                     \
  V(TypeofIsAndBranch)                          \
  V(UnknownOSRValue)                            \
  V(ValueOf)                                    \
  V(ForInPrepareMap)                            \
  V(ForInCacheArray)                            \
  V(CheckMapValue)                              \
  V(LoadFieldByIndex)                           \
  V(DateField)                                  \
  V(WrapReceiver)                               \
  V(Drop)                                       \
  V(InnerAllocatedObject)


#define DECLARE_CONCRETE_INSTRUCTION(type, mnemonic)              \
  virtual Opcode opcode() const { return LInstruction::k##type; } \
  virtual void CompileToNative(LCodeGen* generator);              \
  virtual const char* Mnemonic() const { return mnemonic; }       \
  static L##type* cast(LInstruction* instr) {                     \
    ASSERT(instr->Is##type());                                    \
    return reinterpret_cast<L##type*>(instr);                     \
  }


#define DECLARE_HYDROGEN_ACCESSOR(type)     \
  H##type* hydrogen() const {               \
    return H##type::cast(hydrogen_value()); \
  }


class LInstruction: public ZoneObject {
 public:
  LInstruction()
      : environment_(NULL),
        hydrogen_value_(NULL),
        is_call_(false) { }
  virtual ~LInstruction() { }

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

  void MarkAsCall() { is_call_ = true; }

  // Interface to the register allocator and iterators.
  bool ClobbersTemps() const { return is_call_; }
  bool ClobbersRegisters() const { return is_call_; }
  virtual bool ClobbersDoubleRegisters() const {
    return is_call_ || !CpuFeatures::IsSupported(SSE2);
  }

  virtual bool HasResult() const = 0;
  virtual LOperand* result() = 0;

  bool HasDoubleRegisterResult();
  bool HasDoubleRegisterInput();

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

  LEnvironment* environment_;
  SetOncePointer<LPointerMap> pointer_map_;
  HValue* hydrogen_value_;
  bool is_call_;
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

 protected:
  EmbeddedContainer<LOperand*, R> results_;
  EmbeddedContainer<LOperand*, I> inputs_;
  EmbeddedContainer<LOperand*, T> temps_;

 private:
  // Iterator support.
  virtual int InputCount() { return I; }
  virtual LOperand* InputAt(int i) { return inputs_[i]; }

  virtual int TempCount() { return T; }
  virtual LOperand* TempAt(int i) { return temps_[i]; }
};


class LGap: public LTemplateInstruction<0, 0, 0> {
 public:
  explicit LGap(HBasicBlock* block) : block_(block) {
    parallel_moves_[BEFORE] = NULL;
    parallel_moves_[START] = NULL;
    parallel_moves_[END] = NULL;
    parallel_moves_[AFTER] = NULL;
  }

  // Can't use the DECLARE-macro here because of sub-classes.
  virtual bool IsGap() const { return true; }
  virtual void PrintDataTo(StringStream* stream);
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


class LInstructionGap: public LGap {
 public:
  explicit LInstructionGap(HBasicBlock* block) : LGap(block) { }
  virtual bool ClobbersDoubleRegisters() const { return false; }

  virtual bool HasInterestingComment(LCodeGen* gen) const {
    return !IsRedundant();
  }

  DECLARE_CONCRETE_INSTRUCTION(InstructionGap, "gap")
};


class LGoto: public LTemplateInstruction<0, 0, 0> {
 public:
  explicit LGoto(int block_id) : block_id_(block_id) { }

  virtual bool HasInterestingComment(LCodeGen* gen) const;
  DECLARE_CONCRETE_INSTRUCTION(Goto, "goto")
  virtual void PrintDataTo(StringStream* stream);
  virtual bool IsControl() const { return true; }

  int block_id() const { return block_id_; }

 private:
  int block_id_;
};


class LLazyBailout: public LTemplateInstruction<0, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(LazyBailout, "lazy-bailout")
};


class LDummyUse: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LDummyUse(LOperand* value) {
    inputs_[0] = value;
  }
  DECLARE_CONCRETE_INSTRUCTION(DummyUse, "dummy-use")
};


class LDeoptimize: public LTemplateInstruction<0, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Deoptimize, "deoptimize")
};


class LLabel: public LGap {
 public:
  explicit LLabel(HBasicBlock* block)
      : LGap(block), replacement_(NULL) { }

  virtual bool HasInterestingComment(LCodeGen* gen) const { return false; }
  DECLARE_CONCRETE_INSTRUCTION(Label, "label")

  virtual void PrintDataTo(StringStream* stream);

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


class LParameter: public LTemplateInstruction<1, 0, 0> {
 public:
  virtual bool HasInterestingComment(LCodeGen* gen) const { return false; }
  DECLARE_CONCRETE_INSTRUCTION(Parameter, "parameter")
};


class LCallStub: public LTemplateInstruction<1, 1, 0> {
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


class LUnknownOSRValue: public LTemplateInstruction<1, 0, 0> {
 public:
  virtual bool HasInterestingComment(LCodeGen* gen) const { return false; }
  DECLARE_CONCRETE_INSTRUCTION(UnknownOSRValue, "unknown-osr-value")
};


template<int I, int T>
class LControlInstruction: public LTemplateInstruction<0, I, T> {
 public:
  LControlInstruction() : false_label_(NULL), true_label_(NULL) { }

  virtual bool IsControl() const { return true; }

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


class LWrapReceiver: public LTemplateInstruction<1, 2, 1> {
 public:
  LWrapReceiver(LOperand* receiver,
                LOperand* function,
                LOperand* temp) {
    inputs_[0] = receiver;
    inputs_[1] = function;
    temps_[0] = temp;
  }

  LOperand* receiver() { return inputs_[0]; }
  LOperand* function() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(WrapReceiver, "wrap-receiver")
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

  LOperand* function() { return inputs_[0]; }
  LOperand* receiver() { return inputs_[1]; }
  LOperand* length() { return inputs_[2]; }
  LOperand* elements() { return inputs_[3]; }

  DECLARE_CONCRETE_INSTRUCTION(ApplyArguments, "apply-arguments")
};


class LAccessArgumentsAt: public LTemplateInstruction<1, 3, 0> {
 public:
  LAccessArgumentsAt(LOperand* arguments, LOperand* length, LOperand* index) {
    inputs_[0] = arguments;
    inputs_[1] = length;
    inputs_[2] = index;
  }

  LOperand* arguments() { return inputs_[0]; }
  LOperand* length() { return inputs_[1]; }
  LOperand* index() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(AccessArgumentsAt, "access-arguments-at")

  virtual void PrintDataTo(StringStream* stream);
};


class LArgumentsLength: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LArgumentsLength(LOperand* elements) {
    inputs_[0] = elements;
  }

  LOperand* elements() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength, "arguments-length")
};


class LArgumentsElements: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements, "arguments-elements")
  DECLARE_HYDROGEN_ACCESSOR(ArgumentsElements)
};


class LDebugBreak: public LTemplateInstruction<0, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(DebugBreak, "break")
};


class LModI: public LTemplateInstruction<1, 2, 1> {
 public:
  LModI(LOperand* left, LOperand* right, LOperand* temp) {
    inputs_[0] = left;
    inputs_[1] = right;
    temps_[0] = temp;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

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

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  bool is_flooring() { return hydrogen_value()->IsMathFloorOfDiv(); }

  DECLARE_CONCRETE_INSTRUCTION(DivI, "div-i")
  DECLARE_HYDROGEN_ACCESSOR(Div)
};


class LMathFloorOfDiv: public LTemplateInstruction<1, 2, 1> {
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


class LMulI: public LTemplateInstruction<1, 2, 1> {
 public:
  LMulI(LOperand* left, LOperand* right, LOperand* temp) {
    inputs_[0] = left;
    inputs_[1] = right;
    temps_[0] = temp;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MulI, "mul-i")
  DECLARE_HYDROGEN_ACCESSOR(Mul)
};


class LCmpIDAndBranch: public LControlInstruction<2, 0> {
 public:
  LCmpIDAndBranch(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CmpIDAndBranch, "cmp-id-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(CompareIDAndBranch)

  Token::Value op() const { return hydrogen()->token(); }
  bool is_double() const {
    return hydrogen()->representation().IsDouble();
  }

  virtual void PrintDataTo(StringStream* stream);
};


class LMathFloor: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathFloor(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathFloor, "math-floor")
  DECLARE_HYDROGEN_ACCESSOR(UnaryMathOperation)
};


class LMathRound: public LTemplateInstruction<1, 2, 1> {
 public:
  LMathRound(LOperand* context, LOperand* value, LOperand* temp) {
    inputs_[1] = context;
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* context() { return inputs_[1]; }
  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathRound, "math-round")
  DECLARE_HYDROGEN_ACCESSOR(UnaryMathOperation)
};


class LMathAbs: public LTemplateInstruction<1, 2, 0> {
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


class LMathLog: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathLog(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathLog, "math-log")
};


class LMathSin: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathSin(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathSin, "math-sin")
};


class LMathCos: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathCos(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathCos, "math-cos")
};


class LMathTan: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathTan(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathTan, "math-tan")
};


class LMathExp: public LTemplateInstruction<1, 1, 2> {
 public:
  LMathExp(LOperand* value,
           LOperand* temp1,
           LOperand* temp2) {
    inputs_[0] = value;
    temps_[0] = temp1;
    temps_[1] = temp2;
    ExternalReference::InitializeMathExpData();
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp1() { return temps_[0]; }
  LOperand* temp2() { return temps_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(MathExp, "math-exp")
};


class LMathSqrt: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMathSqrt(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathSqrt, "math-sqrt")
};


class LMathPowHalf: public LTemplateInstruction<1, 2, 1> {
 public:
  LMathPowHalf(LOperand* context, LOperand* value, LOperand* temp) {
    inputs_[1] = context;
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* context() { return inputs_[1]; }
  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MathPowHalf, "math-pow-half")
};


class LCmpObjectEqAndBranch: public LControlInstruction<2, 0> {
 public:
  LCmpObjectEqAndBranch(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CmpObjectEqAndBranch,
                               "cmp-object-eq-and-branch")
};


class LCmpConstantEqAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LCmpConstantEqAndBranch(LOperand* left) {
    inputs_[0] = left;
  }

  LOperand* left() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CmpConstantEqAndBranch,
                               "cmp-constant-eq-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(CompareConstantEqAndBranch)
};


class LIsObjectAndBranch: public LControlInstruction<1, 1> {
 public:
  LIsObjectAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsObjectAndBranch, "is-object-and-branch")

  virtual void PrintDataTo(StringStream* stream);
};


class LIsStringAndBranch: public LControlInstruction<1, 1> {
 public:
  LIsStringAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsStringAndBranch, "is-string-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(IsStringAndBranch)

  virtual void PrintDataTo(StringStream* stream);
};


class LIsSmiAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LIsSmiAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch, "is-smi-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(IsSmiAndBranch)

  virtual void PrintDataTo(StringStream* stream);
};


class LIsUndetectableAndBranch: public LControlInstruction<1, 1> {
 public:
  LIsUndetectableAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsUndetectableAndBranch,
                               "is-undetectable-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(IsUndetectableAndBranch)

  virtual void PrintDataTo(StringStream* stream);
};


class LStringCompareAndBranch: public LControlInstruction<3, 0> {
 public:
  LStringCompareAndBranch(LOperand* context, LOperand* left, LOperand* right) {
    inputs_[0] = context;
    inputs_[1] = left;
    inputs_[2] = right;
  }

  LOperand* left() { return inputs_[1]; }
  LOperand* right() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(StringCompareAndBranch,
                               "string-compare-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(StringCompareAndBranch)

  virtual void PrintDataTo(StringStream* stream);

  Token::Value op() const { return hydrogen()->token(); }
};


class LHasInstanceTypeAndBranch: public LControlInstruction<1, 1> {
 public:
  LHasInstanceTypeAndBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceTypeAndBranch,
                               "has-instance-type-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(HasInstanceTypeAndBranch)

  virtual void PrintDataTo(StringStream* stream);
};


class LGetCachedArrayIndex: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LGetCachedArrayIndex(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(GetCachedArrayIndex, "get-cached-array-index")
  DECLARE_HYDROGEN_ACCESSOR(GetCachedArrayIndex)
};


class LHasCachedArrayIndexAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LHasCachedArrayIndexAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(HasCachedArrayIndexAndBranch,
                               "has-cached-array-index-and-branch")

  virtual void PrintDataTo(StringStream* stream);
};


class LIsConstructCallAndBranch: public LControlInstruction<0, 1> {
 public:
  explicit LIsConstructCallAndBranch(LOperand* temp) {
    temps_[0] = temp;
  }

  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(IsConstructCallAndBranch,
                               "is-construct-call-and-branch")
};


class LClassOfTestAndBranch: public LControlInstruction<1, 2> {
 public:
  LClassOfTestAndBranch(LOperand* value, LOperand* temp, LOperand* temp2) {
    inputs_[0] = value;
    temps_[0] = temp;
    temps_[1] = temp2;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }
  LOperand* temp2() { return temps_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch,
                               "class-of-test-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(ClassOfTestAndBranch)

  virtual void PrintDataTo(StringStream* stream);
};


class LCmpT: public LTemplateInstruction<1, 3, 0> {
 public:
  LCmpT(LOperand* context, LOperand* left, LOperand* right) {
    inputs_[0] = context;
    inputs_[1] = left;
    inputs_[2] = right;
  }

  DECLARE_CONCRETE_INSTRUCTION(CmpT, "cmp-t")
  DECLARE_HYDROGEN_ACCESSOR(CompareGeneric)

  Token::Value op() const { return hydrogen()->token(); }
};


class LInstanceOf: public LTemplateInstruction<1, 3, 0> {
 public:
  LInstanceOf(LOperand* context, LOperand* left, LOperand* right) {
    inputs_[0] = context;
    inputs_[1] = left;
    inputs_[2] = right;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOf, "instance-of")
};


class LInstanceOfKnownGlobal: public LTemplateInstruction<1, 2, 1> {
 public:
  LInstanceOfKnownGlobal(LOperand* context, LOperand* value, LOperand* temp) {
    inputs_[0] = context;
    inputs_[1] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(InstanceOfKnownGlobal,
                               "instance-of-known-global")
  DECLARE_HYDROGEN_ACCESSOR(InstanceOfKnownGlobal)

  Handle<JSFunction> function() const { return hydrogen()->function(); }
  LEnvironment* GetDeferredLazyDeoptimizationEnvironment() {
    return lazy_deopt_env_;
  }
  virtual void SetDeferredLazyDeoptimizationEnvironment(LEnvironment* env) {
    lazy_deopt_env_ = env;
  }

 private:
  LEnvironment* lazy_deopt_env_;
};


class LInstanceSize: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LInstanceSize(LOperand* object) {
    inputs_[0] = object;
  }

  LOperand* object() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(InstanceSize, "instance-size")
  DECLARE_HYDROGEN_ACCESSOR(InstanceSize)
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
  DECLARE_HYDROGEN_ACCESSOR(BoundsCheck)
};


class LBitI: public LTemplateInstruction<1, 2, 0> {
 public:
  LBitI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(BitI, "bit-i")
  DECLARE_HYDROGEN_ACCESSOR(Bitwise)

  Token::Value op() const { return hydrogen()->op(); }
};


class LShiftI: public LTemplateInstruction<1, 2, 0> {
 public:
  LShiftI(Token::Value op, LOperand* left, LOperand* right, bool can_deopt)
      : op_(op), can_deopt_(can_deopt) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(ShiftI, "shift-i")

  Token::Value op() const { return op_; }
  bool can_deopt() const { return can_deopt_; }

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

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(SubI, "sub-i")
  DECLARE_HYDROGEN_ACCESSOR(Sub)
};


class LConstantI: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ConstantI, "constant-i")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  int32_t value() const { return hydrogen()->Integer32Value(); }
};


class LConstantS: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ConstantS, "constant-s")
  DECLARE_HYDROGEN_ACCESSOR(Constant)

  Smi* value() const { return Smi::FromInt(hydrogen()->Integer32Value()); }
};


class LConstantD: public LTemplateInstruction<1, 0, 1> {
 public:
  explicit LConstantD(LOperand* temp) {
    temps_[0] = temp;
  }

  virtual bool ClobbersDoubleRegisters() const {
    return false;
  }

  LOperand* temp() { return temps_[0]; }

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


class LBranch: public LControlInstruction<1, 1> {
 public:
  LBranch(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Branch, "branch")
  DECLARE_HYDROGEN_ACCESSOR(Branch)

  virtual void PrintDataTo(StringStream* stream);
};


class LCmpMapAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LCmpMapAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CmpMapAndBranch, "cmp-map-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(CompareMap)

  Handle<Map> map() const { return hydrogen()->map(); }
};


class LMapEnumLength: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LMapEnumLength(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(MapEnumLength, "map-enum-length")
};


class LElementsKind: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LElementsKind(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ElementsKind, "elements-kind")
  DECLARE_HYDROGEN_ACCESSOR(ElementsKind)
};


class LValueOf: public LTemplateInstruction<1, 1, 1> {
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


class LDateField: public LTemplateInstruction<1, 1, 1> {
 public:
  LDateField(LOperand* date, LOperand* temp, Smi* index)
      : index_(index) {
    inputs_[0] = date;
    temps_[0] = temp;
  }

  LOperand* date() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(DateField, "date-field")
  DECLARE_HYDROGEN_ACCESSOR(DateField)

  Smi* index() const { return index_; }

 private:
  Smi* index_;
};


class LSeqStringSetChar: public LTemplateInstruction<1, 3, 0> {
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


class LThrow: public LTemplateInstruction<0, 2, 0> {
 public:
  LThrow(LOperand* context, LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = value;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(Throw, "throw")
};


class LBitNotI: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LBitNotI(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(BitNotI, "bit-not-i")
};


class LAddI: public LTemplateInstruction<1, 2, 0> {
 public:
  LAddI(LOperand* left, LOperand* right) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  static bool UseLea(HAdd* add) {
    return !add->CheckFlag(HValue::kCanOverflow) &&
        add->BetterLeftOperand()->UseCount() > 1;
  }

  DECLARE_CONCRETE_INSTRUCTION(AddI, "add-i")
  DECLARE_HYDROGEN_ACCESSOR(Add)
};


class LMathMinMax: public LTemplateInstruction<1, 2, 0> {
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


class LPower: public LTemplateInstruction<1, 2, 0> {
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


class LRandom: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LRandom(LOperand* global_object) {
    inputs_[0] = global_object;
  }

  LOperand* global_object() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Random, "random")
  DECLARE_HYDROGEN_ACCESSOR(Random)
};


class LArithmeticD: public LTemplateInstruction<1, 2, 0> {
 public:
  LArithmeticD(Token::Value op, LOperand* left, LOperand* right)
      : op_(op) {
    inputs_[0] = left;
    inputs_[1] = right;
  }

  LOperand* left() { return inputs_[0]; }
  LOperand* right() { return inputs_[1]; }

  Token::Value op() const { return op_; }

  virtual Opcode opcode() const { return LInstruction::kArithmeticD; }
  virtual void CompileToNative(LCodeGen* generator);
  virtual const char* Mnemonic() const;

 private:
  Token::Value op_;
};


class LArithmeticT: public LTemplateInstruction<1, 3, 0> {
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

  virtual Opcode opcode() const { return LInstruction::kArithmeticT; }
  virtual void CompileToNative(LCodeGen* generator);
  virtual const char* Mnemonic() const;

  Token::Value op() const { return op_; }

 private:
  Token::Value op_;
};


class LReturn: public LTemplateInstruction<0, 3, 0> {
 public:
  explicit LReturn(LOperand* value, LOperand* context,
                   LOperand* parameter_count) {
    inputs_[0] = value;
    inputs_[1] = context;
    inputs_[2] = parameter_count;
  }

  bool has_constant_parameter_count() {
    return parameter_count()->IsConstantOperand();
  }
  LConstantOperand* constant_parameter_count() {
    ASSERT(has_constant_parameter_count());
    return LConstantOperand::cast(parameter_count());
  }
  LOperand* parameter_count() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(Return, "return")
  DECLARE_HYDROGEN_ACCESSOR(Return)
};


class LLoadNamedField: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadNamedField(LOperand* object) {
    inputs_[0] = object;
  }

  virtual bool ClobbersDoubleRegisters() const {
    return !CpuFeatures::IsSupported(SSE2) &&
        !hydrogen()->representation().IsDouble();
  }

  LOperand* object() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField, "load-named-field")
  DECLARE_HYDROGEN_ACCESSOR(LoadNamedField)
};


class LLoadNamedFieldPolymorphic: public LTemplateInstruction<1, 2, 0> {
 public:
  LLoadNamedFieldPolymorphic(LOperand* context, LOperand* object) {
    inputs_[0] = context;
    inputs_[1] = object;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField, "load-named-field-polymorphic")
  DECLARE_HYDROGEN_ACCESSOR(LoadNamedFieldPolymorphic)
};


class LLoadNamedGeneric: public LTemplateInstruction<1, 2, 0> {
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


class LLoadFunctionPrototype: public LTemplateInstruction<1, 1, 1> {
 public:
  LLoadFunctionPrototype(LOperand* function, LOperand* temp) {
    inputs_[0] = function;
    temps_[0] = temp;
  }

  LOperand* function() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype, "load-function-prototype")
  DECLARE_HYDROGEN_ACCESSOR(LoadFunctionPrototype)
};


class LLoadExternalArrayPointer: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadExternalArrayPointer(LOperand* object) {
    inputs_[0] = object;
  }

  LOperand* object() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadExternalArrayPointer,
                               "load-external-array-pointer")
};


class LLoadKeyed: public LTemplateInstruction<1, 2, 0> {
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

  virtual bool ClobbersDoubleRegisters() const {
    return !CpuFeatures::IsSupported(SSE2) &&
        !IsDoubleOrFloatElementsKind(hydrogen()->elements_kind());
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyed, "load-keyed")
  DECLARE_HYDROGEN_ACCESSOR(LoadKeyed)

  virtual void PrintDataTo(StringStream* stream);
  uint32_t additional_index() const { return hydrogen()->index_offset(); }
  bool key_is_smi() {
    return hydrogen()->key()->representation().IsTagged();
  }
};


inline static bool ExternalArrayOpRequiresTemp(
    Representation key_representation,
    ElementsKind elements_kind) {
  // Operations that require the key to be divided by two to be converted into
  // an index cannot fold the scale operation into a load and need an extra
  // temp register to do the work.
  return key_representation.IsSmi() &&
      (elements_kind == EXTERNAL_BYTE_ELEMENTS ||
       elements_kind == EXTERNAL_UNSIGNED_BYTE_ELEMENTS ||
       elements_kind == EXTERNAL_PIXEL_ELEMENTS);
}


class LLoadKeyedGeneric: public LTemplateInstruction<1, 3, 0> {
 public:
  LLoadKeyedGeneric(LOperand* context, LOperand* obj, LOperand* key) {
    inputs_[0] = context;
    inputs_[1] = obj;
    inputs_[2] = key;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }
  LOperand* key() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyedGeneric, "load-keyed-generic")
};


class LLoadGlobalCell: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(LoadGlobalCell, "load-global-cell")
  DECLARE_HYDROGEN_ACCESSOR(LoadGlobalCell)
};


class LLoadGlobalGeneric: public LTemplateInstruction<1, 2, 0> {
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


class LStoreGlobalCell: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LStoreGlobalCell(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreGlobalCell, "store-global-cell")
  DECLARE_HYDROGEN_ACCESSOR(StoreGlobalCell)
};


class LStoreGlobalGeneric: public LTemplateInstruction<0, 3, 0> {
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


class LLoadContextSlot: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LLoadContextSlot(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(LoadContextSlot, "load-context-slot")
  DECLARE_HYDROGEN_ACCESSOR(LoadContextSlot)

  int slot_index() { return hydrogen()->slot_index(); }

  virtual void PrintDataTo(StringStream* stream);
};


class LStoreContextSlot: public LTemplateInstruction<0, 2, 1> {
 public:
  LStoreContextSlot(LOperand* context, LOperand* value, LOperand* temp) {
    inputs_[0] = context;
    inputs_[1] = value;
    temps_[0] = temp;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreContextSlot, "store-context-slot")
  DECLARE_HYDROGEN_ACCESSOR(StoreContextSlot)

  int slot_index() { return hydrogen()->slot_index(); }

  virtual void PrintDataTo(StringStream* stream);
};


class LPushArgument: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LPushArgument(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(PushArgument, "push-argument")
};


class LDrop: public LTemplateInstruction<0, 0, 0> {
 public:
  explicit LDrop(int count) : count_(count) { }

  int count() const { return count_; }

  DECLARE_CONCRETE_INSTRUCTION(Drop, "drop")

 private:
  int count_;
};


class LInnerAllocatedObject: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LInnerAllocatedObject(LOperand* base_object) {
    inputs_[0] = base_object;
  }

  LOperand* base_object() { return inputs_[0]; }
  int offset() { return hydrogen()->offset(); }

  virtual void PrintDataTo(StringStream* stream);

  DECLARE_CONCRETE_INSTRUCTION(InnerAllocatedObject, "sub-allocated-object")
  DECLARE_HYDROGEN_ACCESSOR(InnerAllocatedObject)
};


class LThisFunction: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(ThisFunction, "this-function")
  DECLARE_HYDROGEN_ACCESSOR(ThisFunction)
};


class LContext: public LTemplateInstruction<1, 0, 0> {
 public:
  DECLARE_CONCRETE_INSTRUCTION(Context, "context")
  DECLARE_HYDROGEN_ACCESSOR(Context)
};


class LOuterContext: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LOuterContext(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(OuterContext, "outer-context")
};


class LDeclareGlobals: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LDeclareGlobals(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(DeclareGlobals, "declare-globals")
  DECLARE_HYDROGEN_ACCESSOR(DeclareGlobals)
};


class LGlobalObject: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LGlobalObject(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(GlobalObject, "global-object")
};


class LGlobalReceiver: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LGlobalReceiver(LOperand* global_object) {
    inputs_[0] = global_object;
  }

  LOperand* global() { return inputs_[0]; }

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


class LInvokeFunction: public LTemplateInstruction<1, 2, 0> {
 public:
  LInvokeFunction(LOperand* context, LOperand* function) {
    inputs_[0] = context;
    inputs_[1] = function;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* function() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(InvokeFunction, "invoke-function")
  DECLARE_HYDROGEN_ACCESSOR(InvokeFunction)

  virtual void PrintDataTo(StringStream* stream);

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallKeyed: public LTemplateInstruction<1, 2, 0> {
 public:
  LCallKeyed(LOperand* context, LOperand* key) {
    inputs_[0] = context;
    inputs_[1] = key;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CallKeyed, "call-keyed")
  DECLARE_HYDROGEN_ACCESSOR(CallKeyed)

  virtual void PrintDataTo(StringStream* stream);

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallNamed: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCallNamed(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CallNamed, "call-named")
  DECLARE_HYDROGEN_ACCESSOR(CallNamed)

  virtual void PrintDataTo(StringStream* stream);

  Handle<String> name() const { return hydrogen()->name(); }
  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallFunction: public LTemplateInstruction<1, 2, 0> {
 public:
  explicit LCallFunction(LOperand* context, LOperand* function) {
    inputs_[0] = context;
    inputs_[1] = function;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* function() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CallFunction, "call-function")
  DECLARE_HYDROGEN_ACCESSOR(CallFunction)

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallGlobal: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCallGlobal(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

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

  int arity() const { return hydrogen()->argument_count() - 1;  }
};


class LCallNew: public LTemplateInstruction<1, 2, 0> {
 public:
  LCallNew(LOperand* context, LOperand* constructor) {
    inputs_[0] = context;
    inputs_[1] = constructor;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* constructor() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CallNew, "call-new")
  DECLARE_HYDROGEN_ACCESSOR(CallNew)

  virtual void PrintDataTo(StringStream* stream);

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallNewArray: public LTemplateInstruction<1, 2, 0> {
 public:
  LCallNewArray(LOperand* context, LOperand* constructor) {
    inputs_[0] = context;
    inputs_[1] = constructor;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* constructor() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CallNewArray, "call-new-array")
  DECLARE_HYDROGEN_ACCESSOR(CallNewArray)

  virtual void PrintDataTo(StringStream* stream);

  int arity() const { return hydrogen()->argument_count() - 1; }
};


class LCallRuntime: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCallRuntime(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CallRuntime, "call-runtime")
  DECLARE_HYDROGEN_ACCESSOR(CallRuntime)

  const Runtime::Function* function() const { return hydrogen()->function(); }
  int arity() const { return hydrogen()->argument_count(); }
};


class LInteger32ToDouble: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LInteger32ToDouble(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Integer32ToDouble, "int32-to-double")
};


class LInteger32ToSmi: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LInteger32ToSmi(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Integer32ToSmi, "int32-to-smi")
  DECLARE_HYDROGEN_ACCESSOR(Change)
};


class LUint32ToDouble: public LTemplateInstruction<1, 1, 1> {
 public:
  explicit LUint32ToDouble(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Uint32ToDouble, "uint32-to-double")
};


class LNumberTagI: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LNumberTagI(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagI, "number-tag-i")
};


class LNumberTagU: public LTemplateInstruction<1, 1, 1> {
 public:
  LNumberTagU(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagU, "number-tag-u")
};


class LNumberTagD: public LTemplateInstruction<1, 1, 1> {
 public:
  LNumberTagD(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(NumberTagD, "number-tag-d")
  DECLARE_HYDROGEN_ACCESSOR(Change)
};


// Sometimes truncating conversion from a tagged value to an int32.
class LDoubleToI: public LTemplateInstruction<1, 1, 1> {
 public:
  LDoubleToI(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(DoubleToI, "double-to-i")
  DECLARE_HYDROGEN_ACCESSOR(UnaryOperation)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
};


class LDoubleToSmi: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LDoubleToSmi(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(DoubleToSmi, "double-to-smi")
  DECLARE_HYDROGEN_ACCESSOR(UnaryOperation)
};


// Truncating conversion from a tagged value to an int32.
class LTaggedToI: public LTemplateInstruction<1, 1, 1> {
 public:
  LTaggedToI(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(TaggedToI, "tagged-to-i")
  DECLARE_HYDROGEN_ACCESSOR(UnaryOperation)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
};


// Truncating conversion from a tagged value to an int32.
class LTaggedToINoSSE2: public LTemplateInstruction<1, 1, 3> {
 public:
  LTaggedToINoSSE2(LOperand* value,
                   LOperand* temp1,
                   LOperand* temp2,
                   LOperand* temp3) {
    inputs_[0] = value;
    temps_[0] = temp1;
    temps_[1] = temp2;
    temps_[2] = temp3;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* scratch() { return temps_[0]; }
  LOperand* scratch2() { return temps_[1]; }
  LOperand* scratch3() { return temps_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(TaggedToINoSSE2, "tagged-to-i-nosse2")
  DECLARE_HYDROGEN_ACCESSOR(UnaryOperation)

  bool truncating() { return hydrogen()->CanTruncateToInt32(); }
};


class LSmiTag: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LSmiTag(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(SmiTag, "smi-tag")
};


class LNumberUntagD: public LTemplateInstruction<1, 1, 1> {
 public:
  explicit LNumberUntagD(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  virtual bool ClobbersDoubleRegisters() const {
    return false;
  }

  DECLARE_CONCRETE_INSTRUCTION(NumberUntagD, "double-untag")
  DECLARE_HYDROGEN_ACCESSOR(Change);
};


class LSmiUntag: public LTemplateInstruction<1, 1, 0> {
 public:
  LSmiUntag(LOperand* value, bool needs_check)
      : needs_check_(needs_check) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(SmiUntag, "smi-untag")

  bool needs_check() const { return needs_check_; }

 private:
  bool needs_check_;
};


class LStoreNamedField: public LTemplateInstruction<0, 2, 2> {
 public:
  LStoreNamedField(LOperand* obj,
                   LOperand* val,
                   LOperand* temp,
                   LOperand* temp_map) {
    inputs_[0] = obj;
    inputs_[1] = val;
    temps_[0] = temp;
    temps_[1] = temp_map;
  }

  LOperand* object() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }
  LOperand* temp_map() { return temps_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField, "store-named-field")
  DECLARE_HYDROGEN_ACCESSOR(StoreNamedField)

  virtual void PrintDataTo(StringStream* stream);

  Handle<Map> transition() const { return hydrogen()->transition(); }
  Representation representation() const {
    return hydrogen()->field_representation();
  }
};


class LStoreNamedGeneric: public LTemplateInstruction<0, 3, 0> {
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

  virtual void PrintDataTo(StringStream* stream);
  Handle<Object> name() const { return hydrogen()->name(); }
  StrictModeFlag strict_mode_flag() { return hydrogen()->strict_mode_flag(); }
};


class LStoreKeyed: public LTemplateInstruction<0, 3, 0> {
 public:
  LStoreKeyed(LOperand* obj, LOperand* key, LOperand* val) {
    inputs_[0] = obj;
    inputs_[1] = key;
    inputs_[2] = val;
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

  virtual void PrintDataTo(StringStream* stream);
  uint32_t additional_index() const { return hydrogen()->index_offset(); }
  bool NeedsCanonicalization() { return hydrogen()->NeedsCanonicalization(); }
};


class LStoreKeyedGeneric: public LTemplateInstruction<0, 4, 0> {
 public:
  LStoreKeyedGeneric(LOperand* context,
                     LOperand* object,
                     LOperand* key,
                     LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = object;
    inputs_[2] = key;
    inputs_[3] = value;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }
  LOperand* key() { return inputs_[2]; }
  LOperand* value() { return inputs_[3]; }

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyedGeneric, "store-keyed-generic")
  DECLARE_HYDROGEN_ACCESSOR(StoreKeyedGeneric)

  virtual void PrintDataTo(StringStream* stream);

  StrictModeFlag strict_mode_flag() { return hydrogen()->strict_mode_flag(); }
};


class LTransitionElementsKind: public LTemplateInstruction<0, 2, 2> {
 public:
  LTransitionElementsKind(LOperand* object,
                          LOperand* context,
                          LOperand* new_map_temp,
                          LOperand* temp) {
    inputs_[0] = object;
    inputs_[1] = context;
    temps_[0] = new_map_temp;
    temps_[1] = temp;
  }

  LOperand* context() { return inputs_[1]; }
  LOperand* object() { return inputs_[0]; }
  LOperand* new_map_temp() { return temps_[0]; }
  LOperand* temp() { return temps_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(TransitionElementsKind,
                               "transition-elements-kind")
  DECLARE_HYDROGEN_ACCESSOR(TransitionElementsKind)

  virtual void PrintDataTo(StringStream* stream);

  Handle<Map> original_map() { return hydrogen()->original_map(); }
  Handle<Map> transitioned_map() { return hydrogen()->transitioned_map(); }
  ElementsKind from_kind() { return hydrogen()->from_kind(); }
  ElementsKind to_kind() { return hydrogen()->to_kind(); }
};


class LTrapAllocationMemento : public LTemplateInstruction<0, 1, 1> {
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


class LStringAdd: public LTemplateInstruction<1, 3, 0> {
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


class LStringCharCodeAt: public LTemplateInstruction<1, 3, 0> {
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


class LStringCharFromCode: public LTemplateInstruction<1, 2, 0> {
 public:
  LStringCharFromCode(LOperand* context, LOperand* char_code) {
    inputs_[0] = context;
    inputs_[1] = char_code;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* char_code() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(StringCharFromCode, "string-char-from-code")
  DECLARE_HYDROGEN_ACCESSOR(StringCharFromCode)
};


class LStringLength: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LStringLength(LOperand* string) {
    inputs_[0] = string;
  }

  LOperand* string() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(StringLength, "string-length")
  DECLARE_HYDROGEN_ACCESSOR(StringLength)
};


class LCheckFunction: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckFunction(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckFunction, "check-function")
  DECLARE_HYDROGEN_ACCESSOR(CheckFunction)
};


class LCheckInstanceType: public LTemplateInstruction<0, 1, 1> {
 public:
  LCheckInstanceType(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckInstanceType, "check-instance-type")
  DECLARE_HYDROGEN_ACCESSOR(CheckInstanceType)
};


class LCheckMaps: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckMaps(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckMaps, "check-maps")
  DECLARE_HYDROGEN_ACCESSOR(CheckMaps)
};


class LCheckPrototypeMaps: public LTemplateInstruction<0, 0, 1> {
 public:
  explicit LCheckPrototypeMaps(LOperand* temp)  {
    temps_[0] = temp;
  }

  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckPrototypeMaps, "check-prototype-maps")
  DECLARE_HYDROGEN_ACCESSOR(CheckPrototypeMaps)

  ZoneList<Handle<JSObject> >* prototypes() const {
    return hydrogen()->prototypes();
  }
  ZoneList<Handle<Map> >* maps() const { return hydrogen()->maps(); }
};


class LCheckSmi: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LCheckSmi(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckSmi, "check-smi")
};


class LClampDToUint8: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LClampDToUint8(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* unclamped() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ClampDToUint8, "clamp-d-to-uint8")
};


class LClampIToUint8: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LClampIToUint8(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* unclamped() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ClampIToUint8, "clamp-i-to-uint8")
};


class LClampTToUint8: public LTemplateInstruction<1, 1, 1> {
 public:
  LClampTToUint8(LOperand* value, LOperand* temp) {
    inputs_[0] = value;
    temps_[0] = temp;
  }

  LOperand* unclamped() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ClampTToUint8, "clamp-t-to-uint8")
};


// Truncating conversion from a tagged value to an int32.
class LClampTToUint8NoSSE2: public LTemplateInstruction<1, 1, 3> {
 public:
  LClampTToUint8NoSSE2(LOperand* unclamped,
                       LOperand* temp1,
                       LOperand* temp2,
                       LOperand* temp3) {
    inputs_[0] = unclamped;
    temps_[0] = temp1;
    temps_[1] = temp2;
    temps_[2] = temp3;
  }

  LOperand* unclamped() { return inputs_[0]; }
  LOperand* scratch() { return temps_[0]; }
  LOperand* scratch2() { return temps_[1]; }
  LOperand* scratch3() { return temps_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(ClampTToUint8NoSSE2,
                               "clamp-t-to-uint8-nosse2")
  DECLARE_HYDROGEN_ACCESSOR(UnaryOperation)
};


class LCheckNonSmi: public LTemplateInstruction<0, 1, 0> {
 public:
  explicit LCheckNonSmi(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckNonSmi, "check-non-smi")
  DECLARE_HYDROGEN_ACCESSOR(CheckHeapObject)
};


class LAllocateObject: public LTemplateInstruction<1, 1, 1> {
 public:
  LAllocateObject(LOperand* context, LOperand* temp) {
    inputs_[0] = context;
    temps_[0] = temp;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(AllocateObject, "allocate-object")
  DECLARE_HYDROGEN_ACCESSOR(AllocateObject)
};


class LAllocate: public LTemplateInstruction<1, 2, 1> {
 public:
  LAllocate(LOperand* context, LOperand* size, LOperand* temp) {
    inputs_[0] = context;
    inputs_[1] = size;
    temps_[0] = temp;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* size() { return inputs_[1]; }
  LOperand* temp() { return temps_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(Allocate, "allocate")
  DECLARE_HYDROGEN_ACCESSOR(Allocate)
};


class LRegExpLiteral: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LRegExpLiteral(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(RegExpLiteral, "regexp-literal")
  DECLARE_HYDROGEN_ACCESSOR(RegExpLiteral)
};


class LFunctionLiteral: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LFunctionLiteral(LOperand* context) {
    inputs_[0] = context;
  }

  LOperand* context() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(FunctionLiteral, "function-literal")
  DECLARE_HYDROGEN_ACCESSOR(FunctionLiteral)
};


class LToFastProperties: public LTemplateInstruction<1, 1, 0> {
 public:
  explicit LToFastProperties(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(ToFastProperties, "to-fast-properties")
  DECLARE_HYDROGEN_ACCESSOR(ToFastProperties)
};


class LTypeof: public LTemplateInstruction<1, 2, 0> {
 public:
  LTypeof(LOperand* context, LOperand* value) {
    inputs_[0] = context;
    inputs_[1] = value;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* value() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(Typeof, "typeof")
};


class LTypeofIsAndBranch: public LControlInstruction<1, 0> {
 public:
  explicit LTypeofIsAndBranch(LOperand* value) {
    inputs_[0] = value;
  }

  LOperand* value() { return inputs_[0]; }

  DECLARE_CONCRETE_INSTRUCTION(TypeofIsAndBranch, "typeof-is-and-branch")
  DECLARE_HYDROGEN_ACCESSOR(TypeofIsAndBranch)

  Handle<String> type_literal() { return hydrogen()->type_literal(); }

  virtual void PrintDataTo(StringStream* stream);
};


class LDeleteProperty: public LTemplateInstruction<1, 3, 0> {
 public:
  LDeleteProperty(LOperand* context, LOperand* obj, LOperand* key) {
    inputs_[0] = context;
    inputs_[1] = obj;
    inputs_[2] = key;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }
  LOperand* key() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(DeleteProperty, "delete-property")
};


class LOsrEntry: public LTemplateInstruction<0, 0, 0> {
 public:
  LOsrEntry() {}

  virtual bool HasInterestingComment(LCodeGen* gen) const { return false; }
  DECLARE_CONCRETE_INSTRUCTION(OsrEntry, "osr-entry")
};


class LStackCheck: public LTemplateInstruction<0, 1, 0> {
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


class LIn: public LTemplateInstruction<1, 3, 0> {
 public:
  LIn(LOperand* context, LOperand* key, LOperand* object) {
    inputs_[0] = context;
    inputs_[1] = key;
    inputs_[2] = object;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* key() { return inputs_[1]; }
  LOperand* object() { return inputs_[2]; }

  DECLARE_CONCRETE_INSTRUCTION(In, "in")
};


class LForInPrepareMap: public LTemplateInstruction<1, 2, 0> {
 public:
  LForInPrepareMap(LOperand* context, LOperand* object) {
    inputs_[0] = context;
    inputs_[1] = object;
  }

  LOperand* context() { return inputs_[0]; }
  LOperand* object() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(ForInPrepareMap, "for-in-prepare-map")
};


class LForInCacheArray: public LTemplateInstruction<1, 1, 0> {
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


class LCheckMapValue: public LTemplateInstruction<0, 2, 0> {
 public:
  LCheckMapValue(LOperand* value, LOperand* map) {
    inputs_[0] = value;
    inputs_[1] = map;
  }

  LOperand* value() { return inputs_[0]; }
  LOperand* map() { return inputs_[1]; }

  DECLARE_CONCRETE_INSTRUCTION(CheckMapValue, "check-map-value")
};


class LLoadFieldByIndex: public LTemplateInstruction<1, 2, 0> {
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
class LPlatformChunk: public LChunk {
 public:
  LPlatformChunk(CompilationInfo* info, HGraph* graph)
      : LChunk(info, graph),
        num_double_slots_(0) { }

  int GetNextSpillIndex(bool is_double);
  LOperand* GetNextSpillSlot(bool is_double);

  int num_double_slots() const { return num_double_slots_; }

 private:
  int num_double_slots_;
};


class LChunkBuilder BASE_EMBEDDED {
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

  // Declare methods that deal with the individual node types.
#define DECLARE_DO(type) LInstruction* Do##type(H##type* node);
  HYDROGEN_CONCRETE_INSTRUCTION_LIST(DECLARE_DO)
#undef DECLARE_DO

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

  void Abort(const char* reason);

  // Methods for getting operands for Use / Define / Temp.
  LUnallocated* ToUnallocated(Register reg);
  LUnallocated* ToUnallocated(XMMRegister reg);
  LUnallocated* ToUnallocated(X87TopOfStackRegister reg);

  // Methods for setting up define-use relationships.
  MUST_USE_RESULT LOperand* Use(HValue* value, LUnallocated* operand);
  MUST_USE_RESULT LOperand* UseFixed(HValue* value, Register fixed_register);
  MUST_USE_RESULT LOperand* UseFixedDouble(HValue* value,
                                           XMMRegister fixed_register);
  MUST_USE_RESULT LOperand* UseX87TopOfStack(HValue* value);

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
  MUST_USE_RESULT LOperand* FixedTemp(XMMRegister reg);

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
                                      XMMRegister reg);
  template<int I, int T>
      LInstruction* DefineX87TOS(LTemplateInstruction<1, I, T>* instr);
  // Assigns an environment to an instruction.  An instruction which can
  // deoptimize must have an environment.
  LInstruction* AssignEnvironment(LInstruction* instr);
  // Assigns a pointer map to an instruction.  An instruction which can
  // trigger a GC or a lazy deoptimization must have a pointer map.
  LInstruction* AssignPointerMap(LInstruction* instr);

  enum CanDeoptimize { CAN_DEOPTIMIZE_EAGERLY, CANNOT_DEOPTIMIZE_EAGERLY };

  // Marks a call for the register allocator.  Assigns a pointer map to
  // support GC and lazy deoptimization.  Assigns an environment to support
  // eager deoptimization if CAN_DEOPTIMIZE_EAGERLY.
  LInstruction* MarkAsCall(
      LInstruction* instr,
      HInstruction* hinstr,
      CanDeoptimize can_deoptimize = CANNOT_DEOPTIMIZE_EAGERLY);

  LEnvironment* CreateEnvironment(HEnvironment* hydrogen_env,
                                  int* argument_index_accumulator);

  void VisitInstruction(HInstruction* current);

  void DoBasicBlock(HBasicBlock* block, HBasicBlock* next_block);
  LInstruction* DoShift(Token::Value op, HBitwiseBinaryOperation* instr);
  LInstruction* DoArithmeticD(Token::Value op,
                              HArithmeticBinaryOperation* instr);
  LInstruction* DoArithmeticT(Token::Value op,
                              HArithmeticBinaryOperation* instr);

  LOperand* GetStoreKeyedValueOperand(HStoreKeyed* instr);

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

#endif  // V8_IA32_LITHIUM_IA32_H_
