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

#include "v8.h"

#include "codegen.h"
#include "compiler.h"
#include "debug.h"
#include "full-codegen.h"
#include "liveedit.h"
#include "macro-assembler.h"
#include "prettyprinter.h"
#include "scopes.h"
#include "scopeinfo.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

void BreakableStatementChecker::Check(Statement* stmt) {
  Visit(stmt);
}


void BreakableStatementChecker::Check(Expression* expr) {
  Visit(expr);
}


void BreakableStatementChecker::VisitDeclaration(Declaration* decl) {
}


void BreakableStatementChecker::VisitBlock(Block* stmt) {
}


void BreakableStatementChecker::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  // Check if expression is breakable.
  Visit(stmt->expression());
}


void BreakableStatementChecker::VisitEmptyStatement(EmptyStatement* stmt) {
}


void BreakableStatementChecker::VisitIfStatement(IfStatement* stmt) {
  // If the condition is breakable the if statement is breakable.
  Visit(stmt->condition());
}


void BreakableStatementChecker::VisitContinueStatement(
    ContinueStatement* stmt) {
}


void BreakableStatementChecker::VisitBreakStatement(BreakStatement* stmt) {
}


void BreakableStatementChecker::VisitReturnStatement(ReturnStatement* stmt) {
  // Return is breakable if the expression is.
  Visit(stmt->expression());
}


void BreakableStatementChecker::VisitWithStatement(WithStatement* stmt) {
  Visit(stmt->expression());
}


void BreakableStatementChecker::VisitSwitchStatement(SwitchStatement* stmt) {
  // Switch statements breakable if the tag expression is.
  Visit(stmt->tag());
}


void BreakableStatementChecker::VisitDoWhileStatement(DoWhileStatement* stmt) {
  // Mark do while as breakable to avoid adding a break slot in front of it.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitWhileStatement(WhileStatement* stmt) {
  // Mark while statements breakable if the condition expression is.
  Visit(stmt->cond());
}


void BreakableStatementChecker::VisitForStatement(ForStatement* stmt) {
  // Mark for statements breakable if the condition expression is.
  if (stmt->cond() != NULL) {
    Visit(stmt->cond());
  }
}


void BreakableStatementChecker::VisitForInStatement(ForInStatement* stmt) {
  // Mark for in statements breakable if the enumerable expression is.
  Visit(stmt->enumerable());
}


void BreakableStatementChecker::VisitTryCatchStatement(
    TryCatchStatement* stmt) {
  // Mark try catch as breakable to avoid adding a break slot in front of it.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  // Mark try finally as breakable to avoid adding a break slot in front of it.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  // The debugger statement is breakable.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitFunctionLiteral(FunctionLiteral* expr) {
}


void BreakableStatementChecker::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
}


void BreakableStatementChecker::VisitConditional(Conditional* expr) {
}


void BreakableStatementChecker::VisitVariableProxy(VariableProxy* expr) {
}


void BreakableStatementChecker::VisitLiteral(Literal* expr) {
}


void BreakableStatementChecker::VisitRegExpLiteral(RegExpLiteral* expr) {
}


void BreakableStatementChecker::VisitObjectLiteral(ObjectLiteral* expr) {
}


void BreakableStatementChecker::VisitArrayLiteral(ArrayLiteral* expr) {
}


void BreakableStatementChecker::VisitAssignment(Assignment* expr) {
  // If assigning to a property (including a global property) the assignment is
  // breakable.
  VariableProxy* proxy = expr->target()->AsVariableProxy();
  Property* prop = expr->target()->AsProperty();
  if (prop != NULL || (proxy != NULL && proxy->var()->IsUnallocated())) {
    is_breakable_ = true;
    return;
  }

  // Otherwise the assignment is breakable if the assigned value is.
  Visit(expr->value());
}


void BreakableStatementChecker::VisitThrow(Throw* expr) {
  // Throw is breakable if the expression is.
  Visit(expr->exception());
}


void BreakableStatementChecker::VisitProperty(Property* expr) {
  // Property load is breakable.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitCall(Call* expr) {
  // Function calls both through IC and call stub are breakable.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitCallNew(CallNew* expr) {
  // Function calls through new are breakable.
  is_breakable_ = true;
}


void BreakableStatementChecker::VisitCallRuntime(CallRuntime* expr) {
}


void BreakableStatementChecker::VisitUnaryOperation(UnaryOperation* expr) {
  Visit(expr->expression());
}


void BreakableStatementChecker::VisitCountOperation(CountOperation* expr) {
  Visit(expr->expression());
}


void BreakableStatementChecker::VisitBinaryOperation(BinaryOperation* expr) {
  Visit(expr->left());
  if (expr->op() != Token::AND &&
      expr->op() != Token::OR) {
    Visit(expr->right());
  }
}


void BreakableStatementChecker::VisitCompareToNull(CompareToNull* expr) {
  Visit(expr->expression());
}


void BreakableStatementChecker::VisitCompareOperation(CompareOperation* expr) {
  Visit(expr->left());
  Visit(expr->right());
}


