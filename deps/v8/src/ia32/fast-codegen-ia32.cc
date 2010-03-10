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

#include "codegen-inl.h"
#include "fast-codegen.h"
#include "data-flow.h"
#include "scopes.h"

namespace v8 {
namespace internal {

#define BAILOUT(reason)                         \
  do {                                          \
    if (FLAG_trace_bailout) {                   \
      PrintF("%s\n", reason);                   \
    }                                           \
    has_supported_syntax_ = false;              \
    return;                                     \
  } while (false)


#define CHECK_BAILOUT                           \
  do {                                          \
    if (!has_supported_syntax_) return;         \
  } while (false)


void FastCodeGenSyntaxChecker::Check(CompilationInfo* info) {
  info_ = info;

  // We do not specialize if we do not have a receiver or if it is not a
  // JS object with fast mode properties.
  if (!info->has_receiver()) BAILOUT("No receiver");
  if (!info->receiver()->IsJSObject()) BAILOUT("Receiver is not an object");
  Handle<JSObject> object = Handle<JSObject>::cast(info->receiver());
  if (!object->HasFastProperties()) BAILOUT("Receiver is in dictionary mode");

  // We do not support stack or heap slots (both of which require
  // allocation).
  Scope* scope = info->scope();
  if (scope->num_stack_slots() > 0) {
    BAILOUT("Function has stack-allocated locals");
  }
  if (scope->num_heap_slots() > 0) {
    BAILOUT("Function has context-allocated locals");
  }

  VisitDeclarations(scope->declarations());
  CHECK_BAILOUT;

  // We do not support empty function bodies.
  if (info->function()->body()->is_empty()) {
    BAILOUT("Function has an empty body");
  }
  VisitStatements(info->function()->body());
}


void FastCodeGenSyntaxChecker::VisitDeclarations(
    ZoneList<Declaration*>* decls) {
  if (!decls->is_empty()) BAILOUT("Function has declarations");
}


void FastCodeGenSyntaxChecker::VisitStatements(ZoneList<Statement*>* stmts) {
  if (stmts->length() != 1) {
    BAILOUT("Function body is not a singleton statement.");
  }
  Visit(stmts->at(0));
}


void FastCodeGenSyntaxChecker::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void FastCodeGenSyntaxChecker::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void FastCodeGenSyntaxChecker::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void FastCodeGenSyntaxChecker::VisitEmptyStatement(EmptyStatement* stmt) {
  // Supported.
}


void FastCodeGenSyntaxChecker::VisitIfStatement(IfStatement* stmt) {
  BAILOUT("IfStatement");
}


void FastCodeGenSyntaxChecker::VisitContinueStatement(ContinueStatement* stmt) {
  BAILOUT("Continuestatement");
}


void FastCodeGenSyntaxChecker::VisitBreakStatement(BreakStatement* stmt) {
  BAILOUT("BreakStatement");
}


void FastCodeGenSyntaxChecker::VisitReturnStatement(ReturnStatement* stmt) {
  BAILOUT("ReturnStatement");
}


void FastCodeGenSyntaxChecker::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  BAILOUT("WithEnterStatement");
}


void FastCodeGenSyntaxChecker::VisitWithExitStatement(WithExitStatement* stmt) {
  BAILOUT("WithExitStatement");
}


void FastCodeGenSyntaxChecker::VisitSwitchStatement(SwitchStatement* stmt) {
  BAILOUT("SwitchStatement");
}


void FastCodeGenSyntaxChecker::VisitDoWhileStatement(DoWhileStatement* stmt) {
  BAILOUT("DoWhileStatement");
}


void FastCodeGenSyntaxChecker::VisitWhileStatement(WhileStatement* stmt) {
  BAILOUT("WhileStatement");
}


void FastCodeGenSyntaxChecker::VisitForStatement(ForStatement* stmt) {
  BAILOUT("ForStatement");
}


void FastCodeGenSyntaxChecker::VisitForInStatement(ForInStatement* stmt) {
  BAILOUT("ForInStatement");
}


void FastCodeGenSyntaxChecker::VisitTryCatchStatement(TryCatchStatement* stmt) {
  BAILOUT("TryCatchStatement");
}


void FastCodeGenSyntaxChecker::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  BAILOUT("TryFinallyStatement");
}


void FastCodeGenSyntaxChecker::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  BAILOUT("DebuggerStatement");
}


