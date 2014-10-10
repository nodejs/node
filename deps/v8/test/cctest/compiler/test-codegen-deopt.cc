// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/compiler/code-generator.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/register-allocator.h"
#include "src/compiler/schedule.h"

#include "src/full-codegen.h"
#include "src/parser.h"
#include "src/rewriter.h"

#include "test/cctest/compiler/c-signature.h"
#include "test/cctest/compiler/function-tester.h"

using namespace v8::internal;
using namespace v8::internal::compiler;


#if V8_TURBOFAN_TARGET

typedef RawMachineAssembler::Label MLabel;

static Handle<JSFunction> NewFunction(const char* source) {
  return v8::Utils::OpenHandle(
      *v8::Handle<v8::Function>::Cast(CompileRun(source)));
}


class DeoptCodegenTester {
 public:
  explicit DeoptCodegenTester(HandleAndZoneScope* scope, const char* src)
      : scope_(scope),
        function(NewFunction(src)),
        info(function, scope->main_zone()),
        bailout_id(-1) {
    CHECK(Parser::Parse(&info));
    info.SetOptimizing(BailoutId::None(), Handle<Code>(function->code()));
    CHECK(Rewriter::Rewrite(&info));
    CHECK(Scope::Analyze(&info));
    CHECK(Compiler::EnsureDeoptimizationSupport(&info));

    DCHECK(info.shared_info()->has_deoptimization_support());

    graph = new (scope_->main_zone()) Graph(scope_->main_zone());
  }

  virtual ~DeoptCodegenTester() { delete code; }

  void GenerateCodeFromSchedule(Schedule* schedule) {
    OFStream os(stdout);
    if (FLAG_trace_turbo) {
      os << *schedule;
    }

    // Initialize the codegen and generate code.
    Linkage* linkage = new (scope_->main_zone()) Linkage(&info);
    code = new v8::internal::compiler::InstructionSequence(linkage, graph,
                                                           schedule);
    SourcePositionTable source_positions(graph);
    InstructionSelector selector(code, &source_positions);
    selector.SelectInstructions();

    if (FLAG_trace_turbo) {
      os << "----- Instruction sequence before register allocation -----\n"
         << *code;
    }

    RegisterAllocator allocator(code);
    CHECK(allocator.Allocate());

    if (FLAG_trace_turbo) {
      os << "----- Instruction sequence after register allocation -----\n"
         << *code;
    }

    compiler::CodeGenerator generator(code);
    result_code = generator.GenerateCode();

#ifdef OBJECT_PRINT
    if (FLAG_print_opt_code || FLAG_trace_turbo) {
      result_code->Print();
    }
#endif
  }

  Zone* zone() { return scope_->main_zone(); }

  HandleAndZoneScope* scope_;
  Handle<JSFunction> function;
  CompilationInfo info;
  BailoutId bailout_id;
  Handle<Code> result_code;
  v8::internal::compiler::InstructionSequence* code;
  Graph* graph;
};


class TrivialDeoptCodegenTester : public DeoptCodegenTester {
 public:
  explicit TrivialDeoptCodegenTester(HandleAndZoneScope* scope)
      : DeoptCodegenTester(scope,
                           "function foo() { deopt(); return 42; }; foo") {}

  void GenerateCode() {
    GenerateCodeFromSchedule(BuildGraphAndSchedule(graph));
  }

  Schedule* BuildGraphAndSchedule(Graph* graph) {
    CommonOperatorBuilder common(zone());

    // Manually construct a schedule for the function below:
    // function foo() {
    //   deopt();
    // }

    CSignature1<Object*, Object*> sig;
    RawMachineAssembler m(graph, &sig);

    Handle<JSFunction> deopt_function =
        NewFunction("function deopt() { %DeoptimizeFunction(foo); }; deopt");
    Unique<Object> deopt_fun_constant =
        Unique<Object>::CreateUninitialized(deopt_function);
    Node* deopt_fun_node = m.NewNode(common.HeapConstant(deopt_fun_constant));

    Handle<Context> caller_context(function->context(), CcTest::i_isolate());
    Unique<Object> caller_context_constant =
        Unique<Object>::CreateUninitialized(caller_context);
    Node* caller_context_node =
        m.NewNode(common.HeapConstant(caller_context_constant));

    bailout_id = GetCallBailoutId();
    Node* parameters = m.NewNode(common.StateValues(1), m.UndefinedConstant());
    Node* locals = m.NewNode(common.StateValues(0));
    Node* stack = m.NewNode(common.StateValues(0));

    Node* state_node = m.NewNode(
        common.FrameState(JS_FRAME, bailout_id, kIgnoreOutput), parameters,
        locals, stack, caller_context_node, m.UndefinedConstant());

    Handle<Context> context(deopt_function->context(), CcTest::i_isolate());
    Unique<Object> context_constant =
        Unique<Object>::CreateUninitialized(context);
    Node* context_node = m.NewNode(common.HeapConstant(context_constant));

    m.CallJS0(deopt_fun_node, m.UndefinedConstant(), context_node, state_node);

    m.Return(m.UndefinedConstant());

    // Schedule the graph:
    Schedule* schedule = m.Export();

    return schedule;
  }