void BreakableStatementChecker::VisitThisFunction(ThisFunction* expr) {
}


#define __ ACCESS_MASM(masm())

bool FullCodeGenerator::MakeCode(CompilationInfo* info) {
  Isolate* isolate = info->isolate();
  Handle<Script> script = info->script();
  if (!script->IsUndefined() && !script->source()->IsUndefined()) {
    int len = String::cast(script->source())->length();
    isolate->counters()->total_full_codegen_source_size()->Increment(len);
  }
  if (FLAG_trace_codegen) {
    PrintF("Full Compiler - ");
  }
  CodeGenerator::MakeCodePrologue(info);
  const int kInitialBufferSize = 4 * KB;
  MacroAssembler masm(info->isolate(), NULL, kInitialBufferSize);
#ifdef ENABLE_GDB_JIT_INTERFACE
  masm.positions_recorder()->StartGDBJITLineInfoRecording();
#endif

  FullCodeGenerator cgen(&masm);
  cgen.Generate(info);
  if (cgen.HasStackOverflow()) {
    ASSERT(!isolate->has_pending_exception());
    return false;
  }
  unsigned table_offset = cgen.EmitStackCheckTable();

  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION);
  Handle<Code> code = CodeGenerator::MakeCodeEpilogue(&masm, flags, info);
  code->set_optimizable(info->IsOptimizable());
  cgen.PopulateDeoptimizationData(code);
  code->set_has_deoptimization_support(info->HasDeoptimizationSupport());
  code->set_has_debug_break_slots(
      info->isolate()->debugger()->IsDebuggerActive());
  code->set_allow_osr_at_loop_nesting_level(0);
  code->set_stack_check_table_offset(table_offset);
  CodeGenerator::PrintCode(code, info);
  info->SetCode(code);  // may be an empty handle.
#ifdef ENABLE_GDB_JIT_INTERFACE
  if (FLAG_gdbjit && !code.is_null()) {
    GDBJITLineInfo* lineinfo =
        masm.positions_recorder()->DetachGDBJITLineInfo();

    GDBJIT(RegisterDetailedLineInfo(*code, lineinfo));
  }
#endif
  return !code.is_null();
}


unsigned FullCodeGenerator::EmitStackCheckTable() {
  // The stack check table consists of a length (in number of entries)
  // field, and then a sequence of entries.  Each entry is a pair of AST id
  // and code-relative pc offset.
  masm()->Align(kIntSize);
  unsigned offset = masm()->pc_offset();
  unsigned length = stack_checks_.length();
  __ dd(length);
  for (unsigned i = 0; i < length; ++i) {
    __ dd(stack_checks_[i].id);
    __ dd(stack_checks_[i].pc_and_state);
  }
  return offset;
}


void FullCodeGenerator::PopulateDeoptimizationData(Handle<Code> code) {
  // Fill in the deoptimization information.
  ASSERT(info_->HasDeoptimizationSupport() || bailout_entries_.is_empty());
  if (!info_->HasDeoptimizationSupport()) return;
  int length = bailout_entries_.length();
  Handle<DeoptimizationOutputData> data =
      isolate()->factory()->
      NewDeoptimizationOutputData(length, TENURED);
  for (int i = 0; i < length; i++) {
    data->SetAstId(i, Smi::FromInt(bailout_entries_[i].id));
    data->SetPcAndState(i, Smi::FromInt(bailout_entries_[i].pc_and_state));
  }
  code->set_deoptimization_data(*data);
}


void FullCodeGenerator::PrepareForBailout(Expression* node, State state) {
  PrepareForBailoutForId(node->id(), state);
}


void FullCodeGenerator::RecordJSReturnSite(Call* call) {
  // We record the offset of the function return so we can rebuild the frame
  // if the function was inlined, i.e., this is the return address in the
  // inlined function's frame.
  //
  // The state is ignored.  We defensively set it to TOS_REG, which is the
  // real state of the unoptimized code at the return site.
  PrepareForBailoutForId(call->ReturnId(), TOS_REG);
#ifdef DEBUG
  // In debug builds, mark the return so we can verify that this function
  // was called.
  ASSERT(!call->return_is_recorded_);
  call->return_is_recorded_ = true;
#endif
}


void FullCodeGenerator::PrepareForBailoutForId(int id, State state) {
  // There's no need to prepare this code for bailouts from already optimized
  // code or code that can't be optimized.
  if (!FLAG_deopt || !info_->HasDeoptimizationSupport()) return;
  unsigned pc_and_state =
      StateField::encode(state) | PcField::encode(masm_->pc_offset());
  BailoutEntry entry = { id, pc_and_state };
#ifdef DEBUG
  // Assert that we don't have multiple bailout entries for the same node.
  for (int i = 0; i < bailout_entries_.length(); i++) {
    if (bailout_entries_.at(i).id == entry.id) {
      AstPrinter printer;
      PrintF("%s", printer.PrintProgram(info_->function()));
      UNREACHABLE();
    }
  }
#endif  // DEBUG
  bailout_entries_.Add(entry);
}