void FastCodeGenSyntaxChecker::VisitFunctionLiteral(FunctionLiteral* expr) {
  BAILOUT("FunctionLiteral");
}


void FastCodeGenSyntaxChecker::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  BAILOUT("FunctionBoilerplateLiteral");
}


void FastCodeGenSyntaxChecker::VisitConditional(Conditional* expr) {
  BAILOUT("Conditional");
}


void FastCodeGenSyntaxChecker::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void FastCodeGenSyntaxChecker::VisitVariableProxy(VariableProxy* expr) {
  // Only global variable references are supported.
  Variable* var = expr->var();
  if (!var->is_global() || var->is_this()) BAILOUT("Non-global variable");

  // Check if the global variable is existing and non-deletable.
  if (info()->has_global_object()) {
    LookupResult lookup;
    info()->global_object()->Lookup(*expr->name(), &lookup);
    if (!lookup.IsProperty()) {
      BAILOUT("Non-existing global variable");
    }
    // We do not handle global variables with accessors or interceptors.
    if (lookup.type() != NORMAL) {
      BAILOUT("Global variable with accessors or interceptors.");
    }
    // We do not handle deletable global variables.
    if (!lookup.IsDontDelete()) {
      BAILOUT("Deletable global variable");
    }
  }
}


void FastCodeGenSyntaxChecker::VisitLiteral(Literal* expr) {
  BAILOUT("Literal");
}


void FastCodeGenSyntaxChecker::VisitRegExpLiteral(RegExpLiteral* expr) {
  BAILOUT("RegExpLiteral");
}


void FastCodeGenSyntaxChecker::VisitObjectLiteral(ObjectLiteral* expr) {
  BAILOUT("ObjectLiteral");
}


void FastCodeGenSyntaxChecker::VisitArrayLiteral(ArrayLiteral* expr) {
  BAILOUT("ArrayLiteral");
}


void FastCodeGenSyntaxChecker::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  BAILOUT("CatchExtensionObject");
}


void FastCodeGenSyntaxChecker::VisitAssignment(Assignment* expr) {
  // Simple assignments to (named) this properties are supported.
  if (expr->op() != Token::ASSIGN) BAILOUT("Non-simple assignment");

  Property* prop = expr->target()->AsProperty();
  if (prop == NULL) BAILOUT("Non-property assignment");
  VariableProxy* proxy = prop->obj()->AsVariableProxy();
  if (proxy == NULL || !proxy->var()->is_this()) {
    BAILOUT("Non-this-property assignment");
  }
  if (!prop->key()->IsPropertyName()) {
    BAILOUT("Non-named-property assignment");
  }

  // We will only specialize for fields on the object itself.
  // Expression::IsPropertyName implies that the name is a literal
  // symbol but we do not assume that.
  Literal* key = prop->key()->AsLiteral();
  if (key != NULL && key->handle()->IsString()) {
    Handle<Object> receiver = info()->receiver();
    Handle<String> name = Handle<String>::cast(key->handle());
    LookupResult lookup;
    receiver->Lookup(*name, &lookup);
    if (!lookup.IsProperty()) {
      BAILOUT("Assigned property not found at compile time");
    }
    if (lookup.holder() != *receiver) BAILOUT("Non-own property assignment");
    if (!lookup.type() == FIELD) BAILOUT("Non-field property assignment");
  } else {
    UNREACHABLE();
    BAILOUT("Unexpected non-string-literal property key");
  }

  Visit(expr->value());
}


void FastCodeGenSyntaxChecker::VisitThrow(Throw* expr) {
  BAILOUT("Throw");
}


void FastCodeGenSyntaxChecker::VisitProperty(Property* expr) {
  // We support named this property references.
  VariableProxy* proxy = expr->obj()->AsVariableProxy();
  if (proxy == NULL || !proxy->var()->is_this()) {
    BAILOUT("Non-this-property reference");
  }
  if (!expr->key()->IsPropertyName()) {
    BAILOUT("Non-named-property reference");
  }

  // We will only specialize for fields on the object itself.
  // Expression::IsPropertyName implies that the name is a literal
  // symbol but we do not assume that.
  Literal* key = expr->key()->AsLiteral();
  if (key != NULL && key->handle()->IsString()) {
    Handle<Object> receiver = info()->receiver();
    Handle<String> name = Handle<String>::cast(key->handle());
    LookupResult lookup;
    receiver->Lookup(*name, &lookup);
    if (!lookup.IsProperty()) {
      BAILOUT("Referenced property not found at compile time");
    }
    if (lookup.holder() != *receiver) BAILOUT("Non-own property reference");
    if (!lookup.type() == FIELD) BAILOUT("Non-field property reference");
  } else {
    UNREACHABLE();
    BAILOUT("Unexpected non-string-literal property key");
  }
}


