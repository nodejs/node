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

#ifndef V8_ARM_CODEGEN_ARM_H_
#define V8_ARM_CODEGEN_ARM_H_

#include "ast.h"
#include "code-stubs-arm.h"
#include "ic-inl.h"

namespace v8 {
namespace internal {

// Forward declarations
class CompilationInfo;
class DeferredCode;
class JumpTarget;
class RegisterAllocator;
class RegisterFile;

enum InitState { CONST_INIT, NOT_CONST_INIT };
enum TypeofState { INSIDE_TYPEOF, NOT_INSIDE_TYPEOF };
enum GenerateInlineSmi { DONT_GENERATE_INLINE_SMI, GENERATE_INLINE_SMI };
enum WriteBarrierCharacter { UNLIKELY_SMI, LIKELY_SMI, NEVER_NEWSPACE };


// -------------------------------------------------------------------------
// Reference support

// A reference is a C++ stack-allocated object that puts a
// reference on the virtual frame.  The reference may be consumed
// by GetValue, TakeValue, SetValue, and Codegen::UnloadReference.
// When the lifetime (scope) of a valid reference ends, it must have
// been consumed, and be in state UNLOADED.
class Reference BASE_EMBEDDED {
 public:
  // The values of the types is important, see size().
  enum Type { UNLOADED = -2, ILLEGAL = -1, SLOT = 0, NAMED = 1, KEYED = 2 };
  Reference(CodeGenerator* cgen,
            Expression* expression,
            bool persist_after_get = false);
  ~Reference();

  Expression* expression() const { return expression_; }
  Type type() const { return type_; }
  void set_type(Type value) {
    ASSERT_EQ(ILLEGAL, type_);
    type_ = value;
  }

  void set_unloaded() {
    ASSERT_NE(ILLEGAL, type_);
    ASSERT_NE(UNLOADED, type_);
    type_ = UNLOADED;
  }
  // The size the reference takes up on the stack.
  int size() const {
    return (type_ < SLOT) ? 0 : type_;
  }

  bool is_illegal() const { return type_ == ILLEGAL; }
  bool is_slot() const { return type_ == SLOT; }
  bool is_property() const { return type_ == NAMED || type_ == KEYED; }
  bool is_unloaded() const { return type_ == UNLOADED; }

  // Return the name.  Only valid for named property references.
  Handle<String> GetName();

  // Generate code to push the value of the reference on top of the
  // expression stack.  The reference is expected to be already on top of
  // the expression stack, and it is consumed by the call unless the
  // reference is for a compound assignment.
  // If the reference is not consumed, it is left in place under its value.
  void GetValue();

  // Generate code to store the value on top of the expression stack in the
  // reference.  The reference is expected to be immediately below the value
  // on the expression stack.  The  value is stored in the location specified
  // by the reference, and is left on top of the stack, after the reference
  // is popped from beneath it (unloaded).
  void SetValue(InitState init_state, WriteBarrierCharacter wb);

  // This is in preparation for something that uses the reference on the stack.
  // If we need this reference afterwards get then dup it now.  Otherwise mark
  // it as used.
  inline void DupIfPersist();

 private:
  CodeGenerator* cgen_;
  Expression* expression_;
  Type type_;
  // Keep the reference on the stack after get, so it can be used by set later.
  bool persist_after_get_;
};


// -------------------------------------------------------------------------
// Code generation state

// The state is passed down the AST by the code generator (and back up, in
// the form of the state of the label pair).  It is threaded through the
// call stack.  Constructing a state implicitly pushes it on the owning code
// generator's stack of states, and destroying one implicitly pops it.

class CodeGenState BASE_EMBEDDED {
 public:
  // Create an initial code generator state.  Destroying the initial state
  // leaves the code generator with a NULL state.
  explicit CodeGenState(CodeGenerator* owner);

  // Destroy a code generator state and restore the owning code generator's
  // previous state.
  virtual ~CodeGenState();