  BailoutId GetCallBailoutId() {
    ZoneList<Statement*>* body = info.function()->body();
    for (int i = 0; i < body->length(); i++) {
      if (body->at(i)->IsExpressionStatement() &&
          body->at(i)->AsExpressionStatement()->expression()->IsCall()) {
        return body->at(i)->AsExpressionStatement()->expression()->id();
      }
    }
    CHECK(false);
    return BailoutId(-1);
  }
};


TEST(TurboTrivialDeoptCodegen) {
  HandleAndZoneScope scope;
  InitializedHandleScope handles;

  FLAG_allow_natives_syntax = true;
  FLAG_turbo_deoptimization = true;

  TrivialDeoptCodegenTester t(&scope);
  t.GenerateCode();

  DeoptimizationInputData* data =
      DeoptimizationInputData::cast(t.result_code->deoptimization_data());

  // TODO(jarin) Find a way to test the safepoint.

  // Check that we deoptimize to the right AST id.
  CHECK_EQ(1, data->DeoptCount());
  CHECK_EQ(t.bailout_id.ToInt(), data->AstId(0).ToInt());
}


TEST(TurboTrivialDeoptCodegenAndRun) {
  HandleAndZoneScope scope;
  InitializedHandleScope handles;

  FLAG_allow_natives_syntax = true;
  FLAG_turbo_deoptimization = true;

  TrivialDeoptCodegenTester t(&scope);
  t.GenerateCode();

  t.function->ReplaceCode(*t.result_code);
  t.info.context()->native_context()->AddOptimizedCode(*t.result_code);

  Isolate* isolate = scope.main_isolate();
  Handle<Object> result;
  bool has_pending_exception =
      !Execution::Call(isolate, t.function,
                       isolate->factory()->undefined_value(), 0, NULL,
                       false).ToHandle(&result);
  CHECK(!has_pending_exception);
  CHECK(result->SameValue(Smi::FromInt(42)));
}


class TrivialRuntimeDeoptCodegenTester : public DeoptCodegenTester {
 public:
  explicit TrivialRuntimeDeoptCodegenTester(HandleAndZoneScope* scope)
      : DeoptCodegenTester(
            scope,
            "function foo() { %DeoptimizeFunction(foo); return 42; }; foo") {}

  void GenerateCode() {
    GenerateCodeFromSchedule(BuildGraphAndSchedule(graph));
  }

  Schedule* BuildGraphAndSchedule(Graph* graph) {
    CommonOperatorBuilder common(zone());

    // Manually construct a schedule for the function below:
    // function foo() {
    //   %DeoptimizeFunction(foo);
    // }

    CSignature1<Object*, Object*> sig;
    RawMachineAssembler m(graph, &sig);

    Unique<Object> this_fun_constant =
        Unique<Object>::CreateUninitialized(function);
    Node* this_fun_node = m.NewNode(common.HeapConstant(this_fun_constant));

    Handle<Context> context(function->context(), CcTest::i_isolate());
    Unique<Object> context_constant =
        Unique<Object>::CreateUninitialized(context);
    Node* context_node = m.NewNode(common.HeapConstant(context_constant));

    bailout_id = GetCallBailoutId();
    Node* parameters = m.NewNode(common.StateValues(1), m.UndefinedConstant());
    Node* locals = m.NewNode(common.StateValues(0));
    Node* stack = m.NewNode(common.StateValues(0));

    Node* state_node = m.NewNode(
        common.FrameState(JS_FRAME, bailout_id, kIgnoreOutput), parameters,
        locals, stack, context_node, m.UndefinedConstant());

    m.CallRuntime1(Runtime::kDeoptimizeFunction, this_fun_node, context_node,
                   state_node);

    m.Return(m.UndefinedConstant());

    // Schedule the graph:
    Schedule* schedule = m.Export();

    return schedule;
  }

  BailoutId GetCallBailoutId() {
    ZoneList<Statement*>* body = info.function()->body();
    for (int i = 0; i < body->length(); i++) {
      if (body->at(i)->IsExpressionStatement() &&
          body->at(i)->AsExpressionStatement()->expression()->IsCallRuntime()) {
        return body->at(i)->AsExpressionStatement()->expression()->id();
      }
    }
    CHECK(false);
    return BailoutId(-1);
  }
};


TEST(TurboTrivialRuntimeDeoptCodegenAndRun) {
  HandleAndZoneScope scope;
  InitializedHandleScope handles;

  FLAG_allow_natives_syntax = true;
  FLAG_turbo_deoptimization = true;

  TrivialRuntimeDeoptCodegenTester t(&scope);
  t.GenerateCode();

  t.function->ReplaceCode(*t.result_code);
  t.info.context()->native_context()->AddOptimizedCode(*t.result_code);

  Isolate* isolate = scope.main_isolate();
  Handle<Object> result;
  bool has_pending_exception =
      !Execution::Call(isolate, t.function,
                       isolate->factory()->undefined_value(), 0, NULL,
                       false).ToHandle(&result);
  CHECK(!has_pending_exception);
  CHECK(result->SameValue(Smi::FromInt(42)));
}

#endif
