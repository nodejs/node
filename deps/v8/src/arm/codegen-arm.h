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

#include "ic-inl.h"

namespace v8 {
namespace internal {

// Forward declarations
class CompilationInfo;
class DeferredCode;
class RegisterAllocator;
class RegisterFile;

enum InitState { CONST_INIT, NOT_CONST_INIT };
enum TypeofState { INSIDE_TYPEOF, NOT_INSIDE_TYPEOF };


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
  void SetValue(InitState init_state);

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

  // Create a code generator state based on a code generator's current
  // state.  The new state has its own pair of branch labels.
  CodeGenState(CodeGenerator* owner,
               JumpTarget* true_target,
               JumpTarget* false_target);

  // Destroy a code generator state and restore the owning code generator's
  // previous state.
  ~CodeGenState();

  JumpTarget* true_target() const { return true_target_; }
  JumpTarget* false_target() const { return false_target_; }

 private:
  CodeGenerator* owner_;
  JumpTarget* true_target_;
  JumpTarget* false_target_;
  CodeGenState* previous_;
};


// -------------------------------------------------------------------------
// Arguments allocation mode

enum ArgumentsAllocationMode {
  NO_ARGUMENTS_ALLOCATION,
  EAGER_ARGUMENTS_ALLOCATION,
  LAZY_ARGUMENTS_ALLOCATION
};


// Different nop operations are used by the code generator to detect certain
// states of the generated code.
enum NopMarkerTypes {
  NON_MARKING_NOP = 0,
  PROPERTY_ACCESS_INLINED
};


// -------------------------------------------------------------------------
// CodeGenerator

class CodeGenerator: public AstVisitor {
 public:
  // Takes a function literal, generates code for it. This function should only
  // be called by compiler.cc.
  static Handle<Code> MakeCode(CompilationInfo* info);

  // Printing of AST, etc. as requested by flags.
  static void MakeCodePrologue(CompilationInfo* info);

  // Allocate and install the code.
  static Handle<Code> MakeCodeEpilogue(MacroAssembler* masm,
                                       Code::Flags flags,
                                       CompilationInfo* info);

#ifdef ENABLE_LOGGING_AND_PROFILING
  static bool ShouldGenerateLog(Expression* type);
#endif

  static void SetFunctionInfo(Handle<JSFunction> fun,
                              FunctionLiteral* lit,
                              bool is_toplevel,
                              Handle<Script> script);

  static void RecordPositions(MacroAssembler* masm, int pos);

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

  void AddDeferred(DeferredCode* code) { deferred_.Add(code); }

  static const int kUnknownIntValue = -1;

  // If the name is an inline runtime function call return the number of
  // expected arguments. Otherwise return -1.
  static int InlineRuntimeCallArgumentsCount(Handle<String> name);

 private:
  // Construction/Destruction
  explicit CodeGenerator(MacroAssembler* masm);

  // Accessors
  inline bool is_eval();
  inline Scope* scope();

  // Generating deferred code.
  void ProcessDeferred();

  // State
  bool has_cc() const  { return cc_reg_ != al; }
  JumpTarget* true_target() const  { return state_->true_target(); }
  JumpTarget* false_target() const  { return state_->false_target(); }

  // Track loop nesting level.
  int loop_nesting() const { return loop_nesting_; }
  void IncrementLoopNesting() { loop_nesting_++; }
  void DecrementLoopNesting() { loop_nesting_--; }

  // Node visitors.
  void VisitStatements(ZoneList<Statement*>* statements);

#define DEF_VISIT(type) \
  void Visit##type(type* node);
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

  // Visit a statement and then spill the virtual frame if control flow can
  // reach the end of the statement (ie, it does not exit via break,
  // continue, return, or throw).  This function is used temporarily while
  // the code generator is being transformed.
  inline void VisitAndSpill(Statement* statement);

  // Visit a list of statements and then spill the virtual frame if control
  // flow can reach the end of the list.
  inline void VisitStatementsAndSpill(ZoneList<Statement*>* statements);

  // Main code generation function
  void Generate(CompilationInfo* info);

