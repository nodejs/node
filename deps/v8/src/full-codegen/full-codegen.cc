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
      handler_table_(info->zone()),
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
  cgen.PopulateHandlerTable(code);
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


void FullCodeGenerator::PopulateHandlerTable(Handle<Code> code) {
  int handler_table_size = static_cast<int>(handler_table_.size());
  Handle<HandlerTable> table =
      Handle<HandlerTable>::cast(isolate()->factory()->NewFixedArray(
          HandlerTable::LengthForRange(handler_table_size), TENURED));
  for (int i = 0; i < handler_table_size; ++i) {
    table->SetRangeStart(i, handler_table_[i].range_start);
    table->SetRangeEnd(i, handler_table_[i].range_end);
    table->SetRangeHandler(i, handler_table_[i].handler_offset,
                           handler_table_[i].catch_prediction);
    table->SetRangeData(i, handler_table_[i].stack_depth);
  }
  code->set_handler_table(*table);
}


int FullCodeGenerator::NewHandlerTableEntry() {
  int index = static_cast<int>(handler_table_.size());
  HandlerTableEntry entry = {0, 0, 0, 0, HandlerTable::UNCAUGHT};
  handler_table_.push_back(entry);
  return index;
}


bool FullCodeGenerator::MustCreateObjectLiteralWithRuntime(
    ObjectLiteral* expr) const {
  return masm()->serializer_enabled() ||
         !FastCloneShallowObjectStub::IsSupported(expr);
}