void FastCodeGenSyntaxChecker::VisitCall(Call* expr) {
  BAILOUT("Call");
}


void FastCodeGenSyntaxChecker::VisitCallNew(CallNew* expr) {
  BAILOUT("CallNew");
}


void FastCodeGenSyntaxChecker::VisitCallRuntime(CallRuntime* expr) {
  BAILOUT("CallRuntime");
}


void FastCodeGenSyntaxChecker::VisitUnaryOperation(UnaryOperation* expr) {
  BAILOUT("UnaryOperation");
}


void FastCodeGenSyntaxChecker::VisitCountOperation(CountOperation* expr) {
  BAILOUT("CountOperation");
}


void FastCodeGenSyntaxChecker::VisitBinaryOperation(BinaryOperation* expr) {
  // We support bitwise OR.
  switch (expr->op()) {
    case Token::COMMA:
      BAILOUT("BinaryOperation COMMA");
    case Token::OR:
      BAILOUT("BinaryOperation OR");
    case Token::AND:
      BAILOUT("BinaryOperation AND");

    case Token::BIT_OR:
      // We support expressions nested on the left because they only require
      // a pair of registers to keep all intermediate values in registers
      // (i.e., the expression stack has height no more than two).
      if (!expr->right()->IsLeaf()) BAILOUT("expression nested on right");

      // We do not allow subexpressions with side effects because we
      // (currently) bail out to the beginning of the full function.  The
      // only expressions with side effects that we would otherwise handle
      // are assignments.
      if (expr->left()->AsAssignment() != NULL ||
          expr->right()->AsAssignment() != NULL) {
        BAILOUT("subexpression of binary operation has side effects");
      }

      Visit(expr->left());
      CHECK_BAILOUT;
      Visit(expr->right());
      break;

    case Token::BIT_XOR:
      BAILOUT("BinaryOperation BIT_XOR");
    case Token::BIT_AND:
      BAILOUT("BinaryOperation BIT_AND");
    case Token::SHL:
      BAILOUT("BinaryOperation SHL");
    case Token::SAR:
      BAILOUT("BinaryOperation SAR");
    case Token::SHR:
      BAILOUT("BinaryOperation SHR");
    case Token::ADD:
      BAILOUT("BinaryOperation ADD");
    case Token::SUB:
      BAILOUT("BinaryOperation SUB");
    case Token::MUL:
      BAILOUT("BinaryOperation MUL");
    case Token::DIV:
      BAILOUT("BinaryOperation DIV");
    case Token::MOD:
      BAILOUT("BinaryOperation MOD");
    default:
      UNREACHABLE();
  }
}


void FastCodeGenSyntaxChecker::VisitCompareOperation(CompareOperation* expr) {
  BAILOUT("CompareOperation");
}


void FastCodeGenSyntaxChecker::VisitThisFunction(ThisFunction* expr) {
  BAILOUT("ThisFunction");
}

#undef BAILOUT
#undef CHECK_BAILOUT


#define __ ACCESS_MASM(masm())

Handle<Code> FastCodeGenerator::MakeCode(CompilationInfo* info) {
  // Label the AST before calling MakeCodePrologue, so AST node numbers are
  // printed with the AST.
  AstLabeler labeler;
  labeler.Label(info);

  LivenessAnalyzer analyzer;
  analyzer.Analyze(info->function());

  CodeGenerator::MakeCodePrologue(info);

  const int kInitialBufferSize = 4 * KB;
  MacroAssembler masm(NULL, kInitialBufferSize);

  // Generate the fast-path code.
  FastCodeGenerator fast_cgen(&masm);
  fast_cgen.Generate(info);
  if (fast_cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }

  // Generate the full code for the function in bailout mode, using the same
  // macro assembler.
  CodeGenerator cgen(&masm);
  CodeGeneratorScope scope(&cgen);
  info->set_mode(CompilationInfo::SECONDARY);
  cgen.Generate(info);
  if (cgen.HasStackOverflow()) {
    ASSERT(!Top::has_pending_exception());
    return Handle<Code>::null();
  }

  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION, NOT_IN_LOOP);
  return CodeGenerator::MakeCodeEpilogue(&masm, flags, info);
}