  virtual JumpTarget* true_target() const { return NULL; }
  virtual JumpTarget* false_target() const { return NULL; }

 protected:
  inline CodeGenerator* owner() { return owner_; }
  inline CodeGenState* previous() const { return previous_; }

 private:
  CodeGenerator* owner_;
  CodeGenState* previous_;
};


class ConditionCodeGenState : public CodeGenState {
 public:
  // Create a code generator state based on a code generator's current
  // state.  The new state has its own pair of branch labels.
  ConditionCodeGenState(CodeGenerator* owner,
                        JumpTarget* true_target,
                        JumpTarget* false_target);

  virtual JumpTarget* true_target() const { return true_target_; }
  virtual JumpTarget* false_target() const { return false_target_; }

 private:
  JumpTarget* true_target_;
  JumpTarget* false_target_;
};


class TypeInfoCodeGenState : public CodeGenState {
 public:
  TypeInfoCodeGenState(CodeGenerator* owner,
                       Slot* slot_number,
                       TypeInfo info);
  ~TypeInfoCodeGenState();

  virtual JumpTarget* true_target() const { return previous()->true_target(); }
  virtual JumpTarget* false_target() const {
    return previous()->false_target();
  }

 private:
  Slot* slot_;
  TypeInfo old_type_info_;
};


// -------------------------------------------------------------------------
// Arguments allocation mode

enum ArgumentsAllocationMode {
  NO_ARGUMENTS_ALLOCATION,
  EAGER_ARGUMENTS_ALLOCATION,
  LAZY_ARGUMENTS_ALLOCATION
};


// -------------------------------------------------------------------------
// CodeGenerator

class CodeGenerator: public AstVisitor {
 public:
  static bool MakeCode(CompilationInfo* info);

  // Printing of AST, etc. as requested by flags.
  static void MakeCodePrologue(CompilationInfo* info);

  // Allocate and install the code.
  static Handle<Code> MakeCodeEpilogue(MacroAssembler* masm,
                                       Code::Flags flags,
                                       CompilationInfo* info);

  // Print the code after compiling it.
  static void PrintCode(Handle<Code> code, CompilationInfo* info);

#ifdef ENABLE_LOGGING_AND_PROFILING
  static bool ShouldGenerateLog(Expression* type);
#endif

  static void SetFunctionInfo(Handle<JSFunction> fun,
                              FunctionLiteral* lit,
                              bool is_toplevel,
                              Handle<Script> script);

  static bool RecordPositions(MacroAssembler* masm,
                              int pos,
                              bool right_here = false);

  // Accessors
  MacroAssembler* masm() { return masm_; }
  VirtualFrame* frame() const { return frame_; }
  inline Handle<Script> script();

  bool has_valid_frame() const { return frame_ != NULL; }

  // Set the virtual frame to be new_frame, with non-frame register
  // reference counts given by non_frame_registers.  The non-frame
  // register reference counts of the old frame are returned in
  // non_frame_registers.
  void SetFrame(VirtualFrame* new_frame, RegisterFile* non_frame_registers);

  void DeleteFrame();

  RegisterAllocator* allocator() const { return allocator_; }

  CodeGenState* state() { return state_; }
  void set_state(CodeGenState* state) { state_ = state; }

  TypeInfo type_info(Slot* slot) {
    int index = NumberOfSlot(slot);
    if (index == kInvalidSlotNumber) return TypeInfo::Unknown();
    return (*type_info_)[index];
  }

  TypeInfo set_type_info(Slot* slot, TypeInfo info) {
    int index = NumberOfSlot(slot);
    ASSERT(index >= kInvalidSlotNumber);
    if (index != kInvalidSlotNumber) {
      TypeInfo previous_value = (*type_info_)[index];
      (*type_info_)[index] = info;
      return previous_value;
    }
    return TypeInfo::Unknown();
  }

  void AddDeferred(DeferredCode* code) { deferred_.Add(code); }

