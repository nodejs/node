// Copyright 2009 the V8 project authors. All rights reserved.
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
#include "compilation-cache.h"
#include "compiler.h"
#include "debug.h"
#include "fast-codegen.h"
#include "oprofile-agent.h"
#include "rewriter.h"
#include "scopes.h"
#include "usage-analyzer.h"

namespace v8 {
namespace internal {


class CodeGenSelector: public AstVisitor {
 public:
  enum CodeGenTag { NORMAL, FAST };

  CodeGenSelector()
      : has_supported_syntax_(true),
        context_(Expression::kUninitialized) {
  }

  CodeGenTag Select(FunctionLiteral* fun);

 private:
  // Visit an expression in a given expression context.
  void ProcessExpression(Expression* expr, Expression::Context context) {
    Expression::Context saved = context_;
    context_ = context;
    Visit(expr);
    expr->set_context(context);
    context_ = saved;
  }

  void VisitDeclarations(ZoneList<Declaration*>* decls);
  void VisitStatements(ZoneList<Statement*>* stmts);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  bool has_supported_syntax_;

  // The desired expression context of the currently visited expression.
  Expression::Context context_;

  DISALLOW_COPY_AND_ASSIGN(CodeGenSelector);
};


static Handle<Code> MakeCode(FunctionLiteral* literal,
                             Handle<Script> script,
                             Handle<Context> context,
                             bool is_eval,
                             Handle<SharedFunctionInfo> shared) {
  ASSERT(literal != NULL);

  // Rewrite the AST by introducing .result assignments where needed.
  if (!Rewriter::Process(literal) || !AnalyzeVariableUsage(literal)) {
    // Signal a stack overflow by returning a null handle.  The stack
    // overflow exception will be thrown by the caller.
    return Handle<Code>::null();
  }

  {
    // Compute top scope and allocate variables. For lazy compilation
    // the top scope only contains the single lazily compiled function,
    // so this doesn't re-allocate variables repeatedly.
    HistogramTimerScope timer(&Counters::variable_allocation);
    Scope* top = literal->scope();
    while (top->outer_scope() != NULL) top = top->outer_scope();
    top->AllocateVariables(context);
  }

#ifdef DEBUG
  if (Bootstrapper::IsActive() ?
      FLAG_print_builtin_scopes :
      FLAG_print_scopes) {
    literal->scope()->Print();
  }
#endif

  // Optimize the AST.
  if (!Rewriter::Optimize(literal)) {
    // Signal a stack overflow by returning a null handle.  The stack
    // overflow exception will be thrown by the caller.
    return Handle<Code>::null();
  }

  // Generate code and return it.
  if (FLAG_fast_compiler) {
    // If there is no shared function info, try the fast code
    // generator for code in the global scope.  Otherwise obey the
    // explicit hint in the shared function info.
    if (shared.is_null() && !literal->scope()->is_global_scope()) {
      if (FLAG_trace_bailout) PrintF("Non-global scope\n");
    } else if (!shared.is_null() && !shared->try_fast_codegen()) {
      if (FLAG_trace_bailout) PrintF("No hint to try fast\n");
    } else {
      CodeGenSelector selector;
      CodeGenSelector::CodeGenTag code_gen = selector.Select(literal);
      if (code_gen == CodeGenSelector::FAST) {
        return FastCodeGenerator::MakeCode(literal, script, is_eval);
      }
      ASSERT(code_gen == CodeGenSelector::NORMAL);
    }
  }
  return CodeGenerator::MakeCode(literal, script, is_eval);
}


static bool IsValidJSON(FunctionLiteral* lit) {
  if (lit->body()->length() != 1)
    return false;
  Statement* stmt = lit->body()->at(0);
  if (stmt->AsExpressionStatement() == NULL)
    return false;
  Expression* expr = stmt->AsExpressionStatement()->expression();
  return expr->IsValidJSON();
}


static Handle<JSFunction> MakeFunction(bool is_global,
                                       bool is_eval,
                                       Compiler::ValidationState validate,
                                       Handle<Script> script,
                                       Handle<Context> context,
                                       v8::Extension* extension,
                                       ScriptDataImpl* pre_data) {
  CompilationZoneScope zone_scope(DELETE_ON_EXIT);

  PostponeInterruptsScope postpone;

  ASSERT(!i::Top::global_context().is_null());
  script->set_context_data((*i::Top::global_context())->data());

#ifdef ENABLE_DEBUGGER_SUPPORT
  bool is_json = (validate == Compiler::VALIDATE_JSON);
  if (is_eval || is_json) {
    script->set_compilation_type(
        is_json ? Smi::FromInt(Script::COMPILATION_TYPE_JSON) :
                               Smi::FromInt(Script::COMPILATION_TYPE_EVAL));
    // For eval scripts add information on the function from which eval was
    // called.
    if (is_eval) {
      JavaScriptFrameIterator it;
      script->set_eval_from_function(it.frame()->function());
      int offset = static_cast<int>(
          it.frame()->pc() - it.frame()->code()->instruction_start());
      script->set_eval_from_instructions_offset(Smi::FromInt(offset));
    }
  }

  // Notify debugger
  Debugger::OnBeforeCompile(script);
#endif

  // Only allow non-global compiles for eval.
  ASSERT(is_eval || is_global);

  // Build AST.
  FunctionLiteral* lit = MakeAST(is_global, script, extension, pre_data);

  // Check for parse errors.
  if (lit == NULL) {
    ASSERT(Top::has_pending_exception());
    return Handle<JSFunction>::null();
  }

  // When parsing JSON we do an ordinary parse and then afterwards
  // check the AST to ensure it was well-formed.  If not we give a
  // syntax error.
  if (validate == Compiler::VALIDATE_JSON && !IsValidJSON(lit)) {
    HandleScope scope;
    Handle<JSArray> args = Factory::NewJSArray(1);
    Handle<Object> source(script->source());
    SetElement(args, 0, source);
    Handle<Object> result = Factory::NewSyntaxError("invalid_json", args);
    Top::Throw(*result, NULL);
    return Handle<JSFunction>::null();
  }

  // Measure how long it takes to do the compilation; only take the
  // rest of the function into account to avoid overlap with the
  // parsing statistics.
  HistogramTimer* rate = is_eval
      ? &Counters::compile_eval
      : &Counters::compile;
  HistogramTimerScope timer(rate);

  // Compile the code.
  Handle<Code> code = MakeCode(lit, script, context, is_eval,
                               Handle<SharedFunctionInfo>::null());

  // Check for stack-overflow exceptions.
  if (code.is_null()) {
    Top::StackOverflow();
    return Handle<JSFunction>::null();
  }

#if defined ENABLE_LOGGING_AND_PROFILING || defined ENABLE_OPROFILE_AGENT
  // Log the code generation for the script. Check explicit whether logging is
  // to avoid allocating when not required.
  if (Logger::is_logging() || OProfileAgent::is_enabled()) {
    if (script->name()->IsString()) {
      SmartPointer<char> data =
          String::cast(script->name())->ToCString(DISALLOW_NULLS);
      LOG(CodeCreateEvent(is_eval ? Logger::EVAL_TAG : Logger::SCRIPT_TAG,
                          *code, *data));
      OProfileAgent::CreateNativeCodeRegion(*data,
                                            code->instruction_start(),
                                            code->instruction_size());
    } else {
      LOG(CodeCreateEvent(is_eval ? Logger::EVAL_TAG : Logger::SCRIPT_TAG,
                          *code, ""));
      OProfileAgent::CreateNativeCodeRegion(is_eval ? "Eval" : "Script",
                                            code->instruction_start(),
                                            code->instruction_size());
    }
  }
#endif

  // Allocate function.
  Handle<JSFunction> fun =
      Factory::NewFunctionBoilerplate(lit->name(),
                                      lit->materialized_literal_count(),
                                      code);

  ASSERT_EQ(RelocInfo::kNoPosition, lit->function_token_position());
  Compiler::SetFunctionInfo(fun, lit, true, script);

  // Hint to the runtime system used when allocating space for initial
  // property space by setting the expected number of properties for
  // the instances of the function.
  SetExpectedNofPropertiesFromEstimate(fun, lit->expected_property_count());

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger
  Debugger::OnAfterCompile(script, fun);
#endif

  return fun;
}


static StaticResource<SafeStringInputBuffer> safe_string_input_buffer;


Handle<JSFunction> Compiler::Compile(Handle<String> source,
                                     Handle<Object> script_name,
                                     int line_offset, int column_offset,
                                     v8::Extension* extension,
                                     ScriptDataImpl* input_pre_data) {
  int source_length = source->length();
  Counters::total_load_size.Increment(source_length);
  Counters::total_compile_size.Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  // Do a lookup in the compilation cache but not for extensions.
  Handle<JSFunction> result;
  if (extension == NULL) {
    result = CompilationCache::LookupScript(source,
                                            script_name,
                                            line_offset,
                                            column_offset);
  }

  if (result.is_null()) {
    // No cache entry found. Do pre-parsing and compile the script.
    ScriptDataImpl* pre_data = input_pre_data;
    if (pre_data == NULL && source_length >= FLAG_min_preparse_length) {
      Access<SafeStringInputBuffer> buf(&safe_string_input_buffer);
      buf->Reset(source.location());
      pre_data = PreParse(source, buf.value(), extension);
    }

    // Create a script object describing the script to be compiled.
    Handle<Script> script = Factory::NewScript(source);
    if (!script_name.is_null()) {
      script->set_name(*script_name);
      script->set_line_offset(Smi::FromInt(line_offset));
      script->set_column_offset(Smi::FromInt(column_offset));
    }

    // Compile the function and add it to the cache.
    result = MakeFunction(true,
                          false,
                          DONT_VALIDATE_JSON,
                          script,
                          Handle<Context>::null(),
                          extension,
                          pre_data);
    if (extension == NULL && !result.is_null()) {
      CompilationCache::PutScript(source, result);
    }

    // Get rid of the pre-parsing data (if necessary).
    if (input_pre_data == NULL && pre_data != NULL) {
      delete pre_data;
    }
  }

  if (result.is_null()) Top::ReportPendingMessages();
  return result;
}


Handle<JSFunction> Compiler::CompileEval(Handle<String> source,
                                         Handle<Context> context,
                                         bool is_global,
                                         ValidationState validate) {
  // Note that if validation is required then no path through this
  // function is allowed to return a value without validating that
  // the input is legal json.

  int source_length = source->length();
  Counters::total_eval_size.Increment(source_length);
  Counters::total_compile_size.Increment(source_length);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  // Do a lookup in the compilation cache; if the entry is not there,
  // invoke the compiler and add the result to the cache.  If we're
  // evaluating json we bypass the cache since we can't be sure a
  // potential value in the cache has been validated.
  Handle<JSFunction> result;
  if (validate == DONT_VALIDATE_JSON)
    result = CompilationCache::LookupEval(source, context, is_global);

  if (result.is_null()) {
    // Create a script object describing the script to be compiled.
    Handle<Script> script = Factory::NewScript(source);
    result = MakeFunction(is_global,
                          true,
                          validate,
                          script,
                          context,
                          NULL,
                          NULL);
    if (!result.is_null() && validate != VALIDATE_JSON) {
      // For json it's unlikely that we'll ever see exactly the same
      // string again so we don't use the compilation cache.
      CompilationCache::PutEval(source, context, is_global, result);
    }
  }

  return result;
}


bool Compiler::CompileLazy(Handle<SharedFunctionInfo> shared,
                           int loop_nesting) {
  CompilationZoneScope zone_scope(DELETE_ON_EXIT);

  // The VM is in the COMPILER state until exiting this function.
  VMState state(COMPILER);

  PostponeInterruptsScope postpone;

  // Compute name, source code and script data.
  Handle<String> name(String::cast(shared->name()));
  Handle<Script> script(Script::cast(shared->script()));

  int start_position = shared->start_position();
  int end_position = shared->end_position();
  bool is_expression = shared->is_expression();
  Counters::total_compile_size.Increment(end_position - start_position);

  // Generate the AST for the lazily compiled function. The AST may be
  // NULL in case of parser stack overflow.
  FunctionLiteral* lit = MakeLazyAST(script, name,
                                     start_position,
                                     end_position,
                                     is_expression);

  // Check for parse errors.
  if (lit == NULL) {
    ASSERT(Top::has_pending_exception());
    return false;
  }

  // Update the loop nesting in the function literal.
  lit->set_loop_nesting(loop_nesting);

  // Measure how long it takes to do the lazy compilation; only take
  // the rest of the function into account to avoid overlap with the
  // lazy parsing statistics.
  HistogramTimerScope timer(&Counters::compile_lazy);

  // Compile the code.
  Handle<Code> code = MakeCode(lit, script, Handle<Context>::null(), false,
                               shared);

  // Check for stack-overflow exception.
  if (code.is_null()) {
    Top::StackOverflow();
    return false;
  }

#if defined ENABLE_LOGGING_AND_PROFILING || defined ENABLE_OPROFILE_AGENT
  // Log the code generation. If source information is available include script
  // name and line number. Check explicit whether logging is enabled as finding
  // the line number is not for free.
  if (Logger::is_logging() || OProfileAgent::is_enabled()) {
    Handle<String> func_name(name->length() > 0 ?
                             *name : shared->inferred_name());
    if (script->name()->IsString()) {
      int line_num = GetScriptLineNumber(script, start_position) + 1;
      LOG(CodeCreateEvent(Logger::LAZY_COMPILE_TAG, *code, *func_name,
                          String::cast(script->name()), line_num));
      OProfileAgent::CreateNativeCodeRegion(*func_name,
                                            String::cast(script->name()),
                                            line_num,
                                            code->instruction_start(),
                                            code->instruction_size());
    } else {
      LOG(CodeCreateEvent(Logger::LAZY_COMPILE_TAG, *code, *func_name));
      OProfileAgent::CreateNativeCodeRegion(*func_name,
                                            code->instruction_start(),
                                            code->instruction_size());
    }
  }
#endif

  // Update the shared function info with the compiled code.
  shared->set_code(*code);

  // Set the expected number of properties for instances.
  SetExpectedNofPropertiesFromEstimate(shared, lit->expected_property_count());

  // Set the optimication hints after performing lazy compilation, as these are
  // not set when the function is set up as a lazily compiled function.
  shared->SetThisPropertyAssignmentsInfo(
      lit->has_only_simple_this_property_assignments(),
      *lit->this_property_assignments());

  // Check the function has compiled code.
  ASSERT(shared->is_compiled());
  return true;
}


Handle<JSFunction> Compiler::BuildBoilerplate(FunctionLiteral* literal,
                                              Handle<Script> script,
                                              AstVisitor* caller) {
#ifdef DEBUG
  // We should not try to compile the same function literal more than
  // once.
  literal->mark_as_compiled();
#endif

  // Determine if the function can be lazily compiled. This is
  // necessary to allow some of our builtin JS files to be lazily
  // compiled. These builtins cannot be handled lazily by the parser,
  // since we have to know if a function uses the special natives
  // syntax, which is something the parser records.
  bool allow_lazy = literal->AllowsLazyCompilation();

  // Generate code
  Handle<Code> code;
  if (FLAG_lazy && allow_lazy) {
    code = ComputeLazyCompile(literal->num_parameters());
  } else {
    // The bodies of function literals have not yet been visited by
    // the AST optimizer/analyzer.
    if (!Rewriter::Optimize(literal)) {
      return Handle<JSFunction>::null();
    }

    // Generate code and return it.
    bool is_compiled = false;
    if (FLAG_fast_compiler && literal->try_fast_codegen()) {
      CodeGenSelector selector;
      CodeGenSelector::CodeGenTag code_gen = selector.Select(literal);
      if (code_gen == CodeGenSelector::FAST) {
        code = FastCodeGenerator::MakeCode(literal,
                                           script,
                                           false);  // Not eval.
        is_compiled = true;
      }
    }

    if (!is_compiled) {
      // We didn't try the fast compiler, or we failed to select it.
      code = CodeGenerator::MakeCode(literal,
                                     script,
                                     false);  // Not eval.
    }

    // Check for stack-overflow exception.
    if (code.is_null()) {
      caller->SetStackOverflow();
      return Handle<JSFunction>::null();
    }

    // Function compilation complete.
    LOG(CodeCreateEvent(Logger::FUNCTION_TAG, *code, *literal->name()));

#ifdef ENABLE_OPROFILE_AGENT
    OProfileAgent::CreateNativeCodeRegion(*node->name(),
                                          code->instruction_start(),
                                          code->instruction_size());
#endif
  }

  // Create a boilerplate function.
  Handle<JSFunction> function =
      Factory::NewFunctionBoilerplate(literal->name(),
                                      literal->materialized_literal_count(),
                                      code);
  SetFunctionInfo(function, literal, false, script);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Notify debugger that a new function has been added.
  Debugger::OnNewFunction(function);
#endif

  // Set the expected number of properties for instances and return
  // the resulting function.
  SetExpectedNofPropertiesFromEstimate(function,
                                       literal->expected_property_count());
  return function;
}


// Sets the function info on a function.
// The start_position points to the first '(' character after the function name
// in the full script source. When counting characters in the script source the
// the first character is number 0 (not 1).
void Compiler::SetFunctionInfo(Handle<JSFunction> fun,
                               FunctionLiteral* lit,
                               bool is_toplevel,
                               Handle<Script> script) {
  fun->shared()->set_length(lit->num_parameters());
  fun->shared()->set_formal_parameter_count(lit->num_parameters());
  fun->shared()->set_script(*script);
  fun->shared()->set_function_token_position(lit->function_token_position());
  fun->shared()->set_start_position(lit->start_position());
  fun->shared()->set_end_position(lit->end_position());
  fun->shared()->set_is_expression(lit->is_expression());
  fun->shared()->set_is_toplevel(is_toplevel);
  fun->shared()->set_inferred_name(*lit->inferred_name());
  fun->shared()->SetThisPropertyAssignmentsInfo(
      lit->has_only_simple_this_property_assignments(),
      *lit->this_property_assignments());
  fun->shared()->set_try_fast_codegen(lit->try_fast_codegen());
}


CodeGenSelector::CodeGenTag CodeGenSelector::Select(FunctionLiteral* fun) {
  Scope* scope = fun->scope();

  if (scope->num_heap_slots() > 0) {
    // We support functions with a local context if they do not have
    // parameters that need to be copied into the context.
    for (int i = 0, len = scope->num_parameters(); i < len; i++) {
      Slot* slot = scope->parameter(i)->slot();
      if (slot != NULL && slot->type() == Slot::CONTEXT) {
        if (FLAG_trace_bailout) {
          PrintF("function has context-allocated parameters");
        }
        return NORMAL;
      }
    }
  }

  if (scope->arguments() != NULL) {
    if (FLAG_trace_bailout) PrintF("function uses 'arguments'\n");
    return NORMAL;
  }

  has_supported_syntax_ = true;
  VisitDeclarations(scope->declarations());
  if (!has_supported_syntax_) return NORMAL;

  VisitStatements(fun->body());
  return has_supported_syntax_ ? FAST : NORMAL;
}


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


void CodeGenSelector::VisitDeclarations(ZoneList<Declaration*>* decls) {
  for (int i = 0; i < decls->length(); i++) {
    Visit(decls->at(i));
    CHECK_BAILOUT;
  }
}


void CodeGenSelector::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0, len = stmts->length(); i < len; i++) {
    Visit(stmts->at(i));
    CHECK_BAILOUT;
  }
}


