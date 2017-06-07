// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/full-codegen/full-codegen.h"

#include "src/ast/ast-numbering.h"
#include "src/ast/ast.h"
#include "src/ast/prettyprinter.h"
#include "src/ast/scopes.h"
#include "src/code-factory.h"
#include "src/codegen.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/frames-inl.h"
#include "src/globals.h"
#include "src/isolate-inl.h"
#include "src/macro-assembler-inl.h"
#include "src/macro-assembler.h"
#include "src/snapshot/snapshot.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm())

class FullCodegenCompilationJob final : public CompilationJob {
 public:
  explicit FullCodegenCompilationJob(CompilationInfo* info)
      : CompilationJob(info->isolate(), info, "Full-Codegen") {}

  bool can_execute_on_background_thread() const override { return false; }

  CompilationJob::Status PrepareJobImpl() final { return SUCCEEDED; }

  CompilationJob::Status ExecuteJobImpl() final {
    DCHECK(ThreadId::Current().Equals(isolate()->thread_id()));
    return FullCodeGenerator::MakeCode(info(), stack_limit()) ? SUCCEEDED
                                                              : FAILED;
  }

  CompilationJob::Status FinalizeJobImpl() final { return SUCCEEDED; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FullCodegenCompilationJob);
};

FullCodeGenerator::FullCodeGenerator(MacroAssembler* masm,
                                     CompilationInfo* info,
                                     uintptr_t stack_limit)
    : masm_(masm),
      info_(info),
      isolate_(info->isolate()),
      zone_(info->zone()),
      scope_(info->scope()),
      nesting_stack_(NULL),
      loop_depth_(0),
      operand_stack_depth_(0),
      globals_(NULL),
      context_(NULL),
      bailout_entries_(info->HasDeoptimizationSupport()
                           ? info->literal()->ast_node_count()
                           : 0,
                       info->zone()),
      back_edges_(2, info->zone()),
      source_position_table_builder_(info->zone(),
                                     info->SourcePositionRecordingMode()),
      ic_total_count_(0) {
  DCHECK(!info->IsStub());
  Initialize(stack_limit);
}

// static
CompilationJob* FullCodeGenerator::NewCompilationJob(CompilationInfo* info) {
  return new FullCodegenCompilationJob(info);
}

// static
bool FullCodeGenerator::MakeCode(CompilationInfo* info) {
  return MakeCode(info, info->isolate()->stack_guard()->real_climit());
}

// static
bool FullCodeGenerator::MakeCode(CompilationInfo* info, uintptr_t stack_limit) {
  Isolate* isolate = info->isolate();

  DCHECK(!info->shared_info()->must_use_ignition_turbo());
  DCHECK(!FLAG_minimal);
  RuntimeCallTimerScope runtimeTimer(isolate,
                                     &RuntimeCallStats::CompileFullCode);
  TimerEventScope<TimerEventCompileFullCode> timer(info->isolate());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.CompileFullCode");

  Handle<Script> script = info->script();
  if (!script->IsUndefined(isolate) &&
      !script->source()->IsUndefined(isolate)) {
    int len = String::cast(script->source())->length();
    isolate->counters()->total_full_codegen_source_size()->Increment(len);
  }
  CodeGenerator::MakeCodePrologue(info, "full");
  const int kInitialBufferSize = 4 * KB;
  MacroAssembler masm(info->isolate(), NULL, kInitialBufferSize,
                      CodeObjectRequired::kYes);
  if (info->will_serialize()) masm.enable_serializer();

  FullCodeGenerator cgen(&masm, info, stack_limit);
  cgen.Generate();
  if (cgen.HasStackOverflow()) {
    DCHECK(!isolate->has_pending_exception());
    return false;
  }
  unsigned table_offset = cgen.EmitBackEdgeTable();

  Handle<Code> code =
      CodeGenerator::MakeCodeEpilogue(&masm, nullptr, info, masm.CodeObject());
  cgen.PopulateDeoptimizationData(code);
  cgen.PopulateTypeFeedbackInfo(code);
  code->set_has_deoptimization_support(info->HasDeoptimizationSupport());
  code->set_has_reloc_info_for_serialization(info->will_serialize());
  code->set_allow_osr_at_loop_nesting_level(0);
  code->set_profiler_ticks(0);
  code->set_back_edge_table_offset(table_offset);
  Handle<ByteArray> source_positions =
      cgen.source_position_table_builder_.ToSourcePositionTable(
          isolate, Handle<AbstractCode>::cast(code));
  code->set_source_position_table(*source_positions);
  CodeGenerator::PrintCode(code, info);
  info->SetCode(code);

#ifdef DEBUG
  // Check that no context-specific object has been embedded.
  code->VerifyEmbeddedObjects(Code::kNoContextSpecificPointers);
#endif  // DEBUG
  return true;
}


unsigned FullCodeGenerator::EmitBackEdgeTable() {
  // The back edge table consists of a length (in number of entries)
  // field, and then a sequence of entries.  Each entry is a pair of AST id
  // and code-relative pc offset.
  masm()->Align(kPointerSize);
  unsigned offset = masm()->pc_offset();
  unsigned length = back_edges_.length();
  __ dd(length);
  for (unsigned i = 0; i < length; ++i) {
    __ dd(back_edges_[i].id.ToInt());
    __ dd(back_edges_[i].pc);
    __ dd(back_edges_[i].loop_depth);
  }
  return offset;
}


void FullCodeGenerator::PopulateDeoptimizationData(Handle<Code> code) {
  // Fill in the deoptimization information.
  DCHECK(info_->HasDeoptimizationSupport() || bailout_entries_.is_empty());
  if (!info_->HasDeoptimizationSupport()) return;
  int length = bailout_entries_.length();
  Handle<DeoptimizationOutputData> data =
      DeoptimizationOutputData::New(isolate(), length, TENURED);
  for (int i = 0; i < length; i++) {
    data->SetAstId(i, bailout_entries_[i].id);
    data->SetPcAndState(i, Smi::FromInt(bailout_entries_[i].pc_and_state));
  }
  code->set_deoptimization_data(*data);
}


void FullCodeGenerator::PopulateTypeFeedbackInfo(Handle<Code> code) {
  Handle<TypeFeedbackInfo> info = isolate()->factory()->NewTypeFeedbackInfo();
  info->set_ic_total_count(ic_total_count_);
  DCHECK(!isolate()->heap()->InNewSpace(*info));
  code->set_type_feedback_info(*info);
}


bool FullCodeGenerator::MustCreateObjectLiteralWithRuntime(
    ObjectLiteral* expr) const {
  return masm()->serializer_enabled() || !expr->IsFastCloningSupported();
}


bool FullCodeGenerator::MustCreateArrayLiteralWithRuntime(
    ArrayLiteral* expr) const {
  return !expr->IsFastCloningSupported();
}