  // Constants related to patching of inlined load/store.
  static int GetInlinedKeyedLoadInstructionsAfterPatch() {
    return FLAG_debug_code ? 32 : 13;
  }
  static const int kInlinedKeyedStoreInstructionsAfterPatch = 5;
  static int GetInlinedNamedStoreInstructionsAfterPatch() {
    ASSERT(inlined_write_barrier_size_ != -1);
    return inlined_write_barrier_size_ + 4;
  }

 private:
  // Type of a member function that generates inline code for a native function.
  typedef void (CodeGenerator::*InlineFunctionGenerator)
      (ZoneList<Expression*>*);

  static const InlineFunctionGenerator kInlineFunctionGenerators[];

  // Construction/Destruction
  explicit CodeGenerator(MacroAssembler* masm);

  // Accessors
  inline bool is_eval();
  inline Scope* scope();

  // Generating deferred code.
  void ProcessDeferred();

  static const int kInvalidSlotNumber = -1;

  int NumberOfSlot(Slot* slot);

  // State
  bool has_cc() const { return cc_reg_ != al; }
  JumpTarget* true_target() const { return state_->true_target(); }
  JumpTarget* false_target() const { return state_->false_target(); }

  // Track loop nesting level.
  int loop_nesting() const { return loop_nesting_; }
  void IncrementLoopNesting() { loop_nesting_++; }
  void DecrementLoopNesting() { loop_nesting_--; }

  // Node visitors.
  void VisitStatements(ZoneList<Statement*>* statements);

  virtual void VisitSlot(Slot* node);
#define DEF_VISIT(type) \
  virtual void Visit##type(type* node);
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

  // Main code generation function
  void Generate(CompilationInfo* info);

  // Generate the return sequence code.  Should be called no more than
  // once per compiled function, immediately after binding the return
  // target (which can not be done more than once).  The return value should
  // be in r0.
  void GenerateReturnSequence();

  // Returns the arguments allocation mode.
  ArgumentsAllocationMode ArgumentsMode();

  // Store the arguments object and allocate it if necessary.
  void StoreArgumentsObject(bool initial);

  // The following are used by class Reference.
  void LoadReference(Reference* ref);
  void UnloadReference(Reference* ref);

  MemOperand SlotOperand(Slot* slot, Register tmp);

  MemOperand ContextSlotOperandCheckExtensions(Slot* slot,
                                               Register tmp,
                                               Register tmp2,
                                               JumpTarget* slow);

  // Expressions
  void LoadCondition(Expression* x,
                     JumpTarget* true_target,
                     JumpTarget* false_target,
                     bool force_cc);
  void Load(Expression* expr);
  void LoadGlobal();
  void LoadGlobalReceiver(Register scratch);

  // Read a value from a slot and leave it on top of the expression stack.
  void LoadFromSlot(Slot* slot, TypeofState typeof_state);
  void LoadFromSlotCheckForArguments(Slot* slot, TypeofState state);

  // Store the value on top of the stack to a slot.
  void StoreToSlot(Slot* slot, InitState init_state);

  // Support for compiling assignment expressions.
  void EmitSlotAssignment(Assignment* node);
  void EmitNamedPropertyAssignment(Assignment* node);
  void EmitKeyedPropertyAssignment(Assignment* node);

  // Load a named property, returning it in r0. The receiver is passed on the
  // stack, and remains there.
  void EmitNamedLoad(Handle<String> name, bool is_contextual);

  // Store to a named property. If the store is contextual, value is passed on
  // the frame and consumed. Otherwise, receiver and value are passed on the
  // frame and consumed. The result is returned in r0.
  void EmitNamedStore(Handle<String> name, bool is_contextual);

  // Load a keyed property, leaving it in r0.  The receiver and key are
  // passed on the stack, and remain there.
  void EmitKeyedLoad();

  // Store a keyed property. Key and receiver are on the stack and the value is
  // in r0. Result is returned in r0.
  void EmitKeyedStore(StaticType* key_type, WriteBarrierCharacter wb_info);