void CodeGenSelector::VisitDeclaration(Declaration* decl) {
  if (decl->fun() != NULL) {
    ProcessExpression(decl->fun(), Expression::kValue);
  }
}


void CodeGenSelector::VisitBlock(Block* stmt) {
  VisitStatements(stmt->statements());
}


void CodeGenSelector::VisitExpressionStatement(ExpressionStatement* stmt) {
  ProcessExpression(stmt->expression(), Expression::kEffect);
}


void CodeGenSelector::VisitEmptyStatement(EmptyStatement* stmt) {
  // EmptyStatement is supported.
}


void CodeGenSelector::VisitIfStatement(IfStatement* stmt) {
  ProcessExpression(stmt->condition(), Expression::kTest);
  CHECK_BAILOUT;
  Visit(stmt->then_statement());
  CHECK_BAILOUT;
  Visit(stmt->else_statement());
}


void CodeGenSelector::VisitContinueStatement(ContinueStatement* stmt) {
  BAILOUT("ContinueStatement");
}


void CodeGenSelector::VisitBreakStatement(BreakStatement* stmt) {
  BAILOUT("BreakStatement");
}


void CodeGenSelector::VisitReturnStatement(ReturnStatement* stmt) {
  ProcessExpression(stmt->expression(), Expression::kValue);
}