void FullCodeGenerator::Initialize(uintptr_t stack_limit) {
  InitializeAstVisitor(stack_limit);
  masm_->set_emit_debug_code(FLAG_debug_code);
  masm_->set_predictable_code_size(true);
}

void FullCodeGenerator::PrepareForBailout(Expression* node,
                                          BailoutState state) {
  PrepareForBailoutForId(node->id(), state);
}

void FullCodeGenerator::CallIC(Handle<Code> code, TypeFeedbackId ast_id) {
  ic_total_count_++;
  __ Call(code, RelocInfo::CODE_TARGET, ast_id);
}

void FullCodeGenerator::CallLoadIC(FeedbackSlot slot, Handle<Object> name) {
  DCHECK(name->IsName());
  __ Move(LoadDescriptor::NameRegister(), name);

  EmitLoadSlot(LoadDescriptor::SlotRegister(), slot);

  Handle<Code> code = CodeFactory::LoadIC(isolate()).code();
  __ Call(code, RelocInfo::CODE_TARGET);
  RestoreContext();
}

void FullCodeGenerator::CallStoreIC(FeedbackSlot slot, Handle<Object> name,
                                    StoreICKind store_ic_kind) {
  DCHECK(name->IsName());
  __ Move(StoreDescriptor::NameRegister(), name);

  STATIC_ASSERT(!StoreDescriptor::kPassLastArgsOnStack ||
                StoreDescriptor::kStackArgumentsCount == 2);
  if (StoreDescriptor::kPassLastArgsOnStack) {
    __ Push(StoreDescriptor::ValueRegister());
    EmitPushSlot(slot);
  } else {
    EmitLoadSlot(StoreDescriptor::SlotRegister(), slot);
  }

  Handle<Code> code;
  switch (store_ic_kind) {
    case kStoreOwn:
      DCHECK_EQ(FeedbackSlotKind::kStoreOwnNamed,
                feedback_vector_spec()->GetKind(slot));
      code = CodeFactory::StoreOwnIC(isolate()).code();
      break;
    case kStoreGlobal:
      // Ensure that language mode is in sync with the IC slot kind.
      DCHECK_EQ(
          GetLanguageModeFromSlotKind(feedback_vector_spec()->GetKind(slot)),
          language_mode());
      code = CodeFactory::StoreGlobalIC(isolate(), language_mode()).code();
      break;
    case kStoreNamed:
      // Ensure that language mode is in sync with the IC slot kind.
      DCHECK_EQ(
          GetLanguageModeFromSlotKind(feedback_vector_spec()->GetKind(slot)),
          language_mode());
      code = CodeFactory::StoreIC(isolate(), language_mode()).code();
      break;
  }
  __ Call(code, RelocInfo::CODE_TARGET);
  RestoreContext();
}

void FullCodeGenerator::CallKeyedStoreIC(FeedbackSlot slot) {
  STATIC_ASSERT(!StoreDescriptor::kPassLastArgsOnStack ||
                StoreDescriptor::kStackArgumentsCount == 2);
  if (StoreDescriptor::kPassLastArgsOnStack) {
    __ Push(StoreDescriptor::ValueRegister());
    EmitPushSlot(slot);
  } else {
    EmitLoadSlot(StoreDescriptor::SlotRegister(), slot);
  }

  // Ensure that language mode is in sync with the IC slot kind.
  DCHECK_EQ(GetLanguageModeFromSlotKind(feedback_vector_spec()->GetKind(slot)),
            language_mode());
  Handle<Code> code =
      CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
  __ Call(code, RelocInfo::CODE_TARGET);
  RestoreContext();
}

void FullCodeGenerator::RecordJSReturnSite(Call* call) {
  // We record the offset of the function return so we can rebuild the frame
  // if the function was inlined, i.e., this is the return address in the
  // inlined function's frame.
  //
  // The bailout state is ignored.  We defensively set it to TOS_REGISTER, which
  // is the real state of the unoptimized code at the return site.
  PrepareForBailoutForId(call->ReturnId(), BailoutState::TOS_REGISTER);
#ifdef DEBUG
  // In debug builds, mark the return so we can verify that this function
  // was called.
  DCHECK(!call->return_is_recorded_);
  call->return_is_recorded_ = true;
#endif
}

void FullCodeGenerator::PrepareForBailoutForId(BailoutId id,
                                               BailoutState state) {
  // There's no need to prepare this code for bailouts from already optimized
  // code or code that can't be optimized.
  if (!info_->HasDeoptimizationSupport()) return;
  unsigned pc_and_state =
      BailoutStateField::encode(state) | PcField::encode(masm_->pc_offset());
  DCHECK(Smi::IsValid(pc_and_state));
#ifdef DEBUG
  for (int i = 0; i < bailout_entries_.length(); ++i) {
    DCHECK(bailout_entries_[i].id != id);
  }
#endif
  BailoutEntry entry = { id, pc_and_state };
  bailout_entries_.Add(entry, zone());
}


void FullCodeGenerator::RecordBackEdge(BailoutId ast_id) {
  // The pc offset does not need to be encoded and packed together with a state.
  DCHECK(masm_->pc_offset() > 0);
  DCHECK(loop_depth() > 0);
  uint8_t depth = Min(loop_depth(), AbstractCode::kMaxLoopNestingMarker);
  BackEdgeEntry entry =
      { ast_id, static_cast<unsigned>(masm_->pc_offset()), depth };
  back_edges_.Add(entry, zone());
}


bool FullCodeGenerator::ShouldInlineSmiCase(Token::Value op) {
  // Inline smi case inside loops, but not division and modulo which
  // are too complicated and take up too much space.
  if (op == Token::DIV ||op == Token::MOD) return false;
  if (FLAG_always_inline_smi_code) return true;
  return loop_depth_ > 0;
}


void FullCodeGenerator::EffectContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
}


void FullCodeGenerator::AccumulatorValueContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  codegen()->GetVar(result_register(), var);
}


void FullCodeGenerator::TestContext::Plug(Variable* var) const {
  DCHECK(var->IsStackAllocated() || var->IsContextSlot());
  // For simplicity we always test the accumulator register.
  codegen()->GetVar(result_register(), var);
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
  codegen()->DoTest(this);
}


void FullCodeGenerator::EffectContext::Plug(Register reg) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(Register reg) const {
  __ Move(result_register(), reg);
}


void FullCodeGenerator::StackValueContext::Plug(Register reg) const {
  codegen()->PushOperand(reg);
}


void FullCodeGenerator::TestContext::Plug(Register reg) const {
  // For simplicity we always test the accumulator register.
  __ Move(result_register(), reg);
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
  codegen()->DoTest(this);
}