  void LoadFromGlobalSlotCheckExtensions(Slot* slot,
                                         TypeofState typeof_state,
                                         JumpTarget* slow);

  // Support for loading from local/global variables and arguments
  // whose location is known unless they are shadowed by
  // eval-introduced bindings. Generates no code for unsupported slot
  // types and therefore expects to fall through to the slow jump target.
  void EmitDynamicLoadFromSlotFastCase(Slot* slot,
                                       TypeofState typeof_state,
                                       JumpTarget* slow,
                                       JumpTarget* done);

  // Special code for typeof expressions: Unfortunately, we must
  // be careful when loading the expression in 'typeof'
  // expressions. We are not allowed to throw reference errors for
  // non-existing properties of the global object, so we must make it
  // look like an explicit property access, instead of an access
  // through the context chain.
  void LoadTypeofExpression(Expression* x);

  void ToBoolean(JumpTarget* true_target, JumpTarget* false_target);

  // Generate code that computes a shortcutting logical operation.
  void GenerateLogicalBooleanOperation(BinaryOperation* node);

  void GenericBinaryOperation(Token::Value op,
                              OverwriteMode overwrite_mode,
                              GenerateInlineSmi inline_smi,
                              int known_rhs =
                                  GenericBinaryOpStub::kUnknownIntValue);
  void Comparison(Condition cc,
                  Expression* left,
                  Expression* right,
                  bool strict = false);

  void SmiOperation(Token::Value op,
                    Handle<Object> value,
                    bool reversed,
                    OverwriteMode mode);

  void CallWithArguments(ZoneList<Expression*>* arguments,
                         CallFunctionFlags flags,
                         int position);

  // An optimized implementation of expressions of the form
  // x.apply(y, arguments).  We call x the applicand and y the receiver.
  // The optimization avoids allocating an arguments object if possible.
  void CallApplyLazy(Expression* applicand,
                     Expression* receiver,
                     VariableProxy* arguments,
                     int position);

  // Control flow
  void Branch(bool if_true, JumpTarget* target);
  void CheckStack();

  bool CheckForInlineRuntimeCall(CallRuntime* node);

  static Handle<Code> ComputeLazyCompile(int argc);
  void ProcessDeclarations(ZoneList<Declaration*>* declarations);

  // Declare global variables and functions in the given array of
  // name/value pairs.
  void DeclareGlobals(Handle<FixedArray> pairs);

  // Instantiate the function based on the shared function info.
  void InstantiateFunction(Handle<SharedFunctionInfo> function_info,
                           bool pretenure);

  // Support for type checks.
  void GenerateIsSmi(ZoneList<Expression*>* args);
  void GenerateIsNonNegativeSmi(ZoneList<Expression*>* args);
  void GenerateIsArray(ZoneList<Expression*>* args);
  void GenerateIsRegExp(ZoneList<Expression*>* args);
  void GenerateIsObject(ZoneList<Expression*>* args);
  void GenerateIsSpecObject(ZoneList<Expression*>* args);
  void GenerateIsFunction(ZoneList<Expression*>* args);
  void GenerateIsUndetectableObject(ZoneList<Expression*>* args);
  void GenerateIsStringWrapperSafeForDefaultValueOf(
      ZoneList<Expression*>* args);

  // Support for construct call checks.
  void GenerateIsConstructCall(ZoneList<Expression*>* args);

  // Support for arguments.length and arguments[?].
  void GenerateArgumentsLength(ZoneList<Expression*>* args);
  void GenerateArguments(ZoneList<Expression*>* args);

  // Support for accessing the class and value fields of an object.
  void GenerateClassOf(ZoneList<Expression*>* args);
  void GenerateValueOf(ZoneList<Expression*>* args);
  void GenerateSetValueOf(ZoneList<Expression*>* args);

  // Fast support for charCodeAt(n).
  void GenerateStringCharCodeAt(ZoneList<Expression*>* args);

  // Fast support for string.charAt(n) and string[n].
  void GenerateStringCharFromCode(ZoneList<Expression*>* args);