Register FastCodeGenerator::accumulator0() { return eax; }
Register FastCodeGenerator::accumulator1() { return edx; }
Register FastCodeGenerator::scratch0() { return ecx; }
Register FastCodeGenerator::scratch1() { return edi; }
Register FastCodeGenerator::receiver_reg() { return ebx; }
Register FastCodeGenerator::context_reg() { return esi; }


void FastCodeGenerator::EmitLoadReceiver() {
  // Offset 2 is due to return address and saved frame pointer.
  int index = 2 + function()->scope()->num_parameters();
  __ mov(receiver_reg(), Operand(ebp, index * kPointerSize));
}


void FastCodeGenerator::EmitGlobalVariableLoad(Handle<Object> cell) {
  ASSERT(!destination().is(no_reg));
  ASSERT(cell->IsJSGlobalPropertyCell());

  __ mov(destination(), Immediate(cell));
  __ mov(destination(),
         FieldOperand(destination(), JSGlobalPropertyCell::kValueOffset));
  if (FLAG_debug_code) {
    __ cmp(destination(), Factory::the_hole_value());
    __ Check(not_equal, "DontDelete cells can't contain the hole");
  }

  // The loaded value is not known to be a smi.
  clear_as_smi(destination());
}


void FastCodeGenerator::EmitThisPropertyStore(Handle<String> name) {
  LookupResult lookup;
  info()->receiver()->Lookup(*name, &lookup);

  ASSERT(lookup.holder() == *info()->receiver());
  ASSERT(lookup.type() == FIELD);
  Handle<Map> map(Handle<HeapObject>::cast(info()->receiver())->map());
  int index = lookup.GetFieldIndex() - map->inobject_properties();
  int offset = index * kPointerSize;

  // We will emit the write barrier unless the stored value is statically
  // known to be a smi.
  bool needs_write_barrier = !is_smi(accumulator0());

  // Perform the store.  Negative offsets are inobject properties.
  if (offset < 0) {
    offset += map->instance_size();
    __ mov(FieldOperand(receiver_reg(), offset), accumulator0());
    if (needs_write_barrier) {
      // Preserve receiver from write barrier.
      __ mov(scratch0(), receiver_reg());
    }
  } else {
    offset += FixedArray::kHeaderSize;
    __ mov(scratch0(),
           FieldOperand(receiver_reg(), JSObject::kPropertiesOffset));
    __ mov(FieldOperand(scratch0(), offset), accumulator0());
  }

  if (needs_write_barrier) {
    if (destination().is(no_reg)) {
      // After RecordWrite accumulator0 is only accidently a smi, but it is
      // already marked as not known to be one.
      __ RecordWrite(scratch0(), offset, accumulator0(), scratch1());
    } else {
      // Copy the value to the other accumulator to preserve a copy from the
      // write barrier. One of the accumulators is available as a scratch
      // register.  Neither is a smi.
      __ mov(accumulator1(), accumulator0());
      clear_as_smi(accumulator1());
      Register value_scratch = other_accumulator(destination());
      __ RecordWrite(scratch0(), offset, value_scratch, scratch1());
    }
  } else if (destination().is(accumulator1())) {
    __ mov(accumulator1(), accumulator0());
    // Is a smi because we do not need the write barrier.
    set_as_smi(accumulator1());
  }
}


void FastCodeGenerator::EmitThisPropertyLoad(Handle<String> name) {
  ASSERT(!destination().is(no_reg));
  LookupResult lookup;
  info()->receiver()->Lookup(*name, &lookup);

  ASSERT(lookup.holder() == *info()->receiver());
  ASSERT(lookup.type() == FIELD);
  Handle<Map> map(Handle<HeapObject>::cast(info()->receiver())->map());
  int index = lookup.GetFieldIndex() - map->inobject_properties();
  int offset = index * kPointerSize;

  // Perform the load.  Negative offsets are inobject properties.
  if (offset < 0) {
    offset += map->instance_size();
    __ mov(destination(), FieldOperand(receiver_reg(), offset));
  } else {
    offset += FixedArray::kHeaderSize;
    __ mov(scratch0(),
           FieldOperand(receiver_reg(), JSObject::kPropertiesOffset));
    __ mov(destination(), FieldOperand(scratch0(), offset));
  }

  // The loaded value is not known to be a smi.
  clear_as_smi(destination());
}