void FullCodeGenerator::EffectContext::Plug(bool flag) const {}

void FullCodeGenerator::EffectContext::DropAndPlug(int count,
                                                   Register reg) const {
  DCHECK(count > 0);
  codegen()->DropOperands(count);
}

void FullCodeGenerator::AccumulatorValueContext::DropAndPlug(
    int count, Register reg) const {
  DCHECK(count > 0);
  codegen()->DropOperands(count);
  __ Move(result_register(), reg);
}

void FullCodeGenerator::TestContext::DropAndPlug(int count,
                                                 Register reg) const {
  DCHECK(count > 0);
  // For simplicity we always test the accumulator register.
  codegen()->DropOperands(count);
  __ Move(result_register(), reg);
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
  codegen()->DoTest(this);
}

void FullCodeGenerator::EffectContext::PlugTOS() const {
  codegen()->DropOperands(1);
}


void FullCodeGenerator::AccumulatorValueContext::PlugTOS() const {
  codegen()->PopOperand(result_register());
}


void FullCodeGenerator::StackValueContext::PlugTOS() const {
}


void FullCodeGenerator::TestContext::PlugTOS() const {
  // For simplicity we always test the accumulator register.
  codegen()->PopOperand(result_register());
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
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

void FullCodeGenerator::VisitDeclarations(Declaration::List* declarations) {
  ZoneList<Handle<Object> >* saved_globals = globals_;
  ZoneList<Handle<Object> > inner_globals(10, zone());
  globals_ = &inner_globals;

  AstVisitor<FullCodeGenerator>::VisitDeclarations(declarations);

  if (!globals_->is_empty()) {
    // Invoke the platform-dependent code generator to do the actual
    // declaration of the global functions and variables.
    Handle<FixedArray> array =
       isolate()->factory()->NewFixedArray(globals_->length(), TENURED);
    for (int i = 0; i < globals_->length(); ++i)
      array->set(i, *globals_->at(i));
    DeclareGlobals(array);
  }

  globals_ = saved_globals;
}


void FullCodeGenerator::VisitVariableProxy(VariableProxy* expr) {
  Comment cmnt(masm_, "[ VariableProxy");
  EmitVariableLoad(expr);
}

void FullCodeGenerator::EmitGlobalVariableLoad(VariableProxy* proxy,
                                               TypeofMode typeof_mode) {
  Variable* var = proxy->var();
  DCHECK(var->IsUnallocated());
  __ Move(LoadDescriptor::NameRegister(), var->name());

  FeedbackSlot slot = proxy->VariableFeedbackSlot();
  // Ensure that typeof mode is in sync with the IC slot kind.
  DCHECK_EQ(GetTypeofModeFromSlotKind(feedback_vector_spec()->GetKind(slot)),
            typeof_mode);

  EmitLoadSlot(LoadGlobalDescriptor::SlotRegister(), slot);
  Handle<Code> code = CodeFactory::LoadGlobalIC(isolate(), typeof_mode).code();
  __ Call(code, RelocInfo::CODE_TARGET);
  RestoreContext();
}

void FullCodeGenerator::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* declaration) {
  Visit(declaration->statement());
}


int FullCodeGenerator::DeclareGlobalsFlags() {
  return info_->GetDeclareGlobalsFlags();
}

void FullCodeGenerator::PushOperand(Handle<Object> handle) {
  OperandStackDepthIncrement(1);
  __ Push(handle);
}

void FullCodeGenerator::PushOperand(Smi* smi) {
  OperandStackDepthIncrement(1);
  __ Push(smi);
}

void FullCodeGenerator::PushOperand(Register reg) {
  OperandStackDepthIncrement(1);
  __ Push(reg);
}

void FullCodeGenerator::PopOperand(Register reg) {
  OperandStackDepthDecrement(1);
  __ Pop(reg);
}

void FullCodeGenerator::DropOperands(int count) {
  OperandStackDepthDecrement(count);
  __ Drop(count);
}

void FullCodeGenerator::CallRuntimeWithOperands(Runtime::FunctionId id) {
  OperandStackDepthDecrement(Runtime::FunctionForId(id)->nargs);
  __ CallRuntime(id);
}

void FullCodeGenerator::OperandStackDepthIncrement(int count) {
  DCHECK_IMPLIES(!HasStackOverflow(), operand_stack_depth_ >= 0);
  DCHECK_GE(count, 0);
  operand_stack_depth_ += count;
}

void FullCodeGenerator::OperandStackDepthDecrement(int count) {
  DCHECK_IMPLIES(!HasStackOverflow(), operand_stack_depth_ >= count);
  DCHECK_GE(count, 0);
  operand_stack_depth_ -= count;
}

void FullCodeGenerator::EmitSubString(CallRuntime* expr) {
  // Load the arguments on the stack and call the stub.
  SubStringStub stub(isolate());
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 3);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  __ CallStub(&stub);
  RestoreContext();
  OperandStackDepthDecrement(3);
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitIntrinsicAsStubCall(CallRuntime* expr,
                                                const Callable& callable) {
  ZoneList<Expression*>* args = expr->arguments();
  int param_count = callable.descriptor().GetRegisterParameterCount();
  DCHECK_EQ(args->length(), param_count);

  if (param_count > 0) {
    int last = param_count - 1;
    // Put all but last arguments on stack.
    for (int i = 0; i < last; i++) {
      VisitForStackValue(args->at(i));
    }
    // The last argument goes to the accumulator.
    VisitForAccumulatorValue(args->at(last));

    // Move the arguments to the registers, as required by the stub.
    __ Move(callable.descriptor().GetRegisterParameter(last),
            result_register());
    for (int i = last; i-- > 0;) {
      PopOperand(callable.descriptor().GetRegisterParameter(i));
    }
  }
  __ Call(callable.code(), RelocInfo::CODE_TARGET);

  // Reload the context register after the call as i.e. TurboFan code stubs
  // won't preserve the context register.
  LoadFromFrameField(StandardFrameConstants::kContextOffset,
                     context_register());
  context()->Plug(result_register());
}


void FullCodeGenerator::EmitToString(CallRuntime* expr) {
  EmitIntrinsicAsStubCall(expr, CodeFactory::ToString(isolate()));
}


void FullCodeGenerator::EmitToLength(CallRuntime* expr) {
  EmitIntrinsicAsStubCall(expr, CodeFactory::ToLength(isolate()));
}

void FullCodeGenerator::EmitToInteger(CallRuntime* expr) {
  EmitIntrinsicAsStubCall(expr, CodeFactory::ToInteger(isolate()));
}

void FullCodeGenerator::EmitToNumber(CallRuntime* expr) {
  EmitIntrinsicAsStubCall(expr, CodeFactory::ToNumber(isolate()));
}