void CodeGenSelector::VisitWithEnterStatement(WithEnterStatement* stmt) {
  BAILOUT("WithEnterStatement");
}


void CodeGenSelector::VisitWithExitStatement(WithExitStatement* stmt) {
  BAILOUT("WithExitStatement");
}


void CodeGenSelector::VisitSwitchStatement(SwitchStatement* stmt) {
  BAILOUT("SwitchStatement");
}


void CodeGenSelector::VisitDoWhileStatement(DoWhileStatement* stmt) {
  // We do not handle loops with breaks or continue statements in their
  // body.  We will bailout when we hit those statements in the body.
  ProcessExpression(stmt->cond(), Expression::kTest);
  CHECK_BAILOUT;
  Visit(stmt->body());
}


void CodeGenSelector::VisitWhileStatement(WhileStatement* stmt) {
  // We do not handle loops with breaks or continue statements in their
  // body.  We will bailout when we hit those statements in the body.
  ProcessExpression(stmt->cond(), Expression::kTest);
  CHECK_BAILOUT;
  Visit(stmt->body());
}


void CodeGenSelector::VisitForStatement(ForStatement* stmt) {
  // We do not handle loops with breaks or continue statements in their
  // body.  We will bailout when we hit those statements in the body.
  if (stmt->init() != NULL) {
    Visit(stmt->init());
    CHECK_BAILOUT;
  }
  if (stmt->cond() != NULL) {
    ProcessExpression(stmt->cond(), Expression::kTest);
    CHECK_BAILOUT;
  }
  Visit(stmt->body());
  if (stmt->next() != NULL) {
    CHECK_BAILOUT;
    Visit(stmt->next());
  }
}