  // Fast support for string.charAt(n) and string[n].
  void GenerateStringCharAt(ZoneList<Expression*>* args);

  // Fast support for object equality testing.
  void GenerateObjectEquals(ZoneList<Expression*>* args);

  void GenerateLog(ZoneList<Expression*>* args);

  // Fast support for Math.random().
  void GenerateRandomHeapNumber(ZoneList<Expression*>* args);

  // Fast support for StringAdd.
  void GenerateStringAdd(ZoneList<Expression*>* args);

  // Fast support for SubString.
  void GenerateSubString(ZoneList<Expression*>* args);

  // Fast support for StringCompare.
  void GenerateStringCompare(ZoneList<Expression*>* args);

  // Support for direct calls from JavaScript to native RegExp code.
  void GenerateRegExpExec(ZoneList<Expression*>* args);

  void GenerateRegExpConstructResult(ZoneList<Expression*>* args);

  // Support for fast native caches.
  void GenerateGetFromCache(ZoneList<Expression*>* args);

  // Fast support for number to string.
  void GenerateNumberToString(ZoneList<Expression*>* args);

  // Fast swapping of elements.
  void GenerateSwapElements(ZoneList<Expression*>* args);

  // Fast call for custom callbacks.
  void GenerateCallFunction(ZoneList<Expression*>* args);

  // Fast call to math functions.
  void GenerateMathPow(ZoneList<Expression*>* args);
  void GenerateMathSin(ZoneList<Expression*>* args);
  void GenerateMathCos(ZoneList<Expression*>* args);
  void GenerateMathSqrt(ZoneList<Expression*>* args);
  void GenerateMathLog(ZoneList<Expression*>* args);

  void GenerateIsRegExpEquivalent(ZoneList<Expression*>* args);

  void GenerateHasCachedArrayIndex(ZoneList<Expression*>* args);
  void GenerateGetCachedArrayIndex(ZoneList<Expression*>* args);
  void GenerateFastAsciiArrayJoin(ZoneList<Expression*>* args);

  // Simple condition analysis.
  enum ConditionAnalysis {
    ALWAYS_TRUE,
    ALWAYS_FALSE,
    DONT_KNOW
  };
  ConditionAnalysis AnalyzeCondition(Expression* cond);

  // Methods used to indicate which source code is generated for. Source
  // positions are collected by the assembler and emitted with the relocation
  // information.
  void CodeForFunctionPosition(FunctionLiteral* fun);
  void CodeForReturnPosition(FunctionLiteral* fun);
  void CodeForStatementPosition(Statement* node);
  void CodeForDoWhileConditionPosition(DoWhileStatement* stmt);
  void CodeForSourcePosition(int pos);

#ifdef DEBUG
  // True if the registers are valid for entry to a block.
  bool HasValidEntryRegisters();
#endif

  List<DeferredCode*> deferred_;

  // Assembler
  MacroAssembler* masm_;  // to generate code

  CompilationInfo* info_;

  // Code generation state
  VirtualFrame* frame_;
  RegisterAllocator* allocator_;
  Condition cc_reg_;
  CodeGenState* state_;
  int loop_nesting_;

  Vector<TypeInfo>* type_info_;

  // Jump targets
  BreakTarget function_return_;

  // True if the function return is shadowed (ie, jumping to the target
  // function_return_ does not jump to the true function return, but rather
  // to some unlinking code).
  bool function_return_is_shadowed_;

  // Size of inlined write barriers generated by EmitNamedStore.
  static int inlined_write_barrier_size_;

  friend class VirtualFrame;
  friend class JumpTarget;
  friend class Reference;
  friend class FastCodeGenerator;
  friend class FullCodeGenerator;
  friend class FullCodeGenSyntaxChecker;
  friend class LCodeGen;

  DISALLOW_COPY_AND_ASSIGN(CodeGenerator);
};


} }  // namespace v8::internal

#endif  // V8_ARM_CODEGEN_ARM_H_