void FullCodeGenerator::EmitToObject(CallRuntime* expr) {
  EmitIntrinsicAsStubCall(expr, CodeFactory::ToObject(isolate()));
}


void FullCodeGenerator::EmitHasProperty() {
  Callable callable = CodeFactory::HasProperty(isolate());
  PopOperand(callable.descriptor().GetRegisterParameter(1));
  PopOperand(callable.descriptor().GetRegisterParameter(0));
  __ Call(callable.code(), RelocInfo::CODE_TARGET);
  RestoreContext();
}

void FullCodeGenerator::RecordStatementPosition(int pos) {
  DCHECK_NE(kNoSourcePosition, pos);
  source_position_table_builder_.AddPosition(masm_->pc_offset(),
                                             SourcePosition(pos), true);
}

void FullCodeGenerator::RecordPosition(int pos) {
  DCHECK_NE(kNoSourcePosition, pos);
  source_position_table_builder_.AddPosition(masm_->pc_offset(),
                                             SourcePosition(pos), false);
}


void FullCodeGenerator::SetFunctionPosition(FunctionLiteral* fun) {
  RecordPosition(fun->start_position());
}


void FullCodeGenerator::SetReturnPosition(FunctionLiteral* fun) {
  // For default constructors, start position equals end position, and there
  // is no source code besides the class literal.
  RecordStatementPosition(fun->return_position());
  if (info_->is_debug()) {
    // Always emit a debug break slot before a return.
    DebugCodegen::GenerateSlot(masm_, RelocInfo::DEBUG_BREAK_SLOT_AT_RETURN);
  }
}


void FullCodeGenerator::SetStatementPosition(
    Statement* stmt, FullCodeGenerator::InsertBreak insert_break) {
  if (stmt->position() == kNoSourcePosition) return;
  RecordStatementPosition(stmt->position());
  if (insert_break == INSERT_BREAK && info_->is_debug() &&
      !stmt->IsDebuggerStatement()) {
    DebugCodegen::GenerateSlot(masm_, RelocInfo::DEBUG_BREAK_SLOT_AT_POSITION);
  }
}

void FullCodeGenerator::SetExpressionPosition(Expression* expr) {
  if (expr->position() == kNoSourcePosition) return;
  RecordPosition(expr->position());
}


void FullCodeGenerator::SetExpressionAsStatementPosition(Expression* expr) {
  if (expr->position() == kNoSourcePosition) return;
  RecordStatementPosition(expr->position());
  if (info_->is_debug()) {
    DebugCodegen::GenerateSlot(masm_, RelocInfo::DEBUG_BREAK_SLOT_AT_POSITION);
  }
}

void FullCodeGenerator::SetCallPosition(Expression* expr,
                                        TailCallMode tail_call_mode) {
  if (expr->position() == kNoSourcePosition) return;
  RecordPosition(expr->position());
  if (info_->is_debug()) {
    RelocInfo::Mode mode = (tail_call_mode == TailCallMode::kAllow)
                               ? RelocInfo::DEBUG_BREAK_SLOT_AT_TAIL_CALL
                               : RelocInfo::DEBUG_BREAK_SLOT_AT_CALL;
    // Always emit a debug break slot before a call.
    DebugCodegen::GenerateSlot(masm_, mode);
  }
}


void FullCodeGenerator::VisitSuperPropertyReference(
    SuperPropertyReference* super) {
  __ CallRuntime(Runtime::kThrowUnsupportedSuperError);
  // Even though this expression doesn't produce a value, we need to simulate
  // plugging of the value context to ensure stack depth tracking is in sync.
  if (context()->IsStackValue()) OperandStackDepthIncrement(1);
}


void FullCodeGenerator::VisitSuperCallReference(SuperCallReference* super) {
  // Handled by VisitCall
  UNREACHABLE();
}


