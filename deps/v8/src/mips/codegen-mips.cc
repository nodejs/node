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


#include "v8.h"

#include "bootstrapper.h"
#include "codegen-inl.h"
#include "debug.h"
#include "parser.h"
#include "register-allocator-inl.h"
#include "runtime.h"
#include "scopes.h"
#include "compiler.h"



namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm_)



// -------------------------------------------------------------------------
// Platform-specific DeferredCode functions.


void DeferredCode::SaveRegisters() {
  UNIMPLEMENTED_MIPS();
}


void DeferredCode::RestoreRegisters() {
  UNIMPLEMENTED_MIPS();
}


// -------------------------------------------------------------------------
// CodeGenerator implementation

CodeGenerator::CodeGenerator(MacroAssembler* masm)
    : deferred_(8),
      masm_(masm),
      scope_(NULL),
      frame_(NULL),
      allocator_(NULL),
      cc_reg_(cc_always),
      state_(NULL),
      function_return_is_shadowed_(false) {
}


// Calling conventions:
// s8_fp: caller's frame pointer
// sp: stack pointer
// a1: called JS function
// cp: callee's context

void CodeGenerator::Generate(CompilationInfo* info, Mode mode) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitStatements(ZoneList<Statement*>* statements) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitBlock(Block* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::DeclareGlobals(Handle<FixedArray> pairs) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitDeclaration(Declaration* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitExpressionStatement(ExpressionStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitEmptyStatement(EmptyStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitIfStatement(IfStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitContinueStatement(ContinueStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitBreakStatement(BreakStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitReturnStatement(ReturnStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitWithEnterStatement(WithEnterStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitWithExitStatement(WithExitStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitSwitchStatement(SwitchStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitDoWhileStatement(DoWhileStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitWhileStatement(WhileStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitForStatement(ForStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitForInStatement(ForInStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitTryCatchStatement(TryCatchStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitDebuggerStatement(DebuggerStatement* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitFunctionLiteral(FunctionLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitConditional(Conditional* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitSlot(Slot* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitVariableProxy(VariableProxy* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitLiteral(Literal* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitRegExpLiteral(RegExpLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitObjectLiteral(ObjectLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitArrayLiteral(ArrayLiteral* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitAssignment(Assignment* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitThrow(Throw* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitProperty(Property* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCall(Call* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCallNew(CallNew* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateClassOf(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateValueOf(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateSetValueOf(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsSmi(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateLog(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsNonNegativeSmi(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


// This should generate code that performs a charCodeAt() call or returns
// undefined in order to trigger the slow case, Runtime_StringCharCodeAt.
// It is not yet implemented on ARM, so it always goes to the slow case.
void CodeGenerator::GenerateFastCharCodeAt(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsArray(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsConstructCall(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateArgumentsLength(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateArgumentsAccess(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateRandomPositiveSmi(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateObjectEquals(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsObject(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsFunction(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateIsUndetectableObject(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateStringAdd(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateSubString(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateStringCompare(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::GenerateRegExpExec(ZoneList<Expression*>* args) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCallRuntime(CallRuntime* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitUnaryOperation(UnaryOperation* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCountOperation(CountOperation* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitBinaryOperation(BinaryOperation* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitThisFunction(ThisFunction* node) {
  UNIMPLEMENTED_MIPS();
}


void CodeGenerator::VisitCompareOperation(CompareOperation* node) {
  UNIMPLEMENTED_MIPS();
}


#ifdef DEBUG
bool CodeGenerator::HasValidEntryRegisters() { return true; }
#endif


#undef __
#define __ ACCESS_MASM(masm)


// On entry a0 and a1 are the things to be compared.  On exit v0 is 0,
// positive or negative to indicate the result of the comparison.
void CompareStub::Generate(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x765);
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x790);
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x808);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x815);
}

void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x826);
}

void CEntryStub::Generate(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x831);
}

void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  UNIMPLEMENTED_MIPS();
  // Load a result.
  __ li(v0, Operand(0x1234));
  __ jr(ra);
  // Return
  __ nop();
}


// This stub performs an instanceof, calling the builtin function if
// necessary.  Uses a1 for the object, a0 for the function that it may
// be an instance of (these are fetched from the stack).
void InstanceofStub::Generate(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x845);
}


void ArgumentsAccessStub::GenerateReadLength(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x851);
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x857);
}


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  UNIMPLEMENTED_MIPS();
  __ break_(0x863);
}


const char* CompareStub::GetName() {
  UNIMPLEMENTED_MIPS();
  return NULL;  // UNIMPLEMENTED RETURN
}


int CompareStub::MinorKey() {
  // Encode the two parameters in a unique 16 bit value.
  ASSERT(static_cast<unsigned>(cc_) >> 28 < (1 << 15));
  return (static_cast<unsigned>(cc_) >> 27) | (strict_ ? 1 : 0);
}


#undef __

} }  // namespace v8::internal