void CodeGenSelector::VisitForInStatement(ForInStatement* stmt) {
  BAILOUT("ForInStatement");
}


void CodeGenSelector::VisitTryCatchStatement(TryCatchStatement* stmt) {
  BAILOUT("TryCatchStatement");
}


void CodeGenSelector::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  BAILOUT("TryFinallyStatement");
}


void CodeGenSelector::VisitDebuggerStatement(DebuggerStatement* stmt) {
  // Debugger statement is supported.
}


void CodeGenSelector::VisitFunctionLiteral(FunctionLiteral* expr) {
  // Function literal is supported.
}


void CodeGenSelector::VisitFunctionBoilerplateLiteral(
    FunctionBoilerplateLiteral* expr) {
  BAILOUT("FunctionBoilerplateLiteral");
}


void CodeGenSelector::VisitConditional(Conditional* expr) {
  ProcessExpression(expr->condition(), Expression::kTest);
  CHECK_BAILOUT;
  ProcessExpression(expr->then_expression(), context_);
  CHECK_BAILOUT;
  ProcessExpression(expr->else_expression(), context_);
}


void CodeGenSelector::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void CodeGenSelector::VisitVariableProxy(VariableProxy* expr) {
  Expression* rewrite = expr->var()->rewrite();
  // A rewrite of NULL indicates a global variable.
  if (rewrite != NULL) {
    // Non-global.
    Slot* slot = rewrite->AsSlot();
    if (slot != NULL) {
      Slot::Type type = slot->type();
      // When LOOKUP slots are enabled, some currently dead code
      // implementing unary typeof will become live.
      if (type == Slot::LOOKUP) {
        BAILOUT("Lookup slot");
      }
    } else {
      BAILOUT("access to arguments object");
    }
  }
}