void FullCodeGenerator::RecordStackCheck(int ast_id) {
  // The pc offset does not need to be encoded and packed together with a
  // state.
  BailoutEntry entry = { ast_id, masm_->pc_offset() };
  stack_checks_.Add(entry);
}


bool FullCodeGenerator::ShouldInlineSmiCase(Token::Value op) {
  // Inline smi case inside loops, but not division and modulo which
  // are too complicated and take up too much space.
  if (op == Token::DIV ||op == Token::MOD) return false;
  if (FLAG_always_inline_smi_code) return true;
  return loop_depth_ > 0;
}


void FullCodeGenerator::EffectContext::Plug(Register reg) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(Register reg) const {
  __ Move(result_register(), reg);
}


void FullCodeGenerator::StackValueContext::Plug(Register reg) const {
  __ push(reg);
  codegen()->increment_stack_height();
}


void FullCodeGenerator::TestContext::Plug(Register reg) const {
  // For simplicity we always test the accumulator register.
  __ Move(result_register(), reg);
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
  codegen()->DoTest(this);
}


void FullCodeGenerator::EffectContext::PlugTOS() const {
  __ Drop(1);
  codegen()->decrement_stack_height();
}


void FullCodeGenerator::AccumulatorValueContext::PlugTOS() const {
  __ pop(result_register());
  codegen()->decrement_stack_height();
}


void FullCodeGenerator::StackValueContext::PlugTOS() const {
}


void FullCodeGenerator::TestContext::PlugTOS() const {
  // For simplicity we always test the accumulator register.
  __ pop(result_register());
  codegen()->decrement_stack_height();
  codegen()->PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
  codegen()->DoTest(this);
}


void FullCodeGenerator::EffectContext::PrepareTest(
    Label* materialize_true,
    Label* materialize_false,
    Label** if_true,
    Label** if_false,
    Label** fall_through) const {
  // In an effect context, the true and the false case branch to the
  // same label.
  *if_true = *if_false = *fall_through = materialize_true;
}


void FullCodeGenerator::AccumulatorValueContext::PrepareTest(
    Label* materialize_true,
    Label* materialize_false,
    Label** if_true,
    Label** if_false,
    Label** fall_through) const {
  *if_true = *fall_through = materialize_true;
  *if_false = materialize_false;
}


void FullCodeGenerator::StackValueContext::PrepareTest(
    Label* materialize_true,
    Label* materialize_false,
    Label** if_true,
    Label** if_false,
    Label** fall_through) const {
  *if_true = *fall_through = materialize_true;
  *if_false = materialize_false;
}


void FullCodeGenerator::TestContext::PrepareTest(
    Label* materialize_true,
    Label* materialize_false,
    Label** if_true,
    Label** if_false,
    Label** fall_through) const {
  *if_true = true_label_;
  *if_false = false_label_;
  *fall_through = fall_through_;
}


void FullCodeGenerator::DoTest(const TestContext* context) {
  DoTest(context->condition(),
         context->true_label(),
         context->false_label(),
         context->fall_through());
}


void FullCodeGenerator::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
  int length = declarations->length();
  int global_count = 0;
  for (int i = 0; i < length; i++) {
    Declaration* decl = declarations->at(i);
    EmitDeclaration(decl->proxy(), decl->mode(), decl->fun(), &global_count);
  }

  // Batch declare global functions and variables.
  if (global_count > 0) {
    Handle<FixedArray> array =
        isolate()->factory()->NewFixedArray(2 * global_count, TENURED);
    for (int j = 0, i = 0; i < length; i++) {
      Declaration* decl = declarations->at(i);
      Variable* var = decl->proxy()->var();

      if (var->IsUnallocated()) {
        array->set(j++, *(var->name()));
        if (decl->fun() == NULL) {
          if (var->mode() == Variable::CONST) {
            // In case this is const property use the hole.
            array->set_the_hole(j++);
          } else {
            array->set_undefined(j++);
          }
        } else {
          Handle<SharedFunctionInfo> function =
              Compiler::BuildFunctionInfo(decl->fun(), script());
          // Check for stack-overflow exception.
          if (function.is_null()) {
            SetStackOverflow();
            return;
          }
          array->set(j++, *function);
        }
      }
    }
    // Invoke the platform-dependent code generator to do the actual
    // declaration the global functions and variables.
    DeclareGlobals(array);
  }
}


int FullCodeGenerator::DeclareGlobalsFlags() {
  int flags = 0;
  if (is_eval()) flags |= kDeclareGlobalsEvalFlag;
  if (is_strict_mode()) flags |= kDeclareGlobalsStrictModeFlag;
  if (is_native()) flags |= kDeclareGlobalsNativeFlag;
  return flags;
}


void FullCodeGenerator::SetFunctionPosition(FunctionLiteral* fun) {
  CodeGenerator::RecordPositions(masm_, fun->start_position());
}


void FullCodeGenerator::SetReturnPosition(FunctionLiteral* fun) {
  CodeGenerator::RecordPositions(masm_, fun->end_position() - 1);
}