void FullCodeGenerator::EmitDebugBreakInOptimizedCode(CallRuntime* expr) {
  context()->Plug(handle(Smi::kZero, isolate()));
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


void FullCodeGenerator::VisitInDuplicateContext(Expression* expr) {
  if (context()->IsEffect()) {
    VisitForEffect(expr);
  } else if (context()->IsAccumulatorValue()) {
    VisitForAccumulatorValue(expr);
  } else if (context()->IsStackValue()) {
    VisitForStackValue(expr);
  } else if (context()->IsTest()) {
    const TestContext* test = TestContext::cast(context());
    VisitForControl(expr, test->true_label(), test->false_label(),
                    test->fall_through());
  }
}


void FullCodeGenerator::VisitComma(BinaryOperation* expr) {
  Comment cmnt(masm_, "[ Comma");
  VisitForEffect(expr->left());
  VisitInDuplicateContext(expr->right());
}


void FullCodeGenerator::VisitLogicalExpression(BinaryOperation* expr) {
  bool is_logical_and = expr->op() == Token::AND;
  Comment cmnt(masm_, is_logical_and ? "[ Logical AND" :  "[ Logical OR");
  Expression* left = expr->left();
  Expression* right = expr->right();
  BailoutId right_id = expr->RightId();
  Label done;

  if (context()->IsTest()) {
    Label eval_right;
    const TestContext* test = TestContext::cast(context());
    if (is_logical_and) {
      VisitForControl(left, &eval_right, test->false_label(), &eval_right);
    } else {
      VisitForControl(left, test->true_label(), &eval_right, &eval_right);
    }
    PrepareForBailoutForId(right_id, BailoutState::NO_REGISTERS);
    __ bind(&eval_right);

  } else if (context()->IsAccumulatorValue()) {
    VisitForAccumulatorValue(left);
    // We want the value in the accumulator for the test, and on the stack in
    // case we need it.
    __ Push(result_register());
    Label discard, restore;
    if (is_logical_and) {
      DoTest(left, &discard, &restore, &restore);
    } else {
      DoTest(left, &restore, &discard, &restore);
    }
    __ bind(&restore);
    __ Pop(result_register());
    __ jmp(&done);
    __ bind(&discard);
    __ Drop(1);
    PrepareForBailoutForId(right_id, BailoutState::NO_REGISTERS);

  } else if (context()->IsStackValue()) {
    VisitForAccumulatorValue(left);
    // We want the value in the accumulator for the test, and on the stack in
    // case we need it.
    __ Push(result_register());
    Label discard;
    if (is_logical_and) {
      DoTest(left, &discard, &done, &discard);
    } else {
      DoTest(left, &done, &discard, &discard);
    }
    __ bind(&discard);
    __ Drop(1);
    PrepareForBailoutForId(right_id, BailoutState::NO_REGISTERS);

  } else {
    DCHECK(context()->IsEffect());
    Label eval_right;
    if (is_logical_and) {
      VisitForControl(left, &eval_right, &done, &eval_right);
    } else {
      VisitForControl(left, &done, &eval_right, &eval_right);
    }
    PrepareForBailoutForId(right_id, BailoutState::NO_REGISTERS);
    __ bind(&eval_right);
  }

  VisitInDuplicateContext(right);
  __ bind(&done);
}


void FullCodeGenerator::VisitArithmeticExpression(BinaryOperation* expr) {
  Token::Value op = expr->op();
  Comment cmnt(masm_, "[ ArithmeticExpression");
  Expression* left = expr->left();
  Expression* right = expr->right();

  VisitForStackValue(left);
  VisitForAccumulatorValue(right);

  SetExpressionPosition(expr);
  if (ShouldInlineSmiCase(op)) {
    EmitInlineSmiBinaryOp(expr, op, left, right);
  } else {
    EmitBinaryOp(expr, op);
  }
}

void FullCodeGenerator::VisitProperty(Property* expr) {
  Comment cmnt(masm_, "[ Property");
  SetExpressionPosition(expr);

  Expression* key = expr->key();

  if (key->IsPropertyName()) {
    DCHECK(!expr->IsSuperAccess());
    VisitForAccumulatorValue(expr->obj());
    __ Move(LoadDescriptor::ReceiverRegister(), result_register());
    EmitNamedPropertyLoad(expr);
  } else {
    DCHECK(!expr->IsSuperAccess());
    VisitForStackValue(expr->obj());
    VisitForAccumulatorValue(expr->key());
    __ Move(LoadDescriptor::NameRegister(), result_register());
    PopOperand(LoadDescriptor::ReceiverRegister());
    EmitKeyedPropertyLoad(expr);
  }
  PrepareForBailoutForId(expr->LoadId(), BailoutState::TOS_REGISTER);
  context()->Plug(result_register());
}

void FullCodeGenerator::VisitForTypeofValue(Expression* expr) {
  VariableProxy* proxy = expr->AsVariableProxy();
  DCHECK(!context()->IsEffect());
  DCHECK(!context()->IsTest());

  if (proxy != NULL && proxy->var()->IsUnallocated()) {
    EmitVariableLoad(proxy, INSIDE_TYPEOF);
    PrepareForBailout(proxy, BailoutState::TOS_REGISTER);
  } else {
    // This expression cannot throw a reference error at the top level.
    VisitInDuplicateContext(expr);
  }
}


void FullCodeGenerator::VisitBlock(Block* stmt) {
  Comment cmnt(masm_, "[ Block");
  NestedBlock nested_block(this, stmt);

  {
    EnterBlockScopeIfNeeded block_scope_state(
        this, stmt->scope(), stmt->EntryId(), stmt->DeclsId(), stmt->ExitId());
    VisitStatements(stmt->statements());
    __ bind(nested_block.break_label());
  }
}


void FullCodeGenerator::VisitDoExpression(DoExpression* expr) {
  Comment cmnt(masm_, "[ Do Expression");
  SetExpressionPosition(expr);
  VisitBlock(expr->block());
  VisitInDuplicateContext(expr->result());
}


void FullCodeGenerator::VisitExpressionStatement(ExpressionStatement* stmt) {
  Comment cmnt(masm_, "[ ExpressionStatement");
  SetStatementPosition(stmt);
  VisitForEffect(stmt->expression());
}


void FullCodeGenerator::VisitEmptyStatement(EmptyStatement* stmt) {
  Comment cmnt(masm_, "[ EmptyStatement");
}


void FullCodeGenerator::VisitIfStatement(IfStatement* stmt) {
  Comment cmnt(masm_, "[ IfStatement");
  SetStatementPosition(stmt);
  Label then_part, else_part, done;

  if (stmt->HasElseStatement()) {
    VisitForControl(stmt->condition(), &then_part, &else_part, &then_part);
    PrepareForBailoutForId(stmt->ThenId(), BailoutState::NO_REGISTERS);
    __ bind(&then_part);
    Visit(stmt->then_statement());
    __ jmp(&done);

    PrepareForBailoutForId(stmt->ElseId(), BailoutState::NO_REGISTERS);
    __ bind(&else_part);
    Visit(stmt->else_statement());
  } else {
    VisitForControl(stmt->condition(), &then_part, &done, &then_part);
    PrepareForBailoutForId(stmt->ThenId(), BailoutState::NO_REGISTERS);
    __ bind(&then_part);
    Visit(stmt->then_statement());

    PrepareForBailoutForId(stmt->ElseId(), BailoutState::NO_REGISTERS);
  }
  __ bind(&done);
  PrepareForBailoutForId(stmt->IfId(), BailoutState::NO_REGISTERS);
}

void FullCodeGenerator::EmitContinue(Statement* target) {
  NestedStatement* current = nesting_stack_;
  int context_length = 0;
  // When continuing, we clobber the unpredictable value in the accumulator
  // with one that's safe for GC.
  ClearAccumulator();
  while (!current->IsContinueTarget(target)) {
    if (HasStackOverflow()) return;
    current = current->Exit(&context_length);
  }
  int stack_depth = current->GetStackDepthAtTarget();
  int stack_drop = operand_stack_depth_ - stack_depth;
  DCHECK_GE(stack_drop, 0);
  __ Drop(stack_drop);
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

void FullCodeGenerator::VisitContinueStatement(ContinueStatement* stmt) {
  Comment cmnt(masm_, "[ ContinueStatement");
  SetStatementPosition(stmt);
  EmitContinue(stmt->target());
}

void FullCodeGenerator::EmitBreak(Statement* target) {
  NestedStatement* current = nesting_stack_;
  int context_length = 0;
  // When breaking, we clobber the unpredictable value in the accumulator
  // with one that's safe for GC.
  ClearAccumulator();
  while (!current->IsBreakTarget(target)) {
    if (HasStackOverflow()) return;
    current = current->Exit(&context_length);
  }
  int stack_depth = current->GetStackDepthAtTarget();
  int stack_drop = operand_stack_depth_ - stack_depth;
  DCHECK_GE(stack_drop, 0);
  __ Drop(stack_drop);
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

void FullCodeGenerator::VisitBreakStatement(BreakStatement* stmt) {
  Comment cmnt(masm_, "[ BreakStatement");
  SetStatementPosition(stmt);
  EmitBreak(stmt->target());
}

void FullCodeGenerator::EmitUnwindAndReturn() {
  NestedStatement* current = nesting_stack_;
  int context_length = 0;
  while (current != NULL) {
    if (HasStackOverflow()) return;
    current = current->Exit(&context_length);
  }
  EmitReturnSequence();
}

void FullCodeGenerator::EmitNewClosure(Handle<SharedFunctionInfo> info,
                                       FeedbackSlot slot, bool pretenure) {
  // If slot is invalid, then it's a native function literal and we
  // can pass the empty array or empty literal array, something like that...

  // If we're running with the --always-opt or the --prepare-always-opt
  // flag, we need to use the runtime function so that the new function
  // we are creating here gets a chance to have its code optimized and
  // doesn't just get a copy of the existing unoptimized code.
  if (!FLAG_always_opt && !FLAG_prepare_always_opt && !pretenure &&
      scope()->is_function_scope()) {
    Callable callable = CodeFactory::FastNewClosure(isolate());
    __ Move(callable.descriptor().GetRegisterParameter(0), info);
    __ EmitLoadFeedbackVector(callable.descriptor().GetRegisterParameter(1));
    __ Move(callable.descriptor().GetRegisterParameter(2), SmiFromSlot(slot));
    __ Call(callable.code(), RelocInfo::CODE_TARGET);
  } else {
    __ Push(info);
    __ EmitLoadFeedbackVector(result_register());
    __ Push(result_register());
    __ Push(SmiFromSlot(slot));
    __ CallRuntime(pretenure ? Runtime::kNewClosure_Tenured
                             : Runtime::kNewClosure);
  }
  context()->Plug(result_register());
}

void FullCodeGenerator::EmitNamedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  DCHECK(!prop->IsSuperAccess());

  CallLoadIC(prop->PropertyFeedbackSlot(), key->value());
}

void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);

  EmitLoadSlot(LoadDescriptor::SlotRegister(), prop->PropertyFeedbackSlot());

  Handle<Code> code = CodeFactory::KeyedLoadIC(isolate()).code();
  __ Call(code, RelocInfo::CODE_TARGET);
  RestoreContext();
}

