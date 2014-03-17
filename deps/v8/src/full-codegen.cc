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
#include "snapshot.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

void BreakableStatementChecker::Check(Statement* stmt) {
  Visit(stmt);
}


void BreakableStatementChecker::Check(Expression* expr) {
  Visit(expr);
}


void BreakableStatementChecker::VisitVariableDeclaration(
    VariableDeclaration* decl) {
}

void BreakableStatementChecker::VisitFunctionDeclaration(
    FunctionDeclaration* decl) {
}

void BreakableStatementChecker::VisitModuleDeclaration(
    ModuleDeclaration* decl) {
}

void BreakableStatementChecker::VisitImportDeclaration(
    ImportDeclaration* decl) {
}

void BreakableStatementChecker::VisitExportDeclaration(
    ExportDeclaration* decl) {
}


void BreakableStatementChecker::VisitModuleLiteral(ModuleLiteral* module) {
}


void BreakableStatementChecker::VisitModuleVariable(ModuleVariable* module) {
}


void BreakableStatementChecker::VisitModulePath(ModulePath* module) {
}


void BreakableStatementChecker::VisitModuleUrl(ModuleUrl* module) {
}


void BreakableStatementChecker::VisitModuleStatement(ModuleStatement* stmt) {
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


void BreakableStatementChecker::VisitForOfStatement(ForOfStatement* stmt) {
  // For-of is breakable because of the next() call.
  is_breakable_ = true;
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


void BreakableStatementChecker::VisitCaseClause(CaseClause* clause) {
}


void BreakableStatementChecker::VisitFunctionLiteral(FunctionLiteral* expr) {
}


void BreakableStatementChecker::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* expr) {
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


void BreakableStatementChecker::VisitYield(Yield* expr) {
  // Yield is breakable if the expression is.
  Visit(expr->expression());
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


void BreakableStatementChecker::VisitCompareOperation(CompareOperation* expr) {
  Visit(expr->left());
  Visit(expr->right());
}


void BreakableStatementChecker::VisitThisFunction(ThisFunction* expr) {
}


#define __ ACCESS_MASM(masm())

bool FullCodeGenerator::MakeCode(CompilationInfo* info) {
  Isolate* isolate = info->isolate();

  Logger::TimerEventScope timer(
      isolate, Logger::TimerEventScope::v8_compile_full_code);

  Handle<Script> script = info->script();
  if (!script->IsUndefined() && !script->source()->IsUndefined()) {
    int len = String::cast(script->source())->length();
    isolate->counters()->total_full_codegen_source_size()->Increment(len);
  }
  CodeGenerator::MakeCodePrologue(info, "full");
  const int kInitialBufferSize = 4 * KB;
  MacroAssembler masm(info->isolate(), NULL, kInitialBufferSize);
#ifdef ENABLE_GDB_JIT_INTERFACE
  masm.positions_recorder()->StartGDBJITLineInfoRecording();
#endif
  LOG_CODE_EVENT(isolate,
                 CodeStartLinePosInfoRecordEvent(masm.positions_recorder()));

  FullCodeGenerator cgen(&masm, info);
  cgen.Generate();
  if (cgen.HasStackOverflow()) {
    ASSERT(!isolate->has_pending_exception());
    return false;
  }
  unsigned table_offset = cgen.EmitBackEdgeTable();

  Code::Flags flags = Code::ComputeFlags(Code::FUNCTION);
  Handle<Code> code = CodeGenerator::MakeCodeEpilogue(&masm, flags, info);
  code->set_optimizable(info->IsOptimizable() &&
                        !info->function()->dont_optimize() &&
                        info->function()->scope()->AllowsLazyCompilation());
  cgen.PopulateDeoptimizationData(code);
  cgen.PopulateTypeFeedbackInfo(code);
  cgen.PopulateTypeFeedbackCells(code);
  code->set_has_deoptimization_support(info->HasDeoptimizationSupport());
  code->set_handler_table(*cgen.handler_table());
#ifdef ENABLE_DEBUGGER_SUPPORT
  code->set_compiled_optimizable(info->IsOptimizable());
#endif  // ENABLE_DEBUGGER_SUPPORT
  code->set_allow_osr_at_loop_nesting_level(0);
  code->set_profiler_ticks(0);
  code->set_back_edge_table_offset(table_offset);
  code->set_back_edges_patched_for_osr(false);
  CodeGenerator::PrintCode(code, info);
  info->SetCode(code);
#ifdef ENABLE_GDB_JIT_INTERFACE
  if (FLAG_gdbjit) {
    GDBJITLineInfo* lineinfo =
        masm.positions_recorder()->DetachGDBJITLineInfo();
    GDBJIT(RegisterDetailedLineInfo(*code, lineinfo));
  }
#endif
  void* line_info = masm.positions_recorder()->DetachJITHandlerData();
  LOG_CODE_EVENT(isolate, CodeEndLinePosInfoRecordEvent(*code, line_info));
  return true;
}


unsigned FullCodeGenerator::EmitBackEdgeTable() {
  // The back edge table consists of a length (in number of entries)
  // field, and then a sequence of entries.  Each entry is a pair of AST id
  // and code-relative pc offset.
  masm()->Align(kIntSize);
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
  ASSERT(info_->HasDeoptimizationSupport() || bailout_entries_.is_empty());
  if (!info_->HasDeoptimizationSupport()) return;
  int length = bailout_entries_.length();
  Handle<DeoptimizationOutputData> data = isolate()->factory()->
      NewDeoptimizationOutputData(length, TENURED);
  for (int i = 0; i < length; i++) {
    data->SetAstId(i, bailout_entries_[i].id);
    data->SetPcAndState(i, Smi::FromInt(bailout_entries_[i].pc_and_state));
  }
  code->set_deoptimization_data(*data);
}


void FullCodeGenerator::PopulateTypeFeedbackInfo(Handle<Code> code) {
  Handle<TypeFeedbackInfo> info = isolate()->factory()->NewTypeFeedbackInfo();
  info->set_ic_total_count(ic_total_count_);
  ASSERT(!isolate()->heap()->InNewSpace(*info));
  code->set_type_feedback_info(*info);
}


void FullCodeGenerator::Initialize() {
  // The generation of debug code must match between the snapshot code and the
  // code that is generated later.  This is assumed by the debugger when it is
  // calculating PC offsets after generating a debug version of code.  Therefore
  // we disable the production of debug code in the full compiler if we are
  // either generating a snapshot or we booted from a snapshot.
  generate_debug_code_ = FLAG_debug_code &&
                         !Serializer::enabled() &&
                         !Snapshot::HaveASnapshotToStartFrom();
  masm_->set_emit_debug_code(generate_debug_code_);
  masm_->set_predictable_code_size(true);
  InitializeAstVisitor(info_->zone());
}


void FullCodeGenerator::PopulateTypeFeedbackCells(Handle<Code> code) {
  if (type_feedback_cells_.is_empty()) return;
  int length = type_feedback_cells_.length();
  int array_size = TypeFeedbackCells::LengthOfFixedArray(length);
  Handle<TypeFeedbackCells> cache = Handle<TypeFeedbackCells>::cast(
      isolate()->factory()->NewFixedArray(array_size, TENURED));
  for (int i = 0; i < length; i++) {
    cache->SetAstId(i, type_feedback_cells_[i].ast_id);
    cache->SetCell(i, *type_feedback_cells_[i].cell);
  }
  TypeFeedbackInfo::cast(code->type_feedback_info())->set_type_feedback_cells(
      *cache);
}


void FullCodeGenerator::PrepareForBailout(Expression* node, State state) {
  PrepareForBailoutForId(node->id(), state);
}


void FullCodeGenerator::CallLoadIC(ContextualMode contextual_mode,
                                   TypeFeedbackId id) {
  ExtraICState extra_state = LoadIC::ComputeExtraICState(contextual_mode);
  Handle<Code> ic = LoadIC::initialize_stub(isolate(), extra_state);
  CallIC(ic, contextual_mode, id);
}


void FullCodeGenerator::CallStoreIC(ContextualMode mode, TypeFeedbackId id) {
  Handle<Code> ic = StoreIC::initialize_stub(isolate(), strict_mode());
  CallIC(ic, mode, id);
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


void FullCodeGenerator::PrepareForBailoutForId(BailoutId id, State state) {
  // There's no need to prepare this code for bailouts from already optimized
  // code or code that can't be optimized.
  if (!info_->HasDeoptimizationSupport()) return;
  unsigned pc_and_state =
      StateField::encode(state) | PcField::encode(masm_->pc_offset());
  ASSERT(Smi::IsValid(pc_and_state));
  BailoutEntry entry = { id, pc_and_state };
  ASSERT(!prepared_bailout_ids_.Contains(id.ToInt()));
  prepared_bailout_ids_.Add(id.ToInt(), zone());
  bailout_entries_.Add(entry, zone());
}


void FullCodeGenerator::RecordTypeFeedbackCell(
    TypeFeedbackId id, Handle<Cell> cell) {
  TypeFeedbackCellEntry entry = { id, cell };
  type_feedback_cells_.Add(entry, zone());
}


void FullCodeGenerator::RecordBackEdge(BailoutId ast_id) {
  // The pc offset does not need to be encoded and packed together with a state.
  ASSERT(masm_->pc_offset() > 0);
  ASSERT(loop_depth() > 0);
  uint8_t depth = Min(loop_depth(), Code::kMaxLoopNestingMarker);
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


void FullCodeGenerator::EffectContext::Plug(Register reg) const {
}


void FullCodeGenerator::AccumulatorValueContext::Plug(Register reg) const {
  __ Move(result_register(), reg);
}


void FullCodeGenerator::StackValueContext::Plug(Register reg) const {
  __ Push(reg);
}


void FullCodeGenerator::TestContext::Plug(Register reg) const {
  // For simplicity we always test the accumulator register.
  __ Move(result_register(), reg);
  codegen()->PrepareForBailoutBeforeSplit(condition(), false, NULL, NULL);
  codegen()->DoTest(this);
}


void FullCodeGenerator::EffectContext::PlugTOS() const {
  __ Drop(1);
}


void FullCodeGenerator::AccumulatorValueContext::PlugTOS() const {
  __ Pop(result_register());
}


void FullCodeGenerator::StackValueContext::PlugTOS() const {
}


void FullCodeGenerator::TestContext::PlugTOS() const {
  // For simplicity we always test the accumulator register.
  __ Pop(result_register());
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


void FullCodeGenerator::AllocateModules(ZoneList<Declaration*>* declarations) {
  ASSERT(scope_->is_global_scope());

  for (int i = 0; i < declarations->length(); i++) {
    ModuleDeclaration* declaration = declarations->at(i)->AsModuleDeclaration();
    if (declaration != NULL) {
      ModuleLiteral* module = declaration->module()->AsModuleLiteral();
      if (module != NULL) {
        Comment cmnt(masm_, "[ Link nested modules");
        Scope* scope = module->body()->scope();
        Interface* interface = scope->interface();
        ASSERT(interface->IsModule() && interface->IsFrozen());

        interface->Allocate(scope->module_var()->index());

        // Set up module context.
        ASSERT(scope->interface()->Index() >= 0);
        __ Push(Smi::FromInt(scope->interface()->Index()));
        __ Push(scope->GetScopeInfo());
        __ CallRuntime(Runtime::kPushModuleContext, 2);
        StoreToFrameField(StandardFrameConstants::kContextOffset,
                          context_register());

        AllocateModules(scope->declarations());

        // Pop module context.
        LoadContextField(context_register(), Context::PREVIOUS_INDEX);
        // Update local stack frame context field.
        StoreToFrameField(StandardFrameConstants::kContextOffset,
                          context_register());
      }
    }
  }
}


// Modules have their own local scope, represented by their own context.
// Module instance objects have an accessor for every export that forwards
// access to the respective slot from the module's context. (Exports that are
// modules themselves, however, are simple data properties.)
//
// All modules have a _hosting_ scope/context, which (currently) is the
// (innermost) enclosing global scope. To deal with recursion, nested modules
// are hosted by the same scope as global ones.
//
// For every (global or nested) module literal, the hosting context has an
// internal slot that points directly to the respective module context. This
// enables quick access to (statically resolved) module members by 2-dimensional
// access through the hosting context. For example,
//
//   module A {
//     let x;
//     module B { let y; }
//   }
//   module C { let z; }
//
// allocates contexts as follows:
//
// [header| .A | .B | .C | A | C ]  (global)
//           |    |    |
//           |    |    +-- [header| z ]  (module)
//           |    |
//           |    +------- [header| y ]  (module)
//           |
//           +------------ [header| x | B ]  (module)
//
// Here, .A, .B, .C are the internal slots pointing to the hosted module
// contexts, whereas A, B, C hold the actual instance objects (note that every
// module context also points to the respective instance object through its
// extension slot in the header).
//
// To deal with arbitrary recursion and aliases between modules,
// they are created and initialized in several stages. Each stage applies to
// all modules in the hosting global scope, including nested ones.
//
// 1. Allocate: for each module _literal_, allocate the module contexts and
//    respective instance object and wire them up. This happens in the
//    PushModuleContext runtime function, as generated by AllocateModules
//    (invoked by VisitDeclarations in the hosting scope).
//
// 2. Bind: for each module _declaration_ (i.e. literals as well as aliases),
//    assign the respective instance object to respective local variables. This
//    happens in VisitModuleDeclaration, and uses the instance objects created
//    in the previous stage.
//    For each module _literal_, this phase also constructs a module descriptor
//    for the next stage. This happens in VisitModuleLiteral.
//
// 3. Populate: invoke the DeclareModules runtime function to populate each
//    _instance_ object with accessors for it exports. This is generated by
//    DeclareModules (invoked by VisitDeclarations in the hosting scope again),
//    and uses the descriptors generated in the previous stage.
//
// 4. Initialize: execute the module bodies (and other code) in sequence. This
//    happens by the separate statements generated for module bodies. To reenter
//    the module scopes properly, the parser inserted ModuleStatements.

void FullCodeGenerator::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
  Handle<FixedArray> saved_modules = modules_;
  int saved_module_index = module_index_;
  ZoneList<Handle<Object> >* saved_globals = globals_;
  ZoneList<Handle<Object> > inner_globals(10, zone());
  globals_ = &inner_globals;

  if (scope_->num_modules() != 0) {
    // This is a scope hosting modules. Allocate a descriptor array to pass
    // to the runtime for initialization.
    Comment cmnt(masm_, "[ Allocate modules");
    ASSERT(scope_->is_global_scope());
    modules_ =
        isolate()->factory()->NewFixedArray(scope_->num_modules(), TENURED);
    module_index_ = 0;

    // Generate code for allocating all modules, including nested ones.
    // The allocated contexts are stored in internal variables in this scope.
    AllocateModules(declarations);
  }

  AstVisitor::VisitDeclarations(declarations);

  if (scope_->num_modules() != 0) {
    // Initialize modules from descriptor array.
    ASSERT(module_index_ == modules_->length());
    DeclareModules(modules_);
    modules_ = saved_modules;
    module_index_ = saved_module_index;
  }

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


void FullCodeGenerator::VisitModuleLiteral(ModuleLiteral* module) {
  Block* block = module->body();
  Scope* saved_scope = scope();
  scope_ = block->scope();
  Interface* interface = scope_->interface();

  Comment cmnt(masm_, "[ ModuleLiteral");
  SetStatementPosition(block);

  ASSERT(!modules_.is_null());
  ASSERT(module_index_ < modules_->length());
  int index = module_index_++;

  // Set up module context.
  ASSERT(interface->Index() >= 0);
  __ Push(Smi::FromInt(interface->Index()));
  __ Push(Smi::FromInt(0));
  __ CallRuntime(Runtime::kPushModuleContext, 2);
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());

  {
    Comment cmnt(masm_, "[ Declarations");
    VisitDeclarations(scope_->declarations());
  }

  // Populate the module description.
  Handle<ModuleInfo> description =
      ModuleInfo::Create(isolate(), interface, scope_);
  modules_->set(index, *description);

  scope_ = saved_scope;
  // Pop module context.
  LoadContextField(context_register(), Context::PREVIOUS_INDEX);
  // Update local stack frame context field.
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());
}


void FullCodeGenerator::VisitModuleVariable(ModuleVariable* module) {
  // Nothing to do.
  // The instance object is resolved statically through the module's interface.
}


void FullCodeGenerator::VisitModulePath(ModulePath* module) {
  // Nothing to do.
  // The instance object is resolved statically through the module's interface.
}


void FullCodeGenerator::VisitModuleUrl(ModuleUrl* module) {
  // TODO(rossberg): dummy allocation for now.
  Scope* scope = module->body()->scope();
  Interface* interface = scope_->interface();

  ASSERT(interface->IsModule() && interface->IsFrozen());
  ASSERT(!modules_.is_null());
  ASSERT(module_index_ < modules_->length());
  interface->Allocate(scope->module_var()->index());
  int index = module_index_++;

  Handle<ModuleInfo> description =
      ModuleInfo::Create(isolate(), interface, scope_);
  modules_->set(index, *description);
}


int FullCodeGenerator::DeclareGlobalsFlags() {
  ASSERT(DeclareGlobalsLanguageMode::is_valid(language_mode()));
  return DeclareGlobalsEvalFlag::encode(is_eval()) |
      DeclareGlobalsNativeFlag::encode(is_native()) |
      DeclareGlobalsLanguageMode::encode(language_mode());
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
    CodeGenerator::RecordPositions(masm_, stmt->position());
  } else {
    // Check if the statement will be breakable without adding a debug break
    // slot.
    BreakableStatementChecker checker(zone());
    checker.Check(stmt);
    // Record the statement position right here if the statement is not
    // breakable. For breakable statements the actual recording of the
    // position will be postponed to the breakable code (typically an IC).
    bool position_recorded = CodeGenerator::RecordPositions(
        masm_, stmt->position(), !checker.is_breakable());
    // If the position recording did record a new position generate a debug
    // break slot to make the statement breakable.
    if (position_recorded) {
      Debug::GenerateSlot(masm_);
    }
  }
#else
  CodeGenerator::RecordPositions(masm_, stmt->position());
#endif
}


void FullCodeGenerator::SetExpressionPosition(Expression* expr) {
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (!isolate()->debugger()->IsDebuggerActive()) {
    CodeGenerator::RecordPositions(masm_, expr->position());
  } else {
    // Check if the expression will be breakable without adding a debug break
    // slot.
    BreakableStatementChecker checker(zone());
    checker.Check(expr);
    // Record a statement position right here if the expression is not
    // breakable. For breakable expressions the actual recording of the
    // position will be postponed to the breakable code (typically an IC).
    // NOTE this will record a statement position for something which might
    // not be a statement. As stepping in the debugger will only stop at
    // statement positions this is used for e.g. the condition expression of
    // a do while loop.
    bool position_recorded = CodeGenerator::RecordPositions(
        masm_, expr->position(), !checker.is_breakable());
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


void FullCodeGenerator::EmitInlineRuntimeCall(CallRuntime* expr) {
  const Runtime::Function* function = expr->function();
  ASSERT(function != NULL);
  ASSERT(function->intrinsic_type == Runtime::INLINE);
  InlineFunctionGenerator generator =
      FindInlineFunctionGenerator(function->function_id);
  ((*this).*(generator))(expr);
}


void FullCodeGenerator::EmitGeneratorNext(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  EmitGeneratorResume(args->at(0), args->at(1), JSGeneratorObject::NEXT);
}


void FullCodeGenerator::EmitGeneratorThrow(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  ASSERT(args->length() == 2);
  EmitGeneratorResume(args->at(0), args->at(1), JSGeneratorObject::THROW);
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
    PrepareForBailoutForId(right_id, NO_REGISTERS);
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
    PrepareForBailoutForId(right_id, NO_REGISTERS);

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

  VisitInDuplicateContext(right);
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


void FullCodeGenerator::VisitBlock(Block* stmt) {
  Comment cmnt(masm_, "[ Block");
  NestedBlock nested_block(this, stmt);
  SetStatementPosition(stmt);

  Scope* saved_scope = scope();
  // Push a block context when entering a block with block scoped variables.
  if (stmt->scope() != NULL) {
    scope_ = stmt->scope();
    ASSERT(!scope_->is_module_scope());
    { Comment cmnt(masm_, "[ Extend block context");
      __ Push(scope_->GetScopeInfo());
      PushFunctionArgumentForContextAllocation();
      __ CallRuntime(Runtime::kPushBlockContext, 2);

      // Replace the context stored in the frame.
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
  if (stmt->scope() != NULL) {
    LoadContextField(context_register(), Context::PREVIOUS_INDEX);
    // Update local stack frame context field.
    StoreToFrameField(StandardFrameConstants::kContextOffset,
                      context_register());
  }
}


void FullCodeGenerator::VisitModuleStatement(ModuleStatement* stmt) {
  Comment cmnt(masm_, "[ Module context");

  __ Push(Smi::FromInt(stmt->proxy()->interface()->Index()));
  __ Push(Smi::FromInt(0));
  __ CallRuntime(Runtime::kPushModuleContext, 2);
  StoreToFrameField(
      StandardFrameConstants::kContextOffset, context_register());

  Scope* saved_scope = scope_;
  scope_ = stmt->body()->scope();
  VisitStatements(stmt->body()->statements());
  scope_ = saved_scope;
  LoadContextField(context_register(), Context::PREVIOUS_INDEX);
  // Update local stack frame context field.
  StoreToFrameField(StandardFrameConstants::kContextOffset,
                    context_register());
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


void FullCodeGenerator::EmitUnwindBeforeReturn() {
  NestedStatement* current = nesting_stack_;
  int stack_depth = 0;
  int context_length = 0;
  while (current != NULL) {
    current = current->Exit(&stack_depth, &context_length);
  }
  __ Drop(stack_depth);
}


void FullCodeGenerator::VisitReturnStatement(ReturnStatement* stmt) {
  Comment cmnt(masm_, "[ ReturnStatement");
  SetStatementPosition(stmt);
  Expression* expr = stmt->expression();
  VisitForAccumulatorValue(expr);
  EmitUnwindBeforeReturn();
  EmitReturnSequence();
}


void FullCodeGenerator::VisitWithStatement(WithStatement* stmt) {
  Comment cmnt(masm_, "[ WithStatement");
  SetStatementPosition(stmt);

  VisitForStackValue(stmt->expression());
  PushFunctionArgumentForContextAllocation();
  __ CallRuntime(Runtime::kPushWithContext, 2);
  StoreToFrameField(StandardFrameConstants::kContextOffset, context_register());

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
  SetStatementPosition(stmt);
  Label body, book_keeping;

  Iteration loop_statement(this, stmt);
  increment_loop_depth();

  __ bind(&body);
  Visit(stmt->body());

  // Record the position of the do while condition and make sure it is
  // possible to break on the condition.
  __ bind(loop_statement.continue_label());
  PrepareForBailoutForId(stmt->ContinueId(), NO_REGISTERS);
  SetExpressionPosition(stmt->cond());
  VisitForControl(stmt->cond(),
                  &book_keeping,
                  loop_statement.break_label(),
                  &book_keeping);

  // Check stack before looping.
  PrepareForBailoutForId(stmt->BackEdgeId(), NO_REGISTERS);
  __ bind(&book_keeping);
  EmitBackEdgeBookkeeping(stmt, &body);
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
  EmitBackEdgeBookkeeping(stmt, &body);

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

  // Set statement position for a break slot before entering the for-body.
  SetStatementPosition(stmt);

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
  if (stmt->next() != NULL) {
    Visit(stmt->next());
  }

  // Emit the statement position here as this is where the for
  // statement code starts.
  SetStatementPosition(stmt);

  // Check stack before looping.
  EmitBackEdgeBookkeeping(stmt, &body);

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
  // The try block adds a handler to the exception handler chain before
  // entering, and removes it again when exiting normally.  If an exception
  // is thrown during execution of the try block, the handler is consumed
  // and control is passed to the catch block with the exception in the
  // result register.

  Label try_entry, handler_entry, exit;
  __ jmp(&try_entry);
  __ bind(&handler_entry);
  handler_table()->set(stmt->index(), Smi::FromInt(handler_entry.pos()));
  // Exception handler code, the exception is in the result register.
  // Extend the context before executing the catch block.
  { Comment cmnt(masm_, "[ Extend catch context");
    __ Push(stmt->variable()->name());
    __ Push(result_register());
    PushFunctionArgumentForContextAllocation();
    __ CallRuntime(Runtime::kPushCatchContext, 3);
    StoreToFrameField(StandardFrameConstants::kContextOffset,
                      context_register());
  }

  Scope* saved_scope = scope();
  scope_ = stmt->scope();
  ASSERT(scope_->declarations()->is_empty());
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
  __ PushTryHandler(StackHandler::CATCH, stmt->index());
  { TryCatch try_body(this);
    Visit(stmt->try_block());
  }
  __ PopTryHandler();
  __ bind(&exit);
}


void FullCodeGenerator::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  Comment cmnt(masm_, "[ TryFinallyStatement");
  SetStatementPosition(stmt);
  // Try finally is compiled by setting up a try-handler on the stack while
  // executing the try body, and removing it again afterwards.
  //
  // The try-finally construct can enter the finally block in three ways:
  // 1. By exiting the try-block normally. This removes the try-handler and
  //    calls the finally block code before continuing.
  // 2. By exiting the try-block with a function-local control flow transfer
  //    (break/continue/return). The site of the, e.g., break removes the
  //    try handler and calls the finally block code before continuing
  //    its outward control transfer.
  // 3. By exiting the try-block with a thrown exception.
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
  Label try_entry, handler_entry, finally_entry;

  // Jump to try-handler setup and try-block code.
  __ jmp(&try_entry);
  __ bind(&handler_entry);
  handler_table()->set(stmt->index(), Smi::FromInt(handler_entry.pos()));
  // Exception handler code.  This code is only executed when an exception
  // is thrown.  The exception is in the result register, and must be
  // preserved by the finally block.  Call the finally block and then
  // rethrow the exception if it returns.
  __ Call(&finally_entry);
  __ Push(result_register());
  __ CallRuntime(Runtime::kReThrow, 1);

  // Finally block implementation.
  __ bind(&finally_entry);
  EnterFinallyBlock();
  { Finally finally_body(this);
    Visit(stmt->finally_block());
  }
  ExitFinallyBlock();  // Return to the calling code.

  // Set up try handler.
  __ bind(&try_entry);
  __ PushTryHandler(StackHandler::FINALLY, stmt->index());
  { TryFinally try_body(this, &finally_entry);
    Visit(stmt->try_block());
  }
  __ PopTryHandler();
  // Execute the finally block on the way out.  Clobber the unpredictable
  // value in the result register with one that's safe for GC because the
  // finally block will unconditionally preserve the result register on the
  // stack.
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


void FullCodeGenerator::VisitCaseClause(CaseClause* clause) {
  UNREACHABLE();
}


void FullCodeGenerator::VisitConditional(Conditional* expr) {
  Comment cmnt(masm_, "[ Conditional");
  Label true_case, false_case, done;
  VisitForControl(expr->condition(), &true_case, &false_case, &true_case);

  PrepareForBailoutForId(expr->ThenId(), NO_REGISTERS);
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

  PrepareForBailoutForId(expr->ElseId(), NO_REGISTERS);
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
      Compiler::BuildFunctionInfo(expr, script());
  if (function_info.is_null()) {
    SetStackOverflow();
    return;
  }
  EmitNewClosure(function_info, expr->pretenure());
}


void FullCodeGenerator::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* expr) {
  Comment cmnt(masm_, "[ NativeFunctionLiteral");

  // Compute the function template for the native function.
  Handle<String> name = expr->name();
  v8::Handle<v8::FunctionTemplate> fun_template =
      expr->extension()->GetNativeFunctionTemplate(
          reinterpret_cast<v8::Isolate*>(isolate()), v8::Utils::ToLocal(name));
  ASSERT(!fun_template.IsEmpty());

  // Instantiate the function and create a shared function info from it.
  Handle<JSFunction> fun = Utils::OpenHandle(*fun_template->GetFunction());
  const int literals = fun->NumberOfLiterals();
  Handle<Code> code = Handle<Code>(fun->shared()->code());
  Handle<Code> construct_stub = Handle<Code>(fun->shared()->construct_stub());
  bool is_generator = false;
  Handle<SharedFunctionInfo> shared =
      isolate()->factory()->NewSharedFunctionInfo(name, literals, is_generator,
          code, Handle<ScopeInfo>(fun->shared()->scope_info()));
  shared->set_construct_stub(*construct_stub);

  // Copy the function data to the shared function info.
  shared->set_function_data(fun->shared()->function_data());
  int parameters = fun->shared()->formal_parameter_count();
  shared->set_formal_parameter_count(parameters);

  EmitNewClosure(shared, false);
}


void FullCodeGenerator::VisitThrow(Throw* expr) {
  Comment cmnt(masm_, "[ Throw");
  VisitForStackValue(expr->exception());
  __ CallRuntime(Runtime::kThrow, 1);
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


bool FullCodeGenerator::TryLiteralCompare(CompareOperation* expr) {
  Expression* sub_expr;
  Handle<String> check;
  if (expr->IsLiteralCompareTypeof(&sub_expr, &check)) {
    EmitLiteralCompareTypeof(expr, sub_expr, check);
    return true;
  }

  if (expr->IsLiteralCompareUndefined(&sub_expr, isolate())) {
    EmitLiteralCompareNil(expr, sub_expr, kUndefinedValue);
    return true;
  }

  if (expr->IsLiteralCompareNull(&sub_expr)) {
    EmitLiteralCompareNil(expr, sub_expr, kNullValue);
    return true;
  }

  return false;
}


void BackEdgeTable::Patch(Isolate* isolate, Code* unoptimized) {
  DisallowHeapAllocation no_gc;
  Code* patch = isolate->builtins()->builtin(Builtins::kOnStackReplacement);

  // Iterate over the back edge table and patch every interrupt
  // call to an unconditional call to the replacement code.
  int loop_nesting_level = unoptimized->allow_osr_at_loop_nesting_level();

  BackEdgeTable back_edges(unoptimized, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    if (static_cast<int>(back_edges.loop_depth(i)) == loop_nesting_level) {
      ASSERT_EQ(INTERRUPT, GetBackEdgeState(isolate,
                                            unoptimized,
                                            back_edges.pc(i)));
      PatchAt(unoptimized, back_edges.pc(i), ON_STACK_REPLACEMENT, patch);
    }
  }

  unoptimized->set_back_edges_patched_for_osr(true);
  ASSERT(Verify(isolate, unoptimized, loop_nesting_level));
}


void BackEdgeTable::Revert(Isolate* isolate, Code* unoptimized) {
  DisallowHeapAllocation no_gc;
  Code* patch = isolate->builtins()->builtin(Builtins::kInterruptCheck);

  // Iterate over the back edge table and revert the patched interrupt calls.
  ASSERT(unoptimized->back_edges_patched_for_osr());
  int loop_nesting_level = unoptimized->allow_osr_at_loop_nesting_level();

  BackEdgeTable back_edges(unoptimized, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    if (static_cast<int>(back_edges.loop_depth(i)) <= loop_nesting_level) {
      ASSERT_NE(INTERRUPT, GetBackEdgeState(isolate,
                                            unoptimized,
                                            back_edges.pc(i)));
      PatchAt(unoptimized, back_edges.pc(i), INTERRUPT, patch);
    }
  }

  unoptimized->set_back_edges_patched_for_osr(false);
  unoptimized->set_allow_osr_at_loop_nesting_level(0);
  // Assert that none of the back edges are patched anymore.
  ASSERT(Verify(isolate, unoptimized, -1));
}


void BackEdgeTable::AddStackCheck(Handle<Code> code, uint32_t pc_offset) {
  DisallowHeapAllocation no_gc;
  Isolate* isolate = code->GetIsolate();
  Address pc = code->instruction_start() + pc_offset;
  Code* patch = isolate->builtins()->builtin(Builtins::kOsrAfterStackCheck);
  PatchAt(*code, pc, OSR_AFTER_STACK_CHECK, patch);
}


void BackEdgeTable::RemoveStackCheck(Handle<Code> code, uint32_t pc_offset) {
  DisallowHeapAllocation no_gc;
  Isolate* isolate = code->GetIsolate();
  Address pc = code->instruction_start() + pc_offset;

  if (OSR_AFTER_STACK_CHECK == GetBackEdgeState(isolate, *code, pc)) {
    Code* patch = isolate->builtins()->builtin(Builtins::kOnStackReplacement);
    PatchAt(*code, pc, ON_STACK_REPLACEMENT, patch);
  }
}


#ifdef DEBUG
bool BackEdgeTable::Verify(Isolate* isolate,
                           Code* unoptimized,
                           int loop_nesting_level) {
  DisallowHeapAllocation no_gc;
  BackEdgeTable back_edges(unoptimized, &no_gc);
  for (uint32_t i = 0; i < back_edges.length(); i++) {
    uint32_t loop_depth = back_edges.loop_depth(i);
    CHECK_LE(static_cast<int>(loop_depth), Code::kMaxLoopNestingMarker);
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


#undef __


} }  // namespace v8::internal