void FullCodeGenerator::SetStatementPosition(Statement* stmt) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (!isolate()->debugger()->IsDebuggerActive()) {
    CodeGenerator::RecordPositions(masm_, stmt->statement_pos());
  } else {
    // Check if the statement will be breakable without adding a debug break
    // slot.
    BreakableStatementChecker checker;
    checker.Check(stmt);
    // Record the statement position right here if the statement is not
    // breakable. For breakable statements the actual recording of the
    // position will be postponed to the breakable code (typically an IC).
    bool position_recorded = CodeGenerator::RecordPositions(
        masm_, stmt->statement_pos(), !checker.is_breakable());
    // If the position recording did record a new position generate a debug
    // break slot to make the statement breakable.
    if (position_recorded) {
      Debug::GenerateSlot(masm_);
    }
  }
#else
  CodeGenerator::RecordPositions(masm_, stmt->statement_pos());
#endif
}


void FullCodeGenerator::SetExpressionPosition(Expression* expr, int pos) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (!isolate()->debugger()->IsDebuggerActive()) {
    CodeGenerator::RecordPositions(masm_, pos);
  } else {
    // Check if the expression will be breakable without adding a debug break
    // slot.
    BreakableStatementChecker checker;
    checker.Check(expr);
    // Record a statement position right here if the expression is not
    // breakable. For breakable expressions the actual recording of the
    // position will be postponed to the breakable code (typically an IC).
    // NOTE this will record a statement position for something which might
    // not be a statement. As stepping in the debugger will only stop at
    // statement positions this is used for e.g. the condition expression of
    // a do while loop.
    bool position_recorded = CodeGenerator::RecordPositions(
        masm_, pos, !checker.is_breakable());
    // If the position recording did record a new position generate a debug
    // break slot to make the statement breakable.
    if (position_recorded) {
      Debug::GenerateSlot(masm_);
    }
  }
#else
  CodeGenerator::RecordPositions(masm_, pos);
#endif
}


void FullCodeGenerator::SetStatementPosition(int pos) {
  CodeGenerator::RecordPositions(masm_, pos);
}


void FullCodeGenerator::SetSourcePosition(int pos) {
  if (pos != RelocInfo::kNoPosition) {
    masm_->positions_recorder()->RecordPosition(pos);
  }
}


// Lookup table for code generators for  special runtime calls which are
// generated inline.
#define INLINE_FUNCTION_GENERATOR_ADDRESS(Name, argc, ressize)          \
    &FullCodeGenerator::Emit##Name,

const FullCodeGenerator::InlineFunctionGenerator
  FullCodeGenerator::kInlineFunctionGenerators[] = {
    INLINE_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_ADDRESS)
    INLINE_RUNTIME_FUNCTION_LIST(INLINE_FUNCTION_GENERATOR_ADDRESS)
  };
#undef INLINE_FUNCTION_GENERATOR_ADDRESS


FullCodeGenerator::InlineFunctionGenerator
  FullCodeGenerator::FindInlineFunctionGenerator(Runtime::FunctionId id) {
    int lookup_index =
        static_cast<int>(id) - static_cast<int>(Runtime::kFirstInlineFunction);
    ASSERT(lookup_index >= 0);
    ASSERT(static_cast<size_t>(lookup_index) <
           ARRAY_SIZE(kInlineFunctionGenerators));
    return kInlineFunctionGenerators[lookup_index];
}


void FullCodeGenerator::EmitInlineRuntimeCall(CallRuntime* node) {
  ZoneList<Expression*>* args = node->arguments();
  const Runtime::Function* function = node->function();
  ASSERT(function != NULL);
  ASSERT(function->intrinsic_type == Runtime::INLINE);
  InlineFunctionGenerator generator =
      FindInlineFunctionGenerator(function->function_id);
  ((*this).*(generator))(args);
}


void FullCodeGenerator::VisitBinaryOperation(BinaryOperation* expr) {
  switch (expr->op()) {
    case Token::COMMA:
      return VisitComma(expr);
    case Token::OR:
    case Token::AND:
      return VisitLogicalExpression(expr);
    default:
      return VisitArithmeticExpression(expr);
  }
}


void FullCodeGenerator::VisitComma(BinaryOperation* expr) {
  Comment cmnt(masm_, "[ Comma");
  VisitForEffect(expr->left());
  if (context()->IsTest()) ForwardBailoutToChild(expr);
  VisitInCurrentContext(expr->right());
}