void FullCodeGenerator::EmitLoadSlot(Register destination, FeedbackSlot slot) {
  DCHECK(!slot.IsInvalid());
  __ Move(destination, SmiFromSlot(slot));
}

void FullCodeGenerator::EmitPushSlot(FeedbackSlot slot) {
  __ Push(SmiFromSlot(slot));
}

void FullCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Comment cmnt(masm_, "[ ReturnStatement");
  SetStatementPosition(stmt);
  Expression* expr = stmt->expression();
  VisitForAccumulatorValue(expr);
  EmitUnwindAndReturn();
}


void FullCodeGenerator::VisitWithStatement(WithStatement* stmt) {
  // Dynamic scoping is not supported.
  UNREACHABLE();
}


void FullCodeGenerator::VisitDoWhileStatement(DoWhileStatement* stmt) {
  Comment cmnt(masm_, "[ DoWhileStatement");
  // Do not insert break location as we do that below.
  SetStatementPosition(stmt, SKIP_BREAK);

  Label body, book_keeping;

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  __ bind(&body);
  Visit(stmt->body());

  // Record the position of the do while condition and make sure it is
  // possible to break on the condition.
  __ bind(loop_statement.continue_label());
  PrepareForBailoutForId(stmt->ContinueId(), BailoutState::NO_REGISTERS);

  // Here is the actual 'while' keyword.
  SetExpressionAsStatementPosition(stmt->cond());
  VisitForControl(stmt->cond(),
                  &book_keeping,
                  loop_statement.break_label(),
                  &book_keeping);

  // Check stack before looping.
  PrepareForBailoutForId(stmt->BackEdgeId(), BailoutState::NO_REGISTERS);
  __ bind(&book_keeping);
  EmitBackEdgeBookkeeping(stmt, &body);
  __ jmp(&body);

  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
  __ bind(loop_statement.break_label());
  decrement_loop_depth();
}


void FullCodeGenerator::VisitWhileStatement(WhileStatement* stmt) {
  Comment cmnt(masm_, "[ WhileStatement");
  Label loop, body;

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  __ bind(&loop);

  SetExpressionAsStatementPosition(stmt->cond());
  VisitForControl(stmt->cond(),
                  &body,
                  loop_statement.break_label(),
                  &body);

  PrepareForBailoutForId(stmt->BodyId(), BailoutState::NO_REGISTERS);
  __ bind(&body);
  Visit(stmt->body());

  __ bind(loop_statement.continue_label());

  // Check stack before looping.
  EmitBackEdgeBookkeeping(stmt, &loop);
  __ jmp(&loop);

  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
  __ bind(loop_statement.break_label());
  decrement_loop_depth();
}


void FullCodeGenerator::VisitForStatement(ForStatement* stmt) {
  Comment cmnt(masm_, "[ ForStatement");
  // Do not insert break location as we do it below.
  SetStatementPosition(stmt, SKIP_BREAK);

  Label test, body;

  Iteration loop_statement(this, stmt);

  if (stmt->init() != NULL) {
    Visit(stmt->init());
  }

  increment_loop_depth();
  // Emit the test at the bottom of the loop (even if empty).
  __ jmp(&test);

  PrepareForBailoutForId(stmt->BodyId(), BailoutState::NO_REGISTERS);
  __ bind(&body);
  Visit(stmt->body());

  PrepareForBailoutForId(stmt->ContinueId(), BailoutState::NO_REGISTERS);
  __ bind(loop_statement.continue_label());
  if (stmt->next() != NULL) {
    SetStatementPosition(stmt->next());
    Visit(stmt->next());
  }

  // Check stack before looping.
  EmitBackEdgeBookkeeping(stmt, &body);

  __ bind(&test);
  if (stmt->cond() != NULL) {
    SetExpressionAsStatementPosition(stmt->cond());
    VisitForControl(stmt->cond(),
                    &body,
                    loop_statement.break_label(),
                    loop_statement.break_label());
  } else {
    __ jmp(&body);
  }

  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
  __ bind(loop_statement.break_label());
  decrement_loop_depth();
}


void FullCodeGenerator::VisitForOfStatement(ForOfStatement* stmt) {
  // Iterator looping is not supported.
  UNREACHABLE();
}

void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  LoadFromFrameField(JavaScriptFrameConstants::kFunctionOffset,
                     result_register());
  context()->Plug(result_register());
}

void FullCodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  // Exception handling is not supported.
  UNREACHABLE();
}


void FullCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  // Exception handling is not supported.
  UNREACHABLE();
}


void FullCodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
  // Debugger statement is not supported.
  UNREACHABLE();
}


void FullCodeGenerator::VisitCaseClause(CaseClause* clause) {
  UNREACHABLE();
}