void FastCodeGenerator::EmitBitOr() {
  if (is_smi(accumulator0()) && is_smi(accumulator1())) {
    // If both operands are known to be a smi then there is no need to check
    // the operands or result.  There is no need to perform the operation in
    // an effect context.
    if (!destination().is(no_reg)) {
      // Leave the result in the destination register.  Bitwise or is
      // commutative.
      __ or_(destination(), Operand(other_accumulator(destination())));
    }
  } else {
    // Left is in accumulator1, right in accumulator0.
    Label* bailout = NULL;
    if (destination().is(accumulator0())) {
      __ mov(scratch0(), accumulator0());
      __ or_(destination(), Operand(accumulator1()));  // Or is commutative.
      __ test(destination(), Immediate(kSmiTagMask));
      bailout = info()->AddBailout(accumulator1(), scratch0());  // Left, right.
    } else if (destination().is(accumulator1())) {
      __ mov(scratch0(), accumulator1());
      __ or_(destination(), Operand(accumulator0()));
      __ test(destination(), Immediate(kSmiTagMask));
      bailout = info()->AddBailout(scratch0(), accumulator0());
    } else {
      ASSERT(destination().is(no_reg));
      __ mov(scratch0(), accumulator1());
      __ or_(scratch0(), Operand(accumulator0()));
      __ test(scratch0(), Immediate(kSmiTagMask));
      bailout = info()->AddBailout(accumulator1(), accumulator0());
    }
    __ j(not_zero, bailout, not_taken);
  }

  // If we didn't bailout, the result (in fact, both inputs too) is known to
  // be a smi.
  set_as_smi(accumulator0());
  set_as_smi(accumulator1());
}


void FastCodeGenerator::Generate(CompilationInfo* compilation_info) {
  ASSERT(info_ == NULL);
  info_ = compilation_info;
  Comment cmnt(masm_, "[ function compiled by fast code generator");

  // Save the caller's frame pointer and set up our own.
  Comment prologue_cmnt(masm(), ";; Prologue");
  __ push(ebp);
  __ mov(ebp, esp);
  __ push(esi);  // Context.
  __ push(edi);  // Closure.
  // Note that we keep a live register reference to esi (context) at this
  // point.

  Label* bailout_to_beginning = info()->AddBailout();
  // Receiver (this) is allocated to a fixed register.
  if (info()->has_this_properties()) {
    Comment cmnt(masm(), ";; MapCheck(this)");
    if (FLAG_print_ir) {
      PrintF("#: MapCheck(this)\n");
    }
    ASSERT(info()->has_receiver() && info()->receiver()->IsHeapObject());
    Handle<HeapObject> object = Handle<HeapObject>::cast(info()->receiver());
    Handle<Map> map(object->map());
    EmitLoadReceiver();
    __ CheckMap(receiver_reg(), map, bailout_to_beginning, false);
  }

  // If there is a global variable access check if the global object is the
  // same as at lazy-compilation time.
  if (info()->has_globals()) {
    Comment cmnt(masm(), ";; MapCheck(GLOBAL)");
    if (FLAG_print_ir) {
      PrintF("#: MapCheck(GLOBAL)\n");
    }
    ASSERT(info()->has_global_object());
    Handle<Map> map(info()->global_object()->map());
    __ mov(scratch0(), CodeGenerator::GlobalObject());
    __ CheckMap(scratch0(), map, bailout_to_beginning, true);
  }

  VisitStatements(function()->body());

  Comment return_cmnt(masm(), ";; Return(<undefined>)");
  if (FLAG_print_ir) {
    PrintF("#: Return(<undefined>)\n");
  }
  __ mov(eax, Factory::undefined_value());
  __ mov(esp, ebp);
  __ pop(ebp);
  __ ret((scope()->num_parameters() + 1) * kPointerSize);
}


void FastCodeGenerator::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void FastCodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Visit(stmt->expression());
}


void FastCodeGenerator::VisitEmptyStatement(EmptyStatement* stmt) {
  // Nothing to do.
}