void FullCodeGenerator::VisitLogicalExpression(BinaryOperation* expr) {
  bool is_logical_and = expr->op() == Token::AND;
  Comment cmnt(masm_, is_logical_and ? "[ Logical AND" :  "[ Logical OR");
  Expression* left = expr->left();
  Expression* right = expr->right();
  int right_id = expr->RightId();
  Label done;

  if (context()->IsTest()) {
    Label eval_right;
    const TestContext* test = TestContext::cast(context());
    if (is_logical_and) {
      VisitForControl(left, &eval_right, test->false_label(), &eval_right);
    } else {
      VisitForControl(left, test->true_label(), &eval_right, &eval_right);
    }
    PrepareForBailoutForId(right_id, NO_REGISTERS);
    __ bind(&eval_right);
    ForwardBailoutToChild(expr);

  } else if (context()->IsAccumulatorValue()) {
    VisitForAccumulatorValue(left);
    // We want the value in the accumulator for the test, and on the stack in
    // case we need it.
    __ push(result_register());
    Label discard, restore;
    PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
    if (is_logical_and) {
      DoTest(left, &discard, &restore, &restore);
    } else {
      DoTest(left, &restore, &discard, &restore);
    }
    __ bind(&restore);
    __ pop(result_register());
    __ jmp(&done);
    __ bind(&discard);
    __ Drop(1);
    PrepareForBailoutForId(right_id, NO_REGISTERS);

  } else if (context()->IsStackValue()) {
    VisitForAccumulatorValue(left);
    // We want the value in the accumulator for the test, and on the stack in
    // case we need it.
    __ push(result_register());
    Label discard;
    PrepareForBailoutBeforeSplit(TOS_REG, false, NULL, NULL);
    if (is_logical_and) {
      DoTest(left, &discard, &done, &discard);
    } else {
      DoTest(left, &done, &discard, &discard);
    }
    __ bind(&discard);
    __ Drop(1);
    PrepareForBailoutForId(right_id, NO_REGISTERS);

  } else {
    ASSERT(context()->IsEffect());
    Label eval_right;
    if (is_logical_and) {
      VisitForControl(left, &eval_right, &done, &eval_right);
    } else {
      VisitForControl(left, &done, &eval_right, &eval_right);
    }
    PrepareForBailoutForId(right_id, NO_REGISTERS);
    __ bind(&eval_right);
  }

  VisitInCurrentContext(right);
  __ bind(&done);
}


void FullCodeGenerator::VisitArithmeticExpression(BinaryOperation* expr) {
  Token::Value op = expr->op();
  Comment cmnt(masm_, "[ ArithmeticExpression");
  Expression* left = expr->left();
  Expression* right = expr->right();
  OverwriteMode mode =
      left->ResultOverwriteAllowed()
      ? OVERWRITE_LEFT
      : (right->ResultOverwriteAllowed() ? OVERWRITE_RIGHT : NO_OVERWRITE);

  VisitForStackValue(left);
  VisitForAccumulatorValue(right);

  SetSourcePosition(expr->position());
  if (ShouldInlineSmiCase(op)) {
    EmitInlineSmiBinaryOp(expr, op, mode, left, right);
  } else {
    EmitBinaryOp(expr, op, mode);
  }
}


void FullCodeGenerator::ForwardBailoutToChild(Expression* expr) {
  if (!info_->HasDeoptimizationSupport()) return;
  ASSERT(context()->IsTest());
  ASSERT(expr == forward_bailout_stack_->expr());
  forward_bailout_pending_ = forward_bailout_stack_;
}


void FullCodeGenerator::VisitInCurrentContext(Expression* expr) {
  if (context()->IsTest()) {
    ForwardBailoutStack stack(expr, forward_bailout_pending_);
    ForwardBailoutStack* saved = forward_bailout_stack_;
    forward_bailout_pending_ = NULL;
    forward_bailout_stack_ = &stack;
    Visit(expr);
    forward_bailout_stack_ = saved;
  } else {
    ASSERT(forward_bailout_pending_ == NULL);
    Visit(expr);
    State state = context()->IsAccumulatorValue() ? TOS_REG : NO_REGISTERS;
    PrepareForBailout(expr, state);
    // Forwarding bailouts to children is a one shot operation. It should have
    // been processed at this point.
    ASSERT(forward_bailout_pending_ == NULL);
  }
}


void FullCodeGenerator::VisitBlock(Block* stmt) {
  Comment cmnt(masm_, "[ Block");
  NestedBlock nested_block(this, stmt);
  SetStatementPosition(stmt);

  Scope* saved_scope = scope();
  // Push a block context when entering a block with block scoped variables.
  if (stmt->block_scope() != NULL) {
    { Comment cmnt(masm_, "[ Extend block context");
      scope_ = stmt->block_scope();
      __ Push(scope_->GetSerializedScopeInfo());
      PushFunctionArgumentForContextAllocation();
      __ CallRuntime(Runtime::kPushBlockContext, 2);
      StoreToFrameField(StandardFrameConstants::kContextOffset,
                        context_register());
    }
    { Comment cmnt(masm_, "[ Declarations");
      VisitDeclarations(scope_->declarations());
    }
  }
  PrepareForBailoutForId(stmt->EntryId(), NO_REGISTERS);
  VisitStatements(stmt->statements());
  scope_ = saved_scope;
  __ bind(nested_block.break_label());
  PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);

  // Pop block context if necessary.
  if (stmt->block_scope() != NULL) {
    LoadContextField(context_register(), Context::PREVIOUS_INDEX);
    // Update local stack frame context field.
    StoreToFrameField(StandardFrameConstants::kContextOffset,
                      context_register());
  }
}


void FullCodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Comment cmnt(masm_, "[ ExpressionStatement");
  SetStatementPosition(stmt);
  VisitForEffect(stmt->expression());
}


void FullCodeGenerator::VisitEmptyStatement(EmptyStatement* stmt) {
  Comment cmnt(masm_, "[ EmptyStatement");
  SetStatementPosition(stmt);
}


void FullCodeGenerator::VisitIfStatement(IfStatement* stmt) {
  Comment cmnt(masm_, "[ IfStatement");
  SetStatementPosition(stmt);
  Label then_part, else_part, done;

  if (stmt->HasElseStatement()) {
    VisitForControl(stmt->condition(), &then_part, &else_part, &then_part);
    PrepareForBailoutForId(stmt->ThenId(), NO_REGISTERS);
    __ bind(&then_part);
    Visit(stmt->then_statement());
    __ jmp(&done);

    PrepareForBailoutForId(stmt->ElseId(), NO_REGISTERS);
    __ bind(&else_part);
    Visit(stmt->else_statement());
  } else {
    VisitForControl(stmt->condition(), &then_part, &done, &then_part);
    PrepareForBailoutForId(stmt->ThenId(), NO_REGISTERS);
    __ bind(&then_part);
    Visit(stmt->then_statement());

    PrepareForBailoutForId(stmt->ElseId(), NO_REGISTERS);
  }
  __ bind(&done);
  PrepareForBailoutForId(stmt->IfId(), NO_REGISTERS);
}


void FullCodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  Comment cmnt(masm_,  "[ ContinueStatement");
  SetStatementPosition(stmt);
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  int context_length = 0;
  // When continuing, we clobber the unpredictable value in the accumulator
  // with one that's safe for GC.  If we hit an exit from the try block of
  // try...finally on our way out, we will unconditionally preserve the
  // accumulator on the stack.
  ClearAccumulator();
  while (!current->IsContinueTarget(stmt->target())) {
    current = current->Exit(&stack_depth, &context_length);
  }
  __ Drop(stack_depth);
  if (context_length > 0) {
    while (context_length > 0) {
      LoadContextField(context_register(), Context::PREVIOUS_INDEX);
      --context_length;
    }
    StoreToFrameField(StandardFrameConstants::kContextOffset,
                      context_register());
  }

  __ jmp(current->AsIteration()->continue_label());
}


void FullCodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  Comment cmnt(masm_,  "[ BreakStatement");
  SetStatementPosition(stmt);
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  int context_length = 0;
  // When breaking, we clobber the unpredictable value in the accumulator
  // with one that's safe for GC.  If we hit an exit from the try block of
  // try...finally on our way out, we will unconditionally preserve the
  // accumulator on the stack.
  ClearAccumulator();
  while (!current->IsBreakTarget(stmt->target())) {
    current = current->Exit(&stack_depth, &context_length);
  }
  __ Drop(stack_depth);
  if (context_length > 0) {
    while (context_length > 0) {
      LoadContextField(context_register(), Context::PREVIOUS_INDEX);
      --context_length;
    }
    StoreToFrameField(StandardFrameConstants::kContextOffset,
                      context_register());
  }

  __ jmp(current->AsBreakable()->break_label());
}


void FullCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Comment cmnt(masm_, "[ ReturnStatement");
  SetStatementPosition(stmt);
  Expression* expr = stmt->expression();
  VisitForAccumulatorValue(expr);

  // Exit all nested statements.
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  int context_length = 0;
  while (current != NULL) {
    current = current->Exit(&stack_depth, &context_length);
  }
  __ Drop(stack_depth);

  EmitReturnSequence();
}


void FullCodeGenerator::VisitWithStatement(WithStatement* stmt) {
  Comment cmnt(masm_, "[ WithStatement");
  SetStatementPosition(stmt);

  VisitForStackValue(stmt->expression());
  PushFunctionArgumentForContextAllocation();
  __ CallRuntime(Runtime::kPushWithContext, 2);
  decrement_stack_height();
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());

  { WithOrCatch body(this);
    Visit(stmt->statement());
  }

  // Pop context.
  LoadContextField(context_register(), Context::PREVIOUS_INDEX);
  // Update local stack frame context field.
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
}


void FullCodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  Comment cmnt(masm_, "[ DoWhileStatement");
  SetStatementPosition(stmt);
  Label body, stack_check;

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  __ bind(&body);
  Visit(stmt->body());

  // Record the position of the do while condition and make sure it is
  // possible to break on the condition.
  __ bind(loop_statement.continue_label());
  PrepareForBailoutForId(stmt->ContinueId(), NO_REGISTERS);
  SetExpressionPosition(stmt->cond(), stmt->condition_position());
  VisitForControl(stmt->cond(),
                  &stack_check,
                  loop_statement.break_label(),
                  &stack_check);

  // Check stack before looping.
  PrepareForBailoutForId(stmt->BackEdgeId(), NO_REGISTERS);
  __ bind(&stack_check);
  EmitStackCheck(stmt);
  __ jmp(&body);

  PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
  __ bind(loop_statement.break_label());
  decrement_loop_depth();
}


void FullCodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  Comment cmnt(masm_, "[ WhileStatement");
  Label test, body;

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  // Emit the test at the bottom of the loop.
  __ jmp(&test);

  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  __ bind(&body);
  Visit(stmt->body());

  // Emit the statement position here as this is where the while
  // statement code starts.
  __ bind(loop_statement.continue_label());
  SetStatementPosition(stmt);

  // Check stack before looping.
  EmitStackCheck(stmt);

  __ bind(&test);
  VisitForControl(stmt->cond(),
                  &body,
                  loop_statement.break_label(),
                  loop_statement.break_label());

  PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
  __ bind(loop_statement.break_label());
  decrement_loop_depth();
}


void FullCodeGenerator::VisitForStatement(ForStatement* stmt) {
  Comment cmnt(masm_, "[ ForStatement");
  Label test, body;

  Iteration loop_statement(this, stmt);
  if (stmt->init() != NULL) {
    Visit(stmt->init());
  }

  increment_loop_depth();
  // Emit the test at the bottom of the loop (even if empty).
  __ jmp(&test);

  PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
  __ bind(&body);
  Visit(stmt->body());

  PrepareForBailoutForId(stmt->ContinueId(), NO_REGISTERS);
  __ bind(loop_statement.continue_label());
  SetStatementPosition(stmt);
  if (stmt->next() != NULL) {
    Visit(stmt->next());
  }

  // Emit the statement position here as this is where the for
  // statement code starts.
  SetStatementPosition(stmt);

  // Check stack before looping.
  EmitStackCheck(stmt);

  __ bind(&test);
  if (stmt->cond() != NULL) {
    VisitForControl(stmt->cond(),
                    &body,
                    loop_statement.break_label(),
                    loop_statement.break_label());
  } else {
    __ jmp(&body);
  }

  PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
  __ bind(loop_statement.break_label());
  decrement_loop_depth();
}


void FullCodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Comment cmnt(masm_, "[ TryCatchStatement");
  SetStatementPosition(stmt);
  // The try block adds a handler to the exception handler chain
  // before entering, and removes it again when exiting normally.
  // If an exception is thrown during execution of the try block,
  // control is passed to the handler, which also consumes the handler.
  // At this point, the exception is in a register, and store it in
  // the temporary local variable (prints as ".catch-var") before
  // executing the catch block. The catch block has been rewritten
  // to introduce a new scope to bind the catch variable and to remove
  // that scope again afterwards.

  Label try_handler_setup, done;
  __ Call(&try_handler_setup);
  // Try handler code, exception in result register.

  // Extend the context before executing the catch block.
  { Comment cmnt(masm_, "[ Extend catch context");
    __ Push(stmt->variable()->name());
    __ push(result_register());
    PushFunctionArgumentForContextAllocation();
    __ CallRuntime(Runtime::kPushCatchContext, 3);
    StoreToFrameField(StandardFrameConstants::kContextOffset,
                      context_register());
  }

  Scope* saved_scope = scope();
  scope_ = stmt->scope();
  ASSERT(scope_->declarations()->is_empty());
  { WithOrCatch body(this);
    Visit(stmt->catch_block());
  }
  // Restore the context.
  LoadContextField(context_register(), Context::PREVIOUS_INDEX);
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
  scope_ = saved_scope;
  __ jmp(&done);

  // Try block code. Sets up the exception handler chain.
  __ bind(&try_handler_setup);
  {
    const int delta = StackHandlerConstants::kSize / kPointerSize;
    TryCatch try_block(this);
    __ PushTryHandler(IN_JAVASCRIPT, TRY_CATCH_HANDLER);
    increment_stack_height(delta);
    Visit(stmt->try_block());
    __ PopTryHandler();
    decrement_stack_height(delta);
  }
  __ bind(&done);
}


void FullCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  Comment cmnt(masm_, "[ TryFinallyStatement");
  SetStatementPosition(stmt);
  // Try finally is compiled by setting up a try-handler on the stack while
  // executing the try body, and removing it again afterwards.
  //
  // The try-finally construct can enter the finally block in three ways:
  // 1. By exiting the try-block normally. This removes the try-handler and
  //      calls the finally block code before continuing.
  // 2. By exiting the try-block with a function-local control flow transfer
  //    (break/continue/return). The site of the, e.g., break removes the
  //    try handler and calls the finally block code before continuing
  //    its outward control transfer.
  // 3. by exiting the try-block with a thrown exception.
  //    This can happen in nested function calls. It traverses the try-handler
  //    chain and consumes the try-handler entry before jumping to the
  //    handler code. The handler code then calls the finally-block before
  //    rethrowing the exception.
  //
  // The finally block must assume a return address on top of the stack
  // (or in the link register on ARM chips) and a value (return value or
  // exception) in the result register (rax/eax/r0), both of which must
  // be preserved. The return address isn't GC-safe, so it should be
  // cooked before GC.
  Label finally_entry;
  Label try_handler_setup;
  const int original_stack_height = stack_height();

  // Setup the try-handler chain. Use a call to
  // Jump to try-handler setup and try-block code. Use call to put try-handler
  // address on stack.
  __ Call(&try_handler_setup);
  // Try handler code. Return address of call is pushed on handler stack.
  {
    // This code is only executed during stack-handler traversal when an
    // exception is thrown. The exception is in the result register, which
    // is retained by the finally block.
    // Call the finally block and then rethrow the exception if it returns.
    __ Call(&finally_entry);
    __ push(result_register());
    __ CallRuntime(Runtime::kReThrow, 1);
  }

  __ bind(&finally_entry);
  {
    // Finally block implementation.
    Finally finally_block(this);
    EnterFinallyBlock();
    set_stack_height(original_stack_height + Finally::kElementCount);
    Visit(stmt->finally_block());
    ExitFinallyBlock();  // Return to the calling code.
  }

  __ bind(&try_handler_setup);
  {
    // Setup try handler (stack pointer registers).
    const int delta = StackHandlerConstants::kSize / kPointerSize;
    TryFinally try_block(this, &finally_entry);
    __ PushTryHandler(IN_JAVASCRIPT, TRY_FINALLY_HANDLER);
    set_stack_height(original_stack_height + delta);
    Visit(stmt->try_block());
    __ PopTryHandler();
    set_stack_height(original_stack_height);
  }
  // Execute the finally block on the way out.  Clobber the unpredictable
  // value in the accumulator with one that's safe for GC.  The finally
  // block will unconditionally preserve the accumulator on the stack.
  ClearAccumulator();
  __ Call(&finally_entry);
}


void FullCodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  Comment cmnt(masm_, "[ DebuggerStatement");
  SetStatementPosition(stmt);

  __ DebugBreak();
  // Ignore the return value.
#endif
}


void FullCodeGenerator::VisitConditional(Conditional* expr) {
  Comment cmnt(masm_, "[ Conditional");
  Label true_case, false_case, done;
  VisitForControl(expr->condition(), &true_case, &false_case, &true_case);

  PrepareForBailoutForId(expr->ThenId(), NO_REGISTERS);
  __ bind(&true_case);
  SetExpressionPosition(expr->then_expression(),
                        expr->then_expression_position());
  int start_stack_height = stack_height();
  if (context()->IsTest()) {
    const TestContext* for_test = TestContext::cast(context());
    VisitForControl(expr->then_expression(),
                    for_test->true_label(),
                    for_test->false_label(),
                    NULL);
  } else {
    VisitInCurrentContext(expr->then_expression());
    __ jmp(&done);
  }

  PrepareForBailoutForId(expr->ElseId(), NO_REGISTERS);
  __ bind(&false_case);
  set_stack_height(start_stack_height);
  if (context()->IsTest()) ForwardBailoutToChild(expr);
  SetExpressionPosition(expr->else_expression(),
                        expr->else_expression_position());
  VisitInCurrentContext(expr->else_expression());
  // If control flow falls through Visit, merge it with true case here.
  if (!context()->IsTest()) {
    __ bind(&done);
  }
}


void FullCodeGenerator::VisitLiteral(Literal* expr) {
  Comment cmnt(masm_, "[ Literal");
  context()->Plug(expr->handle());
}


void FullCodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<SharedFunctionInfo> function_info =
      Compiler::BuildFunctionInfo(expr, script());
  if (function_info.is_null()) {
    SetStackOverflow();
    return;
  }
  EmitNewClosure(function_info, expr->pretenure());
}


void FullCodeGenerator::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
  Comment cmnt(masm_, "[ SharedFunctionInfoLiteral");
  EmitNewClosure(expr->shared_function_info(), false);
}


void FullCodeGenerator::VisitThrow(Throw* expr) {
  Comment cmnt(masm_, "[ Throw");
  // Throw has no effect on the stack height or the current expression context.
  // Usually the expression context is null, because throw is a statement.
  VisitForStackValue(expr->exception());
  __ CallRuntime(Runtime::kThrow, 1);
  decrement_stack_height();
  // Never returns here.
}


FullCodeGenerator::NestedStatement* FullCodeGenerator::TryCatch::Exit(
    int* stack_depth,
    int* context_length) {
  // The macros used here must preserve the result register.
  __ Drop(*stack_depth);
  __ PopTryHandler();
  *stack_depth = 0;
  return previous_;
}


bool FullCodeGenerator::TryLiteralCompare(CompareOperation* compare,
                                          Label* if_true,
                                          Label* if_false,
                                          Label* fall_through) {
  Expression *expr;
  Handle<String> check;
  if (compare->IsLiteralCompareTypeof(&expr, &check)) {
    EmitLiteralCompareTypeof(expr, check, if_true, if_false, fall_through);
    return true;
  }

  if (compare->IsLiteralCompareUndefined(&expr)) {
    EmitLiteralCompareUndefined(expr, if_true, if_false, fall_through);
    return true;
  }

  return false;
}


#undef __


} }  // namespace v8::internal