void FullCodeGenerator::VisitConditional(Conditional* expr) {
  Comment cmnt(masm_, "[ Conditional");
  Label true_case, false_case, done;
  VisitForControl(expr->condition(), &true_case, &false_case, &true_case);

  int original_stack_depth = operand_stack_depth_;
  PrepareForBailoutForId(expr->ThenId(), BailoutState::NO_REGISTERS);
  __ bind(&true_case);
  SetExpressionPosition(expr->then_expression());
  if (context()->IsTest()) {
    const TestContext* for_test = TestContext::cast(context());
    VisitForControl(expr->then_expression(),
                    for_test->true_label(),
                    for_test->false_label(),
                    NULL);
  } else {
    VisitInDuplicateContext(expr->then_expression());
    __ jmp(&done);
  }

  operand_stack_depth_ = original_stack_depth;
  PrepareForBailoutForId(expr->ElseId(), BailoutState::NO_REGISTERS);
  __ bind(&false_case);
  SetExpressionPosition(expr->else_expression());
  VisitInDuplicateContext(expr->else_expression());
  // If control flow falls through Visit, merge it with true case here.
  if (!context()->IsTest()) {
    __ bind(&done);
  }
}


void FullCodeGenerator::VisitLiteral(Literal* expr) {
  Comment cmnt(masm_, "[ Literal");
  context()->Plug(expr->value());
}


void FullCodeGenerator::VisitFunctionLiteral(FunctionLiteral* expr) {
  Comment cmnt(masm_, "[ FunctionLiteral");

  // Build the function boilerplate and instantiate it.
  Handle<SharedFunctionInfo> function_info =
      Compiler::GetSharedFunctionInfo(expr, script(), info_);
  if (function_info.is_null()) {
    SetStackOverflow();
    return;
  }
  EmitNewClosure(function_info, expr->LiteralFeedbackSlot(), expr->pretenure());
}


void FullCodeGenerator::VisitClassLiteral(ClassLiteral* lit) {
  // Unsupported
  UNREACHABLE();
}

void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Callable callable = CodeFactory::FastCloneRegExp(isolate());
  CallInterfaceDescriptor descriptor = callable.descriptor();
  LoadFromFrameField(JavaScriptFrameConstants::kFunctionOffset,
                     descriptor.GetRegisterParameter(0));
  __ Move(descriptor.GetRegisterParameter(1),
          SmiFromSlot(expr->literal_slot()));
  __ Move(descriptor.GetRegisterParameter(2), expr->pattern());
  __ Move(descriptor.GetRegisterParameter(3), Smi::FromInt(expr->flags()));
  __ Call(callable.code(), RelocInfo::CODE_TARGET);

  // Reload the context register after the call as i.e. TurboFan code stubs
  // won't preserve the context register.
  LoadFromFrameField(StandardFrameConstants::kContextOffset,
                     context_register());
  context()->Plug(result_register());
}

void FullCodeGenerator::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* expr) {
  Comment cmnt(masm_, "[ NativeFunctionLiteral");
  Handle<SharedFunctionInfo> shared =
      Compiler::GetSharedFunctionInfoForNative(expr->extension(), expr->name());
  EmitNewClosure(shared, expr->LiteralFeedbackSlot(), false);
}


void FullCodeGenerator::VisitThrow(Throw* expr) {
  Comment cmnt(masm_, "[ Throw");
  VisitForStackValue(expr->exception());
  SetExpressionPosition(expr);
  CallRuntimeWithOperands(Runtime::kThrow);
  // Never returns here.

  // Even though this expression doesn't produce a value, we need to simulate
  // plugging of the value context to ensure stack depth tracking is in sync.
  if (context()->IsStackValue()) OperandStackDepthIncrement(1);
}


void FullCodeGenerator::VisitCall(Call* expr) {
#ifdef DEBUG
  // We want to verify that RecordJSReturnSite gets called on all paths
  // through this function.  Avoid early returns.
  expr->return_is_recorded_ = false;
#endif

  Comment cmnt(masm_, (expr->tail_call_mode() == TailCallMode::kAllow)
                          ? "[ TailCall"
                          : "[ Call");
  Expression* callee = expr->expression();
  Call::CallType call_type = expr->GetCallType();

  // Eval is unsupported.
  CHECK(!expr->is_possibly_eval());

  switch (call_type) {
    case Call::GLOBAL_CALL:
      EmitCallWithLoadIC(expr);
      break;
    case Call::NAMED_PROPERTY_CALL: {
      Property* property = callee->AsProperty();
      VisitForStackValue(property->obj());
      EmitCallWithLoadIC(expr);
      break;
    }
    case Call::KEYED_PROPERTY_CALL: {
      Property* property = callee->AsProperty();
      VisitForStackValue(property->obj());
      EmitKeyedCallWithLoadIC(expr, property->key());
      break;
    }
    case Call::OTHER_CALL:
      // Call to an arbitrary expression not handled specially above.
      VisitForStackValue(callee);
      OperandStackDepthIncrement(1);
      __ PushRoot(Heap::kUndefinedValueRootIndex);
      // Emit function call.
      EmitCall(expr);
      break;
    case Call::NAMED_SUPER_PROPERTY_CALL:
    case Call::KEYED_SUPER_PROPERTY_CALL:
    case Call::SUPER_CALL:
    case Call::WITH_CALL:
      UNREACHABLE();
  }

#ifdef DEBUG
  // RecordJSReturnSite should have been called.
  DCHECK(expr->return_is_recorded_);
#endif
}

void FullCodeGenerator::VisitCallRuntime(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  int arg_count = args->length();

  if (expr->is_jsruntime()) {
    Comment cmnt(masm_, "[ CallRuntime");
    EmitLoadJSRuntimeFunction(expr);

    // Push the arguments ("left-to-right").
    for (int i = 0; i < arg_count; i++) {
      VisitForStackValue(args->at(i));
    }

    PrepareForBailoutForId(expr->CallId(), BailoutState::NO_REGISTERS);
    EmitCallJSRuntimeFunction(expr);
    context()->DropAndPlug(1, result_register());

  } else {
    const Runtime::Function* function = expr->function();
    switch (function->function_id) {
#define CALL_INTRINSIC_GENERATOR(Name)     \
  case Runtime::kInline##Name: {           \
    Comment cmnt(masm_, "[ Inline" #Name); \
    return Emit##Name(expr);               \
  }
      FOR_EACH_FULL_CODE_INTRINSIC(CALL_INTRINSIC_GENERATOR)
#undef CALL_INTRINSIC_GENERATOR
      default: {
        Comment cmnt(masm_, "[ CallRuntime for unhandled intrinsic");
        // Push the arguments ("left-to-right").
        for (int i = 0; i < arg_count; i++) {
          VisitForStackValue(args->at(i));
        }

        // Call the C runtime function.
        PrepareForBailoutForId(expr->CallId(), BailoutState::NO_REGISTERS);
        __ CallRuntime(expr->function(), arg_count);
        OperandStackDepthDecrement(arg_count);
        context()->Plug(result_register());
      }
    }
  }
}

void FullCodeGenerator::VisitSpread(Spread* expr) { UNREACHABLE(); }