void CodeGenSelector::VisitLiteral(Literal* expr) {
  /* Nothing to do. */
}


void CodeGenSelector::VisitRegExpLiteral(RegExpLiteral* expr) {
  /* Nothing to do. */
}


void CodeGenSelector::VisitObjectLiteral(ObjectLiteral* expr) {
  ZoneList<ObjectLiteral::Property*>* properties = expr->properties();

  for (int i = 0, len = properties->length(); i < len; i++) {
    ObjectLiteral::Property* property = properties->at(i);
    if (property->IsCompileTimeValue()) continue;

    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();

      // For (non-compile-time) materialized literals and computed
      // properties with symbolic keys we will use an IC and therefore not
      // generate code for the key.
      case ObjectLiteral::Property::COMPUTED:  // Fall through.
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        if (property->key()->handle()->IsSymbol()) {
          break;
        }
        // Fall through.

      // In all other cases we need the key's value on the stack
      // for a runtime call.  (Relies on TEMP meaning STACK.)
      case ObjectLiteral::Property::GETTER:  // Fall through.
      case ObjectLiteral::Property::SETTER:  // Fall through.
      case ObjectLiteral::Property::PROTOTYPE:
        ProcessExpression(property->key(), Expression::kValue);
        CHECK_BAILOUT;
        break;
    }
    ProcessExpression(property->value(), Expression::kValue);
    CHECK_BAILOUT;
  }
}