  // Returns the arguments allocation mode.
  ArgumentsAllocationMode ArgumentsMode();

  // Store the arguments object and allocate it if necessary.
  void StoreArgumentsObject(bool initial);

  // The following are used by class Reference.
  void LoadReference(Reference* ref);
  void UnloadReference(Reference* ref);

  static MemOperand ContextOperand(Register context, int index) {
    return MemOperand(context, Context::SlotOffset(index));
  }

  MemOperand SlotOperand(Slot* slot, Register tmp);

  MemOperand ContextSlotOperandCheckExtensions(Slot* slot,
                                               Register tmp,
                                               Register tmp2,
                                               JumpTarget* slow);

  // Expressions
  static MemOperand GlobalObject()  {
    return ContextOperand(cp, Context::GLOBAL_INDEX);
  }

  void LoadCondition(Expression* x,
                     JumpTarget* true_target,
                     JumpTarget* false_target,
                     bool force_cc);
  void Load(Expression* expr);
  void LoadGlobal();
  void LoadGlobalReceiver(Register scratch);

  // Generate code to push the value of an expression on top of the frame
  // and then spill the frame fully to memory.  This function is used
  // temporarily while the code generator is being transformed.
  inline void LoadAndSpill(Expression* expression);

  // Call LoadCondition and then spill the virtual frame unless control flow
  // cannot reach the end of the expression (ie, by emitting only
  // unconditional jumps to the control targets).
  inline void LoadConditionAndSpill(Expression* expression,
                                    JumpTarget* true_target,
                                    JumpTarget* false_target,
                                    bool force_control);

  // Read a value from a slot and leave it on top of the expression stack.
  void LoadFromSlot(Slot* slot, TypeofState typeof_state);
  void LoadFromSlotCheckForArguments(Slot* slot, TypeofState state);
  // Store the value on top of the stack to a slot.
  void StoreToSlot(Slot* slot, InitState init_state);

  // Load a named property, leaving it in r0. The receiver is passed on the
  // stack, and remains there.
  void EmitNamedLoad(Handle<String> name, bool is_contextual);

  // Load a keyed property, leaving it in r0.  The receiver and key are
  // passed on the stack, and remain there.
  void EmitKeyedLoad();

  // Store a keyed property. Key and receiver are on the stack and the value is
  // in r0. Result is returned in r0.
  void EmitKeyedStore(StaticType* key_type);

  void LoadFromGlobalSlotCheckExtensions(Slot* slot,
                                         TypeofState typeof_state,
                                         JumpTarget* slow);

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
                              int known_rhs = kUnknownIntValue);
  void VirtualFrameBinaryOperation(Token::Value op,
                                   OverwriteMode overwrite_mode,
                                   int known_rhs = kUnknownIntValue);
  void Comparison(Condition cc,
                  Expression* left,
                  Expression* right,
                  bool strict = false);

  void SmiOperation(Token::Value op,
                    Handle<Object> value,
                    bool reversed,
                    OverwriteMode mode);