void FastCodeGenerator::VisitIfStatement(IfStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWithEnterStatement(WithEnterStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWithExitStatement(WithExitStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitSwitchStatement(SwitchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitForStatement(ForStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitForInStatement(ForInStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitConditional(Conditional* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  ASSERT(expr->var()->is_global() && !expr->var()->is_this());
  // Check if we can compile a global variable load directly from the cell.
  ASSERT(info()->has_global_object());
  LookupResult lookup;
  info()->global_object()->Lookup(*expr->name(), &lookup);
  // We only support normal (non-accessor/interceptor) DontDelete properties
  // for now.
  ASSERT(lookup.IsProperty());
  ASSERT_EQ(NORMAL, lookup.type());
  ASSERT(lookup.IsDontDelete());
  Handle<Object> cell(info()->global_object()->GetPropertyCell(&lookup));

  // Global variable lookups do not have side effects, so we do not need to
  // emit code if we are in an effect context.
  if (!destination().is(no_reg)) {
    Comment cmnt(masm(), ";; Global");
    if (FLAG_print_ir) {
      SmartPointer<char> name = expr->name()->ToCString();
      PrintF("%d: t%d = Global(%s)  // last_use = %d\n", expr->num(),
             expr->num(), *name, expr->var_def()->last_use()->num());
    }
    EmitGlobalVariableLoad(cell);
  }
}


void FastCodeGenerator::VisitLiteral(Literal* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitObjectLiteral(ObjectLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitArrayLiteral(ArrayLiteral* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitAssignment(Assignment* expr) {
  // Known to be a simple this property assignment.  Effectively a unary
  // operation.
  { Register my_destination = destination();
    set_destination(accumulator0());
    Visit(expr->value());
    set_destination(my_destination);
  }

  Property* prop = expr->target()->AsProperty();
  ASSERT_NOT_NULL(prop);
  ASSERT_NOT_NULL(prop->obj()->AsVariableProxy());
  ASSERT(prop->obj()->AsVariableProxy()->var()->is_this());
  ASSERT(prop->key()->IsPropertyName());
  Handle<String> name =
      Handle<String>::cast(prop->key()->AsLiteral()->handle());

  Comment cmnt(masm(), ";; Store to this");
  if (FLAG_print_ir) {
    SmartPointer<char> name_string = name->ToCString();
    PrintF("%d: ", expr->num());
    if (!destination().is(no_reg)) PrintF("t%d = ", expr->num());
    PrintF("Store(this, \"%s\", t%d)  // last_use(this) = %d\n", *name_string,
           expr->value()->num(),
           expr->var_def()->last_use()->num());
  }

  EmitThisPropertyStore(name);
}


void FastCodeGenerator::VisitThrow(Throw* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitProperty(Property* expr) {
  ASSERT_NOT_NULL(expr->obj()->AsVariableProxy());
  ASSERT(expr->obj()->AsVariableProxy()->var()->is_this());
  ASSERT(expr->key()->IsPropertyName());
  if (!destination().is(no_reg)) {
    Handle<String> name =
        Handle<String>::cast(expr->key()->AsLiteral()->handle());

    Comment cmnt(masm(), ";; Load from this");
    if (FLAG_print_ir) {
      SmartPointer<char> name_string = name->ToCString();
      PrintF("%d: t%d = Load(this, \"%s\")  // last_use(this) = %d\n",
             expr->num(), expr->num(), *name_string,
             expr->var_def()->last_use()->num());
    }
    EmitThisPropertyLoad(name);
  }
}


void FastCodeGenerator::VisitCall(Call* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCallNew(CallNew* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitUnaryOperation(UnaryOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitCountOperation(CountOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitBinaryOperation(BinaryOperation* expr) {
  // We support limited binary operations: bitwise OR only allowed to be
  // nested on the left.
  ASSERT(expr->op() == Token::BIT_OR);
  ASSERT(expr->right()->IsLeaf());

  { Register my_destination = destination();
    set_destination(accumulator1());
    Visit(expr->left());
    set_destination(accumulator0());
    Visit(expr->right());
    set_destination(my_destination);
  }

  Comment cmnt(masm(), ";; BIT_OR");
  if (FLAG_print_ir) {
    PrintF("%d: ", expr->num());
    if (!destination().is(no_reg)) PrintF("t%d = ", expr->num());
    PrintF("BIT_OR(t%d, t%d)\n", expr->left()->num(), expr->right()->num());
  }
  EmitBitOr();
}


void FastCodeGenerator::VisitCompareOperation(CompareOperation* expr) {
  UNREACHABLE();
}


void FastCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  UNREACHABLE();
}

#undef __


} }  // namespace v8::internal