void CodeGenSelector::VisitArrayLiteral(ArrayLiteral* expr) {
  ZoneList<Expression*>* subexprs = expr->values();
  for (int i = 0, len = subexprs->length(); i < len; i++) {
    Expression* subexpr = subexprs->at(i);
    if (subexpr->AsLiteral() != NULL) continue;
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;
    ProcessExpression(subexpr, Expression::kValue);
    CHECK_BAILOUT;
  }
}


void CodeGenSelector::VisitCatchExtensionObject(CatchExtensionObject* expr) {
  BAILOUT("CatchExtensionObject");
}


void CodeGenSelector::VisitAssignment(Assignment* expr) {
  // We support plain non-compound assignments to properties, parameters and
  // non-context (stack-allocated) locals, and global variables.
  Token::Value op = expr->op();
  if (op == Token::INIT_CONST) BAILOUT("initialize constant");
  if (op != Token::ASSIGN && op != Token::INIT_VAR) {
    BAILOUT("compound assignment");
  }

  Variable* var = expr->target()->AsVariableProxy()->AsVariable();
  Property* prop = expr->target()->AsProperty();
  if (var != NULL) {
    // All global variables are supported.
    if (!var->is_global()) {
      if (var->slot() == NULL) {
        // This is a parameter that has rewritten to an arguments access.
        BAILOUT("non-global/non-slot assignment");
      }
      Slot::Type type = var->slot()->type();
      if (type == Slot::LOOKUP) {
        BAILOUT("Lookup slot");
      }
    }
  } else if (prop != NULL) {
    ProcessExpression(prop->obj(), Expression::kValue);
    CHECK_BAILOUT;
    // We will only visit the key during code generation for keyed property
    // stores.  Leave its expression context uninitialized for named
    // property stores.
    Literal* lit = prop->key()->AsLiteral();
    uint32_t ignored;
    if (lit == NULL ||
        !lit->handle()->IsSymbol() ||
        String::cast(*(lit->handle()))->AsArrayIndex(&ignored)) {
      ProcessExpression(prop->key(), Expression::kValue);
      CHECK_BAILOUT;
    }
  } else {
    // This is a throw reference error.
    BAILOUT("non-variable/non-property assignment");
  }

  ProcessExpression(expr->value(), Expression::kValue);
}