bool FullCodeGenerator::MustCreateArrayLiteralWithRuntime(
    ArrayLiteral* expr) const {
  return expr->depth() > 1 ||
         expr->values()->length() > JSArray::kInitialMaxFastElementArray;
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

void FullCodeGenerator::CallLoadIC(FeedbackVectorSlot slot, Handle<Object> name,
                                   TypeFeedbackId id) {
  DCHECK(name->IsName());
  __ Move(LoadDescriptor::NameRegister(), name);

  EmitLoadSlot(LoadDescriptor::SlotRegister(), slot);

  Handle<Code> ic = CodeFactory::LoadIC(isolate()).code();
  CallIC(ic, id);
  if (FLAG_tf_load_ic_stub) RestoreContext();
}

void FullCodeGenerator::CallStoreIC(FeedbackVectorSlot slot,
                                    Handle<Object> name, TypeFeedbackId id) {
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

  Handle<Code> ic = CodeFactory::StoreIC(isolate(), language_mode()).code();
  CallIC(ic, id);
  RestoreContext();
}

void FullCodeGenerator::CallKeyedStoreIC(FeedbackVectorSlot slot) {
  STATIC_ASSERT(!StoreDescriptor::kPassLastArgsOnStack ||
                StoreDescriptor::kStackArgumentsCount == 2);
  if (StoreDescriptor::kPassLastArgsOnStack) {
    __ Push(StoreDescriptor::ValueRegister());
    EmitPushSlot(slot);
  } else {
    EmitLoadSlot(StoreDescriptor::SlotRegister(), slot);
  }

  Handle<Code> ic =
      CodeFactory::KeyedStoreIC(isolate(), language_mode()).code();
  CallIC(ic);
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


void FullCodeGenerator::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
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
#ifdef DEBUG
  Variable* var = proxy->var();
  DCHECK(var->IsUnallocated() ||
         (var->IsLookupSlot() && var->mode() == DYNAMIC_GLOBAL));
#endif
  EmitLoadSlot(LoadGlobalDescriptor::SlotRegister(),
               proxy->VariableFeedbackSlot());
  Handle<Code> ic = CodeFactory::LoadGlobalIC(isolate(), typeof_mode).code();
  CallIC(ic);
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


void FullCodeGenerator::EmitRegExpExec(CallRuntime* expr) {
  // Load the arguments on the stack and call the stub.
  RegExpExecStub stub(isolate());
  ZoneList<Expression*>* args = expr->arguments();
  DCHECK(args->length() == 4);
  VisitForStackValue(args->at(0));
  VisitForStackValue(args->at(1));
  VisitForStackValue(args->at(2));
  VisitForStackValue(args->at(3));
  __ CallStub(&stub);
  OperandStackDepthDecrement(4);
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

void FullCodeGenerator::EmitNewObject(CallRuntime* expr) {
  EmitIntrinsicAsStubCall(expr, CodeFactory::FastNewObject(isolate()));
}

void FullCodeGenerator::EmitNumberToString(CallRuntime* expr) {
  EmitIntrinsicAsStubCall(expr, CodeFactory::NumberToString(isolate()));
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


void FullCodeGenerator::EmitRegExpConstructResult(CallRuntime* expr) {
  EmitIntrinsicAsStubCall(expr, CodeFactory::RegExpConstructResult(isolate()));
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
  source_position_table_builder_.AddPosition(masm_->pc_offset(), pos, true);
}

void FullCodeGenerator::RecordPosition(int pos) {
  DCHECK_NE(kNoSourcePosition, pos);
  source_position_table_builder_.AddPosition(masm_->pc_offset(), pos, false);
}


void FullCodeGenerator::SetFunctionPosition(FunctionLiteral* fun) {
  RecordPosition(fun->start_position());
}


void FullCodeGenerator::SetReturnPosition(FunctionLiteral* fun) {
  // For default constructors, start position equals end position, and there
  // is no source code besides the class literal.
  int pos = std::max(fun->start_position(), fun->end_position() - 1);
  RecordStatementPosition(pos);
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
  context()->Plug(handle(Smi::FromInt(0), isolate()));
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
    if (!expr->IsSuperAccess()) {
      VisitForAccumulatorValue(expr->obj());
      __ Move(LoadDescriptor::ReceiverRegister(), result_register());
      EmitNamedPropertyLoad(expr);
    } else {
      VisitForStackValue(expr->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          expr->obj()->AsSuperPropertyReference()->home_object());
      EmitNamedSuperPropertyLoad(expr);
    }
  } else {
    if (!expr->IsSuperAccess()) {
      VisitForStackValue(expr->obj());
      VisitForAccumulatorValue(expr->key());
      __ Move(LoadDescriptor::NameRegister(), result_register());
      PopOperand(LoadDescriptor::ReceiverRegister());
      EmitKeyedPropertyLoad(expr);
    } else {
      VisitForStackValue(expr->obj()->AsSuperPropertyReference()->this_var());
      VisitForStackValue(
          expr->obj()->AsSuperPropertyReference()->home_object());
      VisitForStackValue(expr->key());
      EmitKeyedSuperPropertyLoad(expr);
    }
  }
  PrepareForBailoutForId(expr->LoadId(), BailoutState::TOS_REGISTER);
  context()->Plug(result_register());
}

void FullCodeGenerator::VisitForTypeofValue(Expression* expr) {
  VariableProxy* proxy = expr->AsVariableProxy();
  DCHECK(!context()->IsEffect());
  DCHECK(!context()->IsTest());

  if (proxy != NULL &&
      (proxy->var()->IsUnallocated() || proxy->var()->IsLookupSlot())) {
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
  // with one that's safe for GC.  If we hit an exit from the try block of
  // try...finally on our way out, we will unconditionally preserve the
  // accumulator on the stack.
  ClearAccumulator();
  while (!current->IsContinueTarget(target)) {
    if (HasStackOverflow()) return;
    if (current->IsTryFinally()) {
      Comment cmnt(masm(), "[ Deferred continue through finally");
      current->Exit(&context_length);
      DCHECK_EQ(-1, context_length);
      current->AsTryFinally()->deferred_commands()->RecordContinue(target);
      return;
    }
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
  // with one that's safe for GC.  If we hit an exit from the try block of
  // try...finally on our way out, we will unconditionally preserve the
  // accumulator on the stack.
  ClearAccumulator();
  while (!current->IsBreakTarget(target)) {
    if (HasStackOverflow()) return;
    if (current->IsTryFinally()) {
      Comment cmnt(masm(), "[ Deferred break through finally");
      current->Exit(&context_length);
      DCHECK_EQ(-1, context_length);
      current->AsTryFinally()->deferred_commands()->RecordBreak(target);
      return;
    }
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
    if (current->IsTryFinally()) {
      Comment cmnt(masm(), "[ Deferred return through finally");
      current->Exit(&context_length);
      DCHECK_EQ(-1, context_length);
      current->AsTryFinally()->deferred_commands()->RecordReturn();
      return;
    }
    current = current->Exit(&context_length);
  }
  EmitReturnSequence();
}

void FullCodeGenerator::EmitNewClosure(Handle<SharedFunctionInfo> info,
                                       bool pretenure) {
  // If we're running with the --always-opt or the --prepare-always-opt
  // flag, we need to use the runtime function so that the new function
  // we are creating here gets a chance to have its code optimized and
  // doesn't just get a copy of the existing unoptimized code.
  if (!FLAG_always_opt && !FLAG_prepare_always_opt && !pretenure &&
      scope()->is_function_scope()) {
    FastNewClosureStub stub(isolate());
    __ Move(stub.GetCallInterfaceDescriptor().GetRegisterParameter(0), info);
    __ CallStub(&stub);
  } else {
    __ Push(info);
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

void FullCodeGenerator::EmitNamedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object
  SetExpressionPosition(prop);
  Literal* key = prop->key()->AsLiteral();
  DCHECK(!key->value()->IsSmi());
  DCHECK(prop->IsSuperAccess());

  PushOperand(key->value());
  CallRuntimeWithOperands(Runtime::kLoadFromSuper);
}

void FullCodeGenerator::EmitKeyedPropertyLoad(Property* prop) {
  SetExpressionPosition(prop);

  EmitLoadSlot(LoadDescriptor::SlotRegister(), prop->PropertyFeedbackSlot());

  Handle<Code> ic = CodeFactory::KeyedLoadIC(isolate()).code();
  CallIC(ic);
  RestoreContext();
}

void FullCodeGenerator::EmitKeyedSuperPropertyLoad(Property* prop) {
  // Stack: receiver, home_object, key.
  SetExpressionPosition(prop);
  CallRuntimeWithOperands(Runtime::kLoadKeyedFromSuper);
}

void FullCodeGenerator::EmitPropertyKey(LiteralProperty* property,
                                        BailoutId bailout_id) {
  VisitForStackValue(property->key());
  CallRuntimeWithOperands(Runtime::kToName);
  PrepareForBailoutForId(bailout_id, BailoutState::TOS_REGISTER);
  PushOperand(result_register());
}

void FullCodeGenerator::EmitLoadSlot(Register destination,
                                     FeedbackVectorSlot slot) {
  DCHECK(!slot.IsInvalid());
  __ Move(destination, SmiFromSlot(slot));
}

void FullCodeGenerator::EmitPushSlot(FeedbackVectorSlot slot) {
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
  Comment cmnt(masm_, "[ WithStatement");
  SetStatementPosition(stmt);

  VisitForAccumulatorValue(stmt->expression());
  Callable callable = CodeFactory::ToObject(isolate());
  __ Move(callable.descriptor().GetRegisterParameter(0), result_register());
  __ Call(callable.code(), RelocInfo::CODE_TARGET);
  RestoreContext();
  PrepareForBailoutForId(stmt->ToObjectId(), BailoutState::TOS_REGISTER);
  PushOperand(result_register());
  PushOperand(stmt->scope()->scope_info());
  PushFunctionArgumentForContextAllocation();
  CallRuntimeWithOperands(Runtime::kPushWithContext);
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
  PrepareForBailoutForId(stmt->EntryId(), BailoutState::NO_REGISTERS);

  Scope* saved_scope = scope();
  scope_ = stmt->scope();
  { WithOrCatch body(this);
    Visit(stmt->statement());
  }
  scope_ = saved_scope;

  // Pop context.
  LoadContextField(context_register(), Context::PREVIOUS_INDEX);
  // Update local stack frame context field.
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
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
  Comment cmnt(masm_, "[ ForOfStatement");

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  // var iterator = iterable[Symbol.iterator]();
  SetExpressionAsStatementPosition(stmt->assign_iterator());
  VisitForEffect(stmt->assign_iterator());

  // Loop entry.
  __ bind(loop_statement.continue_label());

  // result = iterator.next()
  SetExpressionAsStatementPosition(stmt->next_result());
  VisitForEffect(stmt->next_result());

  // if (result.done) break;
  Label result_not_done;
  VisitForControl(stmt->result_done(), loop_statement.break_label(),
                  &result_not_done, &result_not_done);
  __ bind(&result_not_done);

  // each = result.value
  VisitForEffect(stmt->assign_each());

  // Generate code for the body of the loop.
  Visit(stmt->body());

  // Check stack before looping.
  PrepareForBailoutForId(stmt->BackEdgeId(), BailoutState::NO_REGISTERS);
  EmitBackEdgeBookkeeping(stmt, loop_statement.continue_label());
  __ jmp(loop_statement.continue_label());

  // Exit and decrement the loop depth.
  PrepareForBailoutForId(stmt->ExitId(), BailoutState::NO_REGISTERS);
  __ bind(loop_statement.break_label());
  decrement_loop_depth();
}

void FullCodeGenerator::VisitThisFunction(ThisFunction* expr) {
  LoadFromFrameField(JavaScriptFrameConstants::kFunctionOffset,
                     result_register());
  context()->Plug(result_register());
}

void FullCodeGenerator::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Comment cmnt(masm_, "[ TryCatchStatement");
  SetStatementPosition(stmt, SKIP_BREAK);

  // The try block adds a handler to the exception handler chain before
  // entering, and removes it again when exiting normally.  If an exception
  // is thrown during execution of the try block, the handler is consumed
  // and control is passed to the catch block with the exception in the
  // result register.

  Label try_entry, handler_entry, exit;
  __ jmp(&try_entry);
  __ bind(&handler_entry);
  if (stmt->clear_pending_message()) ClearPendingMessage();

  // Exception handler code, the exception is in the result register.
  // Extend the context before executing the catch block.
  { Comment cmnt(masm_, "[ Extend catch context");
    PushOperand(stmt->variable()->name());
    PushOperand(result_register());
    PushOperand(stmt->scope()->scope_info());
    PushFunctionArgumentForContextAllocation();
    CallRuntimeWithOperands(Runtime::kPushCatchContext);
    StoreToFrameField(StandardFrameConstants::kContextOffset,
                      context_register());
  }

  Scope* saved_scope = scope();
  scope_ = stmt->scope();
  DCHECK(scope_->declarations()->is_empty());
  { WithOrCatch catch_body(this);
    Visit(stmt->catch_block());
  }
  // Restore the context.
  LoadContextField(context_register(), Context::PREVIOUS_INDEX);
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
  scope_ = saved_scope;
  __ jmp(&exit);

  // Try block code. Sets up the exception handler chain.
  __ bind(&try_entry);

  int handler_index = NewHandlerTableEntry();
  EnterTryBlock(handler_index, &handler_entry, stmt->catch_prediction());
  {
    Comment cmnt_try(masm(), "[ Try block");
    Visit(stmt->try_block());
  }
  ExitTryBlock(handler_index);
  __ bind(&exit);
}


void FullCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  Comment cmnt(masm_, "[ TryFinallyStatement");
  SetStatementPosition(stmt, SKIP_BREAK);

  // Try finally is compiled by setting up a try-handler on the stack while
  // executing the try body, and removing it again afterwards.
  //
  // The try-finally construct can enter the finally block in three ways:
  // 1. By exiting the try-block normally. This exits the try block,
  //    pushes the continuation token and falls through to the finally
  //    block.
  // 2. By exiting the try-block with a function-local control flow transfer
  //    (break/continue/return). The site of the, e.g., break exits the
  //    try block, pushes the continuation token and jumps to the
  //    finally block. After the finally block executes, the execution
  //    continues based on the continuation token to a block that
  //    continues with the control flow transfer.
  // 3. By exiting the try-block with a thrown exception. In the handler,
  //    we push the exception and continuation token and jump to the
  //    finally block (which will again dispatch based on the token once
  //    it is finished).

  Label try_entry, handler_entry, finally_entry;
  DeferredCommands deferred(this, &finally_entry);

  // Jump to try-handler setup and try-block code.
  __ jmp(&try_entry);
  __ bind(&handler_entry);

  // Exception handler code.  This code is only executed when an exception
  // is thrown.  Record the continuation and jump to the finally block.
  {
    Comment cmnt_handler(masm(), "[ Finally handler");
    deferred.RecordThrow();
  }

  // Set up try handler.
  __ bind(&try_entry);
  int handler_index = NewHandlerTableEntry();
  EnterTryBlock(handler_index, &handler_entry, stmt->catch_prediction());
  {
    Comment cmnt_try(masm(), "[ Try block");
    TryFinally try_body(this, &deferred);
    Visit(stmt->try_block());
  }
  ExitTryBlock(handler_index);
  // Execute the finally block on the way out.  Clobber the unpredictable
  // value in the result register with one that's safe for GC because the
  // finally block will unconditionally preserve the result register on the
  // stack.
  ClearAccumulator();
  deferred.EmitFallThrough();
  // Fall through to the finally block.

  // Finally block implementation.
  __ bind(&finally_entry);
  {
    Comment cmnt_finally(masm(), "[ Finally block");
    OperandStackDepthIncrement(2);  // Token and accumulator are on stack.
    EnterFinallyBlock();
    Visit(stmt->finally_block());
    ExitFinallyBlock();
    OperandStackDepthDecrement(2);  // Token and accumulator were on stack.
  }

  {
    Comment cmnt_deferred(masm(), "[ Post-finally dispatch");
    deferred.EmitCommands();  // Return to the calling code.
  }
}


void FullCodeGenerator::VisitDebuggerStatement(DebuggerStatement* stmt) {
  Comment cmnt(masm_, "[ DebuggerStatement");
  SetStatementPosition(stmt);

  __ DebugBreak();
  // Ignore the return value.

  PrepareForBailoutForId(stmt->DebugBreakId(), BailoutState::NO_REGISTERS);
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
  EmitNewClosure(function_info, expr->pretenure());
}


void FullCodeGenerator::VisitClassLiteral(ClassLiteral* lit) {
  Comment cmnt(masm_, "[ ClassLiteral");

  if (lit->extends() != NULL) {
    VisitForStackValue(lit->extends());
  } else {
    PushOperand(isolate()->factory()->the_hole_value());
  }

  VisitForStackValue(lit->constructor());

  PushOperand(Smi::FromInt(lit->start_position()));
  PushOperand(Smi::FromInt(lit->end_position()));

  CallRuntimeWithOperands(Runtime::kDefineClass);
  PrepareForBailoutForId(lit->CreateLiteralId(), BailoutState::TOS_REGISTER);
  PushOperand(result_register());

  // Load the "prototype" from the constructor.
  __ Move(LoadDescriptor::ReceiverRegister(), result_register());
  CallLoadIC(lit->PrototypeSlot(), isolate()->factory()->prototype_string());
  PrepareForBailoutForId(lit->PrototypeId(), BailoutState::TOS_REGISTER);
  PushOperand(result_register());

  EmitClassDefineProperties(lit);
  DropOperands(1);

  // Set the constructor to have fast properties.
  CallRuntimeWithOperands(Runtime::kToFastProperties);

  if (lit->class_variable_proxy() != nullptr) {
    EmitVariableAssignment(lit->class_variable_proxy()->var(), Token::INIT,
                           lit->ProxySlot());
  }

  context()->Plug(result_register());
}

void FullCodeGenerator::VisitRegExpLiteral(RegExpLiteral* expr) {
  Comment cmnt(masm_, "[ RegExpLiteral");
  Callable callable = CodeFactory::FastCloneRegExp(isolate());
  CallInterfaceDescriptor descriptor = callable.descriptor();
  LoadFromFrameField(JavaScriptFrameConstants::kFunctionOffset,
                     descriptor.GetRegisterParameter(0));
  __ Move(descriptor.GetRegisterParameter(1),
          Smi::FromInt(expr->literal_index()));
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
  EmitNewClosure(shared, false);
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

void FullCodeGenerator::EnterTryBlock(
    int handler_index, Label* handler,
    HandlerTable::CatchPrediction catch_prediction) {
  HandlerTableEntry* entry = &handler_table_[handler_index];
  entry->range_start = masm()->pc_offset();
  entry->handler_offset = handler->pos();
  entry->stack_depth = operand_stack_depth_;
  entry->catch_prediction = catch_prediction;

  // We are using the operand stack depth, check for accuracy.
  EmitOperandStackDepthCheck();

  // Push context onto operand stack.
  STATIC_ASSERT(TryBlockConstant::kElementCount == 1);
  PushOperand(context_register());
}


void FullCodeGenerator::ExitTryBlock(int handler_index) {
  HandlerTableEntry* entry = &handler_table_[handler_index];
  entry->range_end = masm()->pc_offset();

  // Drop context from operand stack.
  DropOperands(TryBlockConstant::kElementCount);
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

  switch (call_type) {
    case Call::POSSIBLY_EVAL_CALL:
      EmitPossiblyEvalCall(expr);
      break;
    case Call::GLOBAL_CALL:
      EmitCallWithLoadIC(expr);
      break;
    case Call::LOOKUP_SLOT_CALL:
      // Call to a lookup slot (dynamically introduced variable).
      PushCalleeAndWithBaseObject(expr);
      EmitCall(expr);
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
    case Call::NAMED_SUPER_PROPERTY_CALL:
      EmitSuperCallWithLoadIC(expr);
      break;
    case Call::KEYED_SUPER_PROPERTY_CALL:
      EmitKeyedSuperCallWithLoadIC(expr);
      break;
    case Call::SUPER_CALL:
      EmitSuperConstructorCall(expr);
      break;
    case Call::OTHER_CALL:
      // Call to an arbitrary expression not handled specially above.
      VisitForStackValue(callee);
      OperandStackDepthIncrement(1);
      __ PushRoot(Heap::kUndefinedValueRootIndex);
      // Emit function call.
      EmitCall(expr);
      break;
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


void FullCodeGenerator::VisitRewritableExpression(RewritableExpression* expr) {
  Visit(expr->expression());
}

FullCodeGenerator::NestedStatement* FullCodeGenerator::TryFinally::Exit(
    int* context_length) {
  // The macros used here must preserve the result register.

  // Calculate how many operands to drop to get down to handler block.
  int stack_drop = codegen_->operand_stack_depth_ - GetStackDepthAtTarget();
  DCHECK_GE(stack_drop, 0);

  // Because the handler block contains the context of the finally
  // code, we can restore it directly from there for the finally code
  // rather than iteratively unwinding contexts via their previous
  // links.
  if (*context_length > 0) {
    __ Drop(stack_drop);  // Down to the handler block.
    // Restore the context to its dedicated register and the stack.
    STATIC_ASSERT(TryBlockConstant::kElementCount == 1);
    __ Pop(codegen_->context_register());
    codegen_->StoreToFrameField(StandardFrameConstants::kContextOffset,
                                codegen_->context_register());
  } else {
    // Down to the handler block and also drop context.
    __ Drop(stack_drop + TryBlockConstant::kElementCount);
  }

  // The caller will ignore outputs.
  *context_length = -1;
  return previous_;
}

void FullCodeGenerator::DeferredCommands::RecordBreak(Statement* target) {
  TokenId token = dispenser_.GetBreakContinueToken();
  commands_.push_back({kBreak, token, target});
  EmitJumpToFinally(token);
}

void FullCodeGenerator::DeferredCommands::RecordContinue(Statement* target) {
  TokenId token = dispenser_.GetBreakContinueToken();
  commands_.push_back({kContinue, token, target});
  EmitJumpToFinally(token);
}

void FullCodeGenerator::DeferredCommands::RecordReturn() {
  if (return_token_ == TokenDispenserForFinally::kInvalidToken) {
    return_token_ = TokenDispenserForFinally::kReturnToken;
    commands_.push_back({kReturn, return_token_, nullptr});
  }
  EmitJumpToFinally(return_token_);
}

void FullCodeGenerator::DeferredCommands::RecordThrow() {
  if (throw_token_ == TokenDispenserForFinally::kInvalidToken) {
    throw_token_ = TokenDispenserForFinally::kThrowToken;
    commands_.push_back({kThrow, throw_token_, nullptr});
  }
  EmitJumpToFinally(throw_token_);
}

void FullCodeGenerator::DeferredCommands::EmitFallThrough() {
  __ Push(Smi::FromInt(TokenDispenserForFinally::kFallThroughToken));
  __ Push(result_register());
}

void FullCodeGenerator::DeferredCommands::EmitJumpToFinally(TokenId token) {
  __ Push(Smi::FromInt(token));
  __ Push(result_register());
  __ jmp(finally_entry_);
}

bool FullCodeGenerator::TryLiteralCompare(CompareOperation* expr) {
  Expression* sub_expr;
  Handle<String> check;
  if (expr->IsLiteralCompareTypeof(&sub_expr, &check)) {
    SetExpressionPosition(expr);
    EmitLiteralCompareTypeof(expr, sub_expr, check);
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


bool FullCodeGenerator::NeedsHoleCheckForLoad(VariableProxy* proxy) {
  Variable* var = proxy->var();

  if (!var->binding_needs_init()) {
    return false;
  }

  // var->scope() may be NULL when the proxy is located in eval code and
  // refers to a potential outside binding. Currently those bindings are
  // always looked up dynamically, i.e. in that case
  //     var->location() == LOOKUP.
  // always holds.
  DCHECK(var->scope() != NULL);
  DCHECK(var->location() == VariableLocation::PARAMETER ||
         var->location() == VariableLocation::LOCAL ||
         var->location() == VariableLocation::CONTEXT);

  // Check if the binding really needs an initialization check. The check
  // can be skipped in the following situation: we have a LET or CONST
  // binding in harmony mode, both the Variable and the VariableProxy have
  // the same declaration scope (i.e. they are both in global code, in the
  // same function or in the same eval code), the VariableProxy is in
  // the source physically located after the initializer of the variable,
  // and that the initializer cannot be skipped due to a nonlinear scope.
  //
  // We cannot skip any initialization checks for CONST in non-harmony
  // mode because const variables may be declared but never initialized:
  //   if (false) { const x; }; var y = x;
  //
  // The condition on the declaration scopes is a conservative check for
  // nested functions that access a binding and are called before the
  // binding is initialized:
  //   function() { f(); let x = 1; function f() { x = 2; } }
  //
  // The check cannot be skipped on non-linear scopes, namely switch
  // scopes, to ensure tests are done in cases like the following:
  //   switch (1) { case 0: let x = 2; case 1: f(x); }
  // The scope of the variable needs to be checked, in case the use is
  // in a sub-block which may be linear.
  if (var->scope()->GetDeclarationScope() != scope()->GetDeclarationScope()) {
    return true;
  }

  if (var->is_this()) {
    DCHECK(literal() != nullptr &&
           (literal()->kind() & kSubclassConstructor) != 0);
    // TODO(littledan): implement 'this' hole check elimination.
    return true;
  }

  // Check that we always have valid source position.
  DCHECK(var->initializer_position() != kNoSourcePosition);
  DCHECK(proxy->position() != kNoSourcePosition);

  return var->scope()->is_nonlinear() ||
         var->initializer_position() >= proxy->position();
}

Handle<Script> FullCodeGenerator::script() { return info_->script(); }

LanguageMode FullCodeGenerator::language_mode() {
  return scope()->language_mode();
}

bool FullCodeGenerator::has_simple_parameters() {
  return info_->has_simple_parameters();
}

FunctionLiteral* FullCodeGenerator::literal() const { return info_->literal(); }

#undef __


}  // namespace internal
}  // namespace v8