  void VirtualFrameSmiOperation(Token::Value op,
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

  struct InlineRuntimeLUT {
    void (CodeGenerator::*method)(ZoneList<Expression*>*);
    const char* name;
    int nargs;
  };

  static InlineRuntimeLUT* FindInlineRuntimeLUT(Handle<String> name);
  bool CheckForInlineRuntimeCall(CallRuntime* node);
  static bool PatchInlineRuntimeEntry(Handle<String> name,
                                      const InlineRuntimeLUT& new_entry,
                                      InlineRuntimeLUT* old_entry);

  static Handle<Code> ComputeLazyCompile(int argc);
  void ProcessDeclarations(ZoneList<Declaration*>* declarations);

  static Handle<Code> ComputeCallInitialize(int argc, InLoopFlag in_loop);

  // Declare global variables and functions in the given array of
  // name/value pairs.
  void DeclareGlobals(Handle<FixedArray> pairs);

  // Instantiate the function based on the shared function info.
  void InstantiateFunction(Handle<SharedFunctionInfo> function_info);

  // Support for type checks.
  void GenerateIsSmi(ZoneList<Expression*>* args);
  void GenerateIsNonNegativeSmi(ZoneList<Expression*>* args);
  void GenerateIsArray(ZoneList<Expression*>* args);
  void GenerateIsRegExp(ZoneList<Expression*>* args);
  void GenerateIsObject(ZoneList<Expression*>* args);
  void GenerateIsFunction(ZoneList<Expression*>* args);
  void GenerateIsUndetectableObject(ZoneList<Expression*>* args);

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
  void GenerateFastCharCodeAt(ZoneList<Expression*>* args);

  // Fast support for string.charAt(n) and string[n].
  void GenerateCharFromCode(ZoneList<Expression*>* args);

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

  // Fast call for custom callbacks.
  void GenerateCallFunction(ZoneList<Expression*>* args);

  // Fast call to math functions.
  void GenerateMathPow(ZoneList<Expression*>* args);
  void GenerateMathSin(ZoneList<Expression*>* args);
  void GenerateMathCos(ZoneList<Expression*>* args);
  void GenerateMathSqrt(ZoneList<Expression*>* args);

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

  // Jump targets
  BreakTarget function_return_;

  // True if the function return is shadowed (ie, jumping to the target
  // function_return_ does not jump to the true function return, but rather
  // to some unlinking code).
  bool function_return_is_shadowed_;

  static InlineRuntimeLUT kInlineRuntimeLUT[];

  friend class VirtualFrame;
  friend class JumpTarget;
  friend class Reference;
  friend class FastCodeGenerator;
  friend class FullCodeGenerator;
  friend class FullCodeGenSyntaxChecker;

  DISALLOW_COPY_AND_ASSIGN(CodeGenerator);
};


class GenericBinaryOpStub : public CodeStub {
 public:
  GenericBinaryOpStub(Token::Value op,
                      OverwriteMode mode,
                      Register lhs,
                      Register rhs,
                      int constant_rhs = CodeGenerator::kUnknownIntValue)
      : op_(op),
        mode_(mode),
        lhs_(lhs),
        rhs_(rhs),
        constant_rhs_(constant_rhs),
        specialized_on_rhs_(RhsIsOneWeWantToOptimizeFor(op, constant_rhs)),
        runtime_operands_type_(BinaryOpIC::DEFAULT),
        name_(NULL) { }

  GenericBinaryOpStub(int key, BinaryOpIC::TypeInfo type_info)
      : op_(OpBits::decode(key)),
        mode_(ModeBits::decode(key)),
        lhs_(LhsRegister(RegisterBits::decode(key))),
        rhs_(RhsRegister(RegisterBits::decode(key))),
        constant_rhs_(KnownBitsForMinorKey(KnownIntBits::decode(key))),
        specialized_on_rhs_(RhsIsOneWeWantToOptimizeFor(op_, constant_rhs_)),
        runtime_operands_type_(type_info),
        name_(NULL) { }

 private:
  Token::Value op_;
  OverwriteMode mode_;
  Register lhs_;
  Register rhs_;
  int constant_rhs_;
  bool specialized_on_rhs_;
  BinaryOpIC::TypeInfo runtime_operands_type_;
  char* name_;

  static const int kMaxKnownRhs = 0x40000000;
  static const int kKnownRhsKeyBits = 6;

  // Minor key encoding in 17 bits.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 6> {};
  class TypeInfoBits: public BitField<int, 8, 2> {};
  class RegisterBits: public BitField<bool, 10, 1> {};
  class KnownIntBits: public BitField<int, 11, kKnownRhsKeyBits> {};

  Major MajorKey() { return GenericBinaryOp; }
  int MinorKey() {
    ASSERT((lhs_.is(r0) && rhs_.is(r1)) ||
           (lhs_.is(r1) && rhs_.is(r0)));
    // Encode the parameters in a unique 18 bit value.
    return OpBits::encode(op_)
           | ModeBits::encode(mode_)
           | KnownIntBits::encode(MinorKeyForKnownInt())
           | TypeInfoBits::encode(runtime_operands_type_)
           | RegisterBits::encode(lhs_.is(r0));
  }