void CodeGenSelector::VisitThrow(Throw* expr) {
  BAILOUT("Throw");
}


void CodeGenSelector::VisitProperty(Property* expr) {
  ProcessExpression(expr->obj(), Expression::kValue);
  CHECK_BAILOUT;
  ProcessExpression(expr->key(), Expression::kValue);
}


void CodeGenSelector::VisitCall(Call* expr) {
  Expression* fun = expr->expression();
  ZoneList<Expression*>* args = expr->arguments();
  Variable* var = fun->AsVariableProxy()->AsVariable();

  // Check for supported calls
  if (var != NULL && var->is_possibly_eval()) {
    BAILOUT("call to the identifier 'eval'");
  } else if (var != NULL && !var->is_this() && var->is_global()) {
    // Calls to global variables are supported.
  } else if (var != NULL && var->slot() != NULL &&
             var->slot()->type() == Slot::LOOKUP) {
    BAILOUT("call to a lookup slot");
  } else if (fun->AsProperty() != NULL) {
    Property* prop = fun->AsProperty();
    Literal* literal_key = prop->key()->AsLiteral();
    if (literal_key != NULL && literal_key->handle()->IsSymbol()) {
      ProcessExpression(prop->obj(), Expression::kValue);
      CHECK_BAILOUT;
    } else {
      ProcessExpression(prop->obj(), Expression::kValue);
      CHECK_BAILOUT;
      ProcessExpression(prop->key(), Expression::kValue);
      CHECK_BAILOUT;
    }
  } else {
    // Otherwise the call is supported if the function expression is.
    ProcessExpression(fun, Expression::kValue);
  }
  // Check all arguments to the call.
  for (int i = 0; i < args->length(); i++) {
    ProcessExpression(args->at(i), Expression::kValue);
    CHECK_BAILOUT;
  }
}