void FullCodeGenerator::VisitEmptyParentheses(EmptyParentheses* expr) {
  UNREACHABLE();
}

void FullCodeGenerator::VisitGetIterator(GetIterator* expr) { UNREACHABLE(); }

void FullCodeGenerator::VisitImportCallExpression(ImportCallExpression* expr) {
  UNREACHABLE();
}

void FullCodeGenerator::VisitRewritableExpression(RewritableExpression* expr) {
  Visit(expr->expression());
}


bool FullCodeGenerator::TryLiteralCompare(CompareOperation* expr) {
  Expression* sub_expr;
  Literal* literal;
  if (expr->IsLiteralCompareTypeof(&sub_expr, &literal)) {
    SetExpressionPosition(expr);
    EmitLiteralCompareTypeof(expr, sub_expr,
                             Handle<String>::cast(literal->value()));
    return true;
  }

  if (expr->IsLiteralCompareUndefined(&sub_expr)) {
    SetExpressionPosition(expr);
    EmitLiteralCompareNil(expr, sub_expr, kUndefinedValue);
    return true;
  }

  if (expr->IsLiteralCompareNull(&sub_expr)) {
    SetExpressionPosition(expr);
    EmitLiteralCompareNil(expr, sub_expr, kNullValue);
    return true;
  }

  return false;
}


void BackEdgeTable::Patch(Isolate* isolate, Code* unoptimized) {
  DisallowHeapAllocation no_gc;
  Code* patch = isolate->builtins()->builtin(Builtins::kOnStackReplacement);

  // Increment loop nesting level by one and iterate over the back edge table
  // to find the matching loops to patch the interrupt
  // call to an unconditional call to the replacement code.
  int loop_nesting_level = unoptimized->allow_osr_at_loop_nesting_level() + 1;
  if (loop_nesting_level > AbstractCode::kMaxLoopNestingMarker) return;

  BackEdgeTable back_edges(unoptimized, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    if (static_cast<int>(back_edges.loop_depth(i)) == loop_nesting_level) {
      DCHECK_EQ(INTERRUPT, GetBackEdgeState(isolate,
                                            unoptimized,
                                            back_edges.pc(i)));
      PatchAt(unoptimized, back_edges.pc(i), ON_STACK_REPLACEMENT, patch);
    }
  }

  unoptimized->set_allow_osr_at_loop_nesting_level(loop_nesting_level);
  DCHECK(Verify(isolate, unoptimized));
}


void BackEdgeTable::Revert(Isolate* isolate, Code* unoptimized) {
  DisallowHeapAllocation no_gc;
  Code* patch = isolate->builtins()->builtin(Builtins::kInterruptCheck);

  // Iterate over the back edge table and revert the patched interrupt calls.
  int loop_nesting_level = unoptimized->allow_osr_at_loop_nesting_level();

  BackEdgeTable back_edges(unoptimized, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    if (static_cast<int>(back_edges.loop_depth(i)) <= loop_nesting_level) {
      DCHECK_NE(INTERRUPT, GetBackEdgeState(isolate,
                                            unoptimized,
                                            back_edges.pc(i)));
      PatchAt(unoptimized, back_edges.pc(i), INTERRUPT, patch);
    }
  }

  unoptimized->set_allow_osr_at_loop_nesting_level(0);
  // Assert that none of the back edges are patched anymore.
  DCHECK(Verify(isolate, unoptimized));
}


#ifdef DEBUG
bool BackEdgeTable::Verify(Isolate* isolate, Code* unoptimized) {
  DisallowHeapAllocation no_gc;
  int loop_nesting_level = unoptimized->allow_osr_at_loop_nesting_level();
  BackEdgeTable back_edges(unoptimized, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    uint32_t loop_depth = back_edges.loop_depth(i);
    CHECK_LE(static_cast<int>(loop_depth), AbstractCode::kMaxLoopNestingMarker);
    // Assert that all back edges for shallower loops (and only those)
    // have already been patched.
    CHECK_EQ((static_cast<int>(loop_depth) <= loop_nesting_level),
             GetBackEdgeState(isolate,
                              unoptimized,
                              back_edges.pc(i)) != INTERRUPT);
  }
  return true;
}
#endif  // DEBUG


FullCodeGenerator::EnterBlockScopeIfNeeded::EnterBlockScopeIfNeeded(
    FullCodeGenerator* codegen, Scope* scope, BailoutId entry_id,
    BailoutId declarations_id, BailoutId exit_id)
    : codegen_(codegen), exit_id_(exit_id) {
  saved_scope_ = codegen_->scope();

  if (scope == NULL) {
    codegen_->PrepareForBailoutForId(entry_id, BailoutState::NO_REGISTERS);
    needs_block_context_ = false;
  } else {
    needs_block_context_ = scope->NeedsContext();
    codegen_->scope_ = scope;
    {
      if (needs_block_context_) {
        Comment cmnt(masm(), "[ Extend block context");
        codegen_->PushOperand(scope->scope_info());
        codegen_->PushFunctionArgumentForContextAllocation();
        codegen_->CallRuntimeWithOperands(Runtime::kPushBlockContext);

        // Replace the context stored in the frame.
        codegen_->StoreToFrameField(StandardFrameConstants::kContextOffset,
                                    codegen_->context_register());
      }
      CHECK_EQ(0, scope->num_stack_slots());
      codegen_->PrepareForBailoutForId(entry_id, BailoutState::NO_REGISTERS);
    }
    {
      Comment cmnt(masm(), "[ Declarations");
      codegen_->VisitDeclarations(scope->declarations());
      codegen_->PrepareForBailoutForId(declarations_id,
                                       BailoutState::NO_REGISTERS);
    }
  }
}


FullCodeGenerator::EnterBlockScopeIfNeeded::~EnterBlockScopeIfNeeded() {
  if (needs_block_context_) {
    codegen_->LoadContextField(codegen_->context_register(),
                               Context::PREVIOUS_INDEX);
    // Update local stack frame context field.
    codegen_->StoreToFrameField(StandardFrameConstants::kContextOffset,
                                codegen_->context_register());
  }
  codegen_->PrepareForBailoutForId(exit_id_, BailoutState::NO_REGISTERS);
  codegen_->scope_ = saved_scope_;
}

Handle<Script> FullCodeGenerator::script() { return info_->script(); }

LanguageMode FullCodeGenerator::language_mode() {
  return scope()->language_mode();
}

bool FullCodeGenerator::has_simple_parameters() {
  return info_->has_simple_parameters();
}

FunctionLiteral* FullCodeGenerator::literal() const { return info_->literal(); }

const FeedbackVectorSpec* FullCodeGenerator::feedback_vector_spec() const {
  return literal()->feedback_vector_spec();
}

#undef __


}  // namespace internal
}  // namespace v8