  void Generate(MacroAssembler* masm);
  void HandleNonSmiBitwiseOp(MacroAssembler* masm, Register lhs, Register rhs);
  void HandleBinaryOpSlowCases(MacroAssembler* masm,
                               Label* not_smi,
                               Register lhs,
                               Register rhs,
                               const Builtins::JavaScript& builtin);
  void GenerateTypeTransition(MacroAssembler* masm);

  static bool RhsIsOneWeWantToOptimizeFor(Token::Value op, int constant_rhs) {
    if (constant_rhs == CodeGenerator::kUnknownIntValue) return false;
    if (op == Token::DIV) return constant_rhs >= 2 && constant_rhs <= 3;
    if (op == Token::MOD) {
      if (constant_rhs <= 1) return false;
      if (constant_rhs <= 10) return true;
      if (constant_rhs <= kMaxKnownRhs && IsPowerOf2(constant_rhs)) return true;
      return false;
    }
    return false;
  }

  int MinorKeyForKnownInt() {
    if (!specialized_on_rhs_) return 0;
    if (constant_rhs_ <= 10) return constant_rhs_ + 1;
    ASSERT(IsPowerOf2(constant_rhs_));
    int key = 12;
    int d = constant_rhs_;
    while ((d & 1) == 0) {
      key++;
      d >>= 1;
    }
    ASSERT(key >= 0 && key < (1 << kKnownRhsKeyBits));
    return key;
  }

  int KnownBitsForMinorKey(int key) {
    if (!key) return 0;
    if (key <= 11) return key - 1;
    int d = 1;
    while (key != 12) {
      key--;
      d <<= 1;
    }
    return d;
  }

  Register LhsRegister(bool lhs_is_r0) {
    return lhs_is_r0 ? r0 : r1;
  }

  Register RhsRegister(bool lhs_is_r0) {
    return lhs_is_r0 ? r1 : r0;
  }

  bool ShouldGenerateSmiCode() {
    return ((op_ != Token::DIV && op_ != Token::MOD) || specialized_on_rhs_) &&
        runtime_operands_type_ != BinaryOpIC::HEAP_NUMBERS &&
        runtime_operands_type_ != BinaryOpIC::STRINGS;
  }

  bool ShouldGenerateFPCode() {
    return runtime_operands_type_ != BinaryOpIC::STRINGS;
  }

  virtual int GetCodeKind() { return Code::BINARY_OP_IC; }

  virtual InlineCacheState GetICState() {
    return BinaryOpIC::ToState(runtime_operands_type_);
  }

  const char* GetName();

#ifdef DEBUG
  void Print() {
    if (!specialized_on_rhs_) {
      PrintF("GenericBinaryOpStub (%s)\n", Token::String(op_));
    } else {
      PrintF("GenericBinaryOpStub (%s by %d)\n",
             Token::String(op_),
             constant_rhs_);
    }
  }
#endif
};


class StringStubBase: public CodeStub {
 public:
  // Generate code for copying characters using a simple loop. This should only
  // be used in places where the number of characters is small and the
  // additional setup and checking in GenerateCopyCharactersLong adds too much
  // overhead. Copying of overlapping regions is not supported.
  // Dest register ends at the position after the last character written.
  void GenerateCopyCharacters(MacroAssembler* masm,
                              Register dest,
                              Register src,
                              Register count,
                              Register scratch,
                              bool ascii);

  // Generate code for copying a large number of characters. This function
  // is allowed to spend extra time setting up conditions to make copying
  // faster. Copying of overlapping regions is not supported.
  // Dest register ends at the position after the last character written.
  void GenerateCopyCharactersLong(MacroAssembler* masm,
                                  Register dest,
                                  Register src,
                                  Register count,
                                  Register scratch1,
                                  Register scratch2,
                                  Register scratch3,
                                  Register scratch4,
                                  Register scratch5,
                                  int flags);