void CodeGenSelector::VisitCallNew(CallNew* expr) {
  ProcessExpression(expr->expression(), Expression::kValue);
  CHECK_BAILOUT;
  ZoneList<Expression*>* args = expr->arguments();
  // Check all arguments to the call
  for (int i = 0; i < args->length(); i++) {
    ProcessExpression(args->at(i), Expression::kValue);
    CHECK_BAILOUT;
  }
}


void CodeGenSelector::VisitCallRuntime(CallRuntime* expr) {
  // In case of JS runtime function bail out.
  if (expr->function() == NULL) BAILOUT("call JS runtime function");
  // Check for inline runtime call
  if (expr->name()->Get(0) == '_' &&
      CodeGenerator::FindInlineRuntimeLUT(expr->name()) != NULL) {
    BAILOUT("inlined runtime call");
  }
  // Check all arguments to the call.  (Relies on TEMP meaning STACK.)
  for (int i = 0; i < expr->arguments()->length(); i++) {
    ProcessExpression(expr->arguments()->at(i), Expression::kValue);
    CHECK_BAILOUT;
  }
}


void CodeGenSelector::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::VOID:
      ProcessExpression(expr->expression(), Expression::kEffect);
      break;
    case Token::NOT:
      ProcessExpression(expr->expression(), Expression::kTest);
      break;
    case Token::TYPEOF:
      ProcessExpression(expr->expression(), Expression::kValue);
      break;
    default:
      BAILOUT("UnaryOperation");
  }
}


void CodeGenSelector::VisitCountOperation(CountOperation* expr) {
  // We support postfix count operations on global variables.
  if (expr->is_prefix()) BAILOUT("Prefix CountOperation");
  Variable* var = expr->expression()->AsVariableProxy()->AsVariable();
  if (var == NULL || !var->is_global()) BAILOUT("non-global postincrement");
  ProcessExpression(expr->expression(), Expression::kValue);
}


void CodeGenSelector::VisitBinaryOperation(BinaryOperation* expr) {
  switch (expr->op()) {
    case Token::COMMA:
      ProcessExpression(expr->left(), Expression::kEffect);
      CHECK_BAILOUT;
      ProcessExpression(expr->right(), context_);
      break;

    case Token::OR:
      switch (context_) {
        case Expression::kUninitialized:
          UNREACHABLE();
        case Expression::kEffect:  // Fall through.
        case Expression::kTest:  // Fall through.
        case Expression::kTestValue:
          // The left subexpression's value is not needed, it is in a pure
          // test context.
          ProcessExpression(expr->left(), Expression::kTest);
          break;
        case Expression::kValue:  // Fall through.
        case Expression::kValueTest:
          // The left subexpression's value is needed, it is in a hybrid
          // value/test context.
          ProcessExpression(expr->left(), Expression::kValueTest);
          break;
      }
      CHECK_BAILOUT;
      ProcessExpression(expr->right(), context_);
      break;

    case Token::AND:
      switch (context_) {
        case Expression::kUninitialized:
          UNREACHABLE();
        case Expression::kEffect:  // Fall through.
        case Expression::kTest:  // Fall through.
        case Expression::kValueTest:
          // The left subexpression's value is not needed, it is in a pure
          // test context.
          ProcessExpression(expr->left(), Expression::kTest);
          break;
        case Expression::kValue:  // Fall through.
        case Expression::kTestValue:
          // The left subexpression's value is needed, it is in a hybrid
          // test/value context.
          ProcessExpression(expr->left(), Expression::kTestValue);
          break;
      }
      CHECK_BAILOUT;
      ProcessExpression(expr->right(), context_);
      break;

    case Token::ADD:
    case Token::SUB:
    case Token::DIV:
    case Token::MOD:
    case Token::MUL:
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SHL:
    case Token::SHR:
    case Token::SAR:
      ProcessExpression(expr->left(), Expression::kValue);
      CHECK_BAILOUT;
      ProcessExpression(expr->right(), Expression::kValue);
      break;

    default:
      BAILOUT("Unsupported binary operation");
  }
}


void CodeGenSelector::VisitCompareOperation(CompareOperation* expr) {
      ProcessExpression(expr->left(), Expression::kValue);
      CHECK_BAILOUT;
      ProcessExpression(expr->right(), Expression::kValue);
}


void CodeGenSelector::VisitThisFunction(ThisFunction* expr) {
  BAILOUT("ThisFunction");
}

#undef BAILOUT
#undef CHECK_BAILOUT


} }  // namespace v8::internal