  // Probe the symbol table for a two character string. If the string is
  // not found by probing a jump to the label not_found is performed. This jump
  // does not guarantee that the string is not in the symbol table. If the
  // string is found the code falls through with the string in register r0.
  // Contents of both c1 and c2 registers are modified. At the exit c1 is
  // guaranteed to contain halfword with low and high bytes equal to
  // initial contents of c1 and c2 respectively.
  void GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
                                            Register c1,
                                            Register c2,
                                            Register scratch1,
                                            Register scratch2,
                                            Register scratch3,
                                            Register scratch4,
                                            Register scratch5,
                                            Label* not_found);

  // Generate string hash.
  void GenerateHashInit(MacroAssembler* masm,
                        Register hash,
                        Register character);

  void GenerateHashAddCharacter(MacroAssembler* masm,
                                Register hash,
                                Register character);

  void GenerateHashGetHash(MacroAssembler* masm,
                           Register hash);
};


// Flag that indicates how to generate code for the stub StringAddStub.
enum StringAddFlags {
  NO_STRING_ADD_FLAGS = 0,
  NO_STRING_CHECK_IN_STUB = 1 << 0  // Omit string check in stub.
};


class StringAddStub: public StringStubBase {
 public:
  explicit StringAddStub(StringAddFlags flags) {
    string_check_ = ((flags & NO_STRING_CHECK_IN_STUB) == 0);
  }

 private:
  Major MajorKey() { return StringAdd; }
  int MinorKey() { return string_check_ ? 0 : 1; }

  void Generate(MacroAssembler* masm);

  // Should the stub check whether arguments are strings?
  bool string_check_;
};


class SubStringStub: public StringStubBase {
 public:
  SubStringStub() {}

 private:
  Major MajorKey() { return SubString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};



class StringCompareStub: public CodeStub {
 public:
  StringCompareStub() { }

  // Compare two flat ASCII strings and returns result in r0.
  // Does not use the stack.
  static void GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                              Register left,
                                              Register right,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              Register scratch4);

 private:
  Major MajorKey() { return StringCompare; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


// This stub can convert a signed int32 to a heap number (double).  It does
// not work for int32s that are in Smi range!  No GC occurs during this stub
// so you don't have to set up the frame.
class WriteInt32ToHeapNumberStub : public CodeStub {
 public:
  WriteInt32ToHeapNumberStub(Register the_int,
                             Register the_heap_number,
                             Register scratch)
      : the_int_(the_int),
        the_heap_number_(the_heap_number),
        scratch_(scratch) { }

 private:
  Register the_int_;
  Register the_heap_number_;
  Register scratch_;

  // Minor key encoding in 16 bits.
  class IntRegisterBits: public BitField<int, 0, 4> {};
  class HeapNumberRegisterBits: public BitField<int, 4, 4> {};
  class ScratchRegisterBits: public BitField<int, 8, 4> {};

  Major MajorKey() { return WriteInt32ToHeapNumber; }
  int MinorKey() {
    // Encode the parameters in a unique 16 bit value.
    return IntRegisterBits::encode(the_int_.code())
           | HeapNumberRegisterBits::encode(the_heap_number_.code())
           | ScratchRegisterBits::encode(scratch_.code());
  }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "WriteInt32ToHeapNumberStub"; }

#ifdef DEBUG
  void Print() { PrintF("WriteInt32ToHeapNumberStub\n"); }
#endif
};


class NumberToStringStub: public CodeStub {
 public:
  NumberToStringStub() { }

  // Generate code to do a lookup in the number string cache. If the number in
  // the register object is found in the cache the generated code falls through
  // with the result in the result register. The object and the result register
  // can be the same. If the number is not found in the cache the code jumps to
  // the label not_found with only the content of register object unchanged.
  static void GenerateLookupNumberStringCache(MacroAssembler* masm,
                                              Register object,
                                              Register result,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              bool object_is_smi,
                                              Label* not_found);

 private:
  Major MajorKey() { return NumberToString; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "NumberToStringStub"; }

#ifdef DEBUG
  void Print() {
    PrintF("NumberToStringStub\n");
  }
#endif
};


} }  // namespace v8::internal

#endif  // V8_ARM_CODEGEN_ARM_H_
