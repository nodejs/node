// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-scope-info.h"

#include <iterator>

#include "src/ast/scopes.h"
#include "src/debug/debug-interface.h"
#include "src/debug/debug.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/objects/debug-objects-inl.h"
#include "src/objects/scope-info.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class DebugScopeInfoTest : public TestWithIsolate {
 public:
  struct ParsedScript {
    std::unique_ptr<UnoptimizedCompileState> compile_state;
    std::unique_ptr<ReusableUnoptimizedCompileState> reusable_state;
    std::unique_ptr<ParseInfo> info;
    Handle<DebugScriptScopeInfo> scope_info;

    DeclarationScope* script_scope() const { return info->literal()->scope(); }
  };

  ParsedScript ParseAndSerialize(const char* source) {
    ParsedScript result;
    result.compile_state = std::make_unique<UnoptimizedCompileState>();
    result.reusable_state =
        std::make_unique<ReusableUnoptimizedCompileState>(isolate());
    Handle<String> source_str =
        isolate()->factory()->NewStringFromAsciiChecked(source);
    Handle<Script> script = isolate()->factory()->NewScript(source_str);
    UnoptimizedCompileFlags flags =
        UnoptimizedCompileFlags::ForScriptCompile(isolate(), *script)
            .set_is_eager(true);
    result.info = std::make_unique<ParseInfo>(isolate(), flags,
                                              result.compile_state.get(),
                                              result.reusable_state.get());
    CHECK(parsing::ParseProgram(result.info.get(), script, isolate(),
                                parsing::ReportStatisticsMode{false}));
    result.scope_info = SerializeDebugScriptScopeInfo(
        isolate(), result.info->literal()->scope());
    return result;
  }
};

namespace {

void VerifyScopeTreeParity(Scope* ast_scope, DebugScriptScope debug_scope) {
  EXPECT_EQ(ast_scope->start_position(), debug_scope.start_position());
  EXPECT_EQ(ast_scope->end_position(), debug_scope.end_position());
  EXPECT_EQ(ast_scope->scope_type(), debug_scope.scope_type());
  EXPECT_EQ(ast_scope->is_script_scope(), debug_scope.is_script_scope());
  EXPECT_EQ(ast_scope->is_function_scope(), debug_scope.is_function_scope());
  EXPECT_EQ(ast_scope->is_block_scope(), debug_scope.is_block_scope());
  EXPECT_EQ(ast_scope->is_declaration_scope(),
            debug_scope.is_declaration_scope());
  EXPECT_EQ(ast_scope->is_class_scope(), debug_scope.is_class_scope());
  EXPECT_EQ(ast_scope->is_with_scope(), debug_scope.is_with_scope());
  EXPECT_EQ(ast_scope->is_module_scope(), debug_scope.is_module_scope());
  EXPECT_EQ(ast_scope->is_eval_scope(), debug_scope.is_eval_scope());
  EXPECT_EQ(ast_scope->is_catch_scope(), debug_scope.is_catch_scope());
  EXPECT_EQ(ast_scope->is_repl_mode_scope(), debug_scope.is_repl_mode_scope());
  EXPECT_EQ(ast_scope->is_hidden(), debug_scope.is_hidden());
  EXPECT_EQ(ast_scope->language_mode(), debug_scope.language_mode());
  EXPECT_EQ(ast_scope->HasThisReference(), debug_scope.has_this_reference());
  EXPECT_EQ(ast_scope->NeedsContext(), debug_scope.needs_context());
  if (ast_scope->NeedsContext()) {
    EXPECT_EQ(ast_scope->UniqueIdInScript(), debug_scope.unique_id_in_script());
  } else {
    EXPECT_EQ(-3, debug_scope.unique_id_in_script());
  }
  if (ast_scope->is_declaration_scope()) {
    DeclarationScope* decl = ast_scope->AsDeclarationScope();
    EXPECT_EQ(decl->is_arrow_scope(), debug_scope.is_arrow_scope());
    EXPECT_EQ(decl->has_this_declaration(), debug_scope.has_this_declaration());
    EXPECT_EQ(decl->has_simple_parameters(),
              debug_scope.has_simple_parameters());
    EXPECT_EQ(decl->sloppy_eval_can_extend_vars(),
              debug_scope.sloppy_eval_can_extend_vars());
    if (decl->has_this_declaration()) {
      Variable* var = decl->receiver();
      VariableAllocationInfo expected_info =
          var->location() == VariableLocation::CONTEXT
              ? VariableAllocationInfo::CONTEXT
              : VariableAllocationInfo::STACK;
      EXPECT_EQ(debug_scope.receiver_info(),
                (std::pair{expected_info, var->index()}));
    }
    if (decl->arguments() != nullptr) {
      EXPECT_TRUE(debug_scope.has_arguments());
      Variable* var = decl->arguments();
      VariableAllocationInfo expected_info =
          var->location() == VariableLocation::CONTEXT
              ? VariableAllocationInfo::CONTEXT
              : VariableAllocationInfo::STACK;
      EXPECT_EQ(debug_scope.arguments_info(),
                (std::pair{expected_info, var->index()}));
    } else {
      EXPECT_FALSE(debug_scope.has_arguments());
      EXPECT_EQ(debug_scope.arguments_info(),
                (std::pair{VariableAllocationInfo::NONE, -1}));
    }
    if (decl->function_var() != nullptr) {
      EXPECT_TRUE(debug_scope.has_function_variable());
      Variable* var = decl->function_var();
      VariableAllocationInfo expected_info =
          var->location() == VariableLocation::CONTEXT
              ? VariableAllocationInfo::CONTEXT
              : VariableAllocationInfo::STACK;
      EXPECT_EQ(debug_scope.function_variable_info(),
                (std::pair{expected_info, var->index()}));
      EXPECT_TRUE(debug_scope.function_variable_name()->Equals(*var->name()));
    } else {
      EXPECT_FALSE(debug_scope.has_function_variable());
      EXPECT_EQ(debug_scope.function_variable_info(),
                (std::pair{VariableAllocationInfo::NONE, -1}));
      EXPECT_TRUE(debug_scope.function_variable_name().is_null());
    }
  } else {
    EXPECT_FALSE(debug_scope.is_arrow_scope());
    EXPECT_FALSE(debug_scope.has_this_declaration());
    EXPECT_FALSE(debug_scope.has_simple_parameters());
    EXPECT_FALSE(debug_scope.has_arguments());
    EXPECT_FALSE(debug_scope.has_function_variable());
    EXPECT_FALSE(debug_scope.sloppy_eval_can_extend_vars());
    EXPECT_EQ(debug_scope.receiver_info(),
              (std::pair{VariableAllocationInfo::NONE, -1}));
    EXPECT_EQ(debug_scope.arguments_info(),
              (std::pair{VariableAllocationInfo::NONE, -1}));
    EXPECT_EQ(debug_scope.function_variable_info(),
              (std::pair{VariableAllocationInfo::NONE, -1}));
    EXPECT_TRUE(debug_scope.function_variable_name().is_null());
  }

  int ast_var_count = static_cast<int>(
      std::distance(ast_scope->locals()->begin(), ast_scope->locals()->end()));
  EXPECT_EQ(ast_var_count, debug_scope.variable_count());

  int var_idx = 0;
  for (Variable* ast_var : *ast_scope->locals()) {
    DebugVariableInfo debug_var = debug_scope.variable(var_idx++);
    EXPECT_EQ(ast_var->location(), debug_var.location);
    EXPECT_EQ(ast_var->mode(), debug_var.mode);
    EXPECT_EQ(ast_var->index(), debug_var.index);
    EXPECT_EQ(ast_var->initializer_position(), debug_var.initializer_position);
    const AstRawString* raw = ast_var->raw_name();
    // Keep in sync with ScopeInfo::VariableIsSynthetic() in
    // src/objects/scope-info.cc.
    bool expected_synthetic =
        raw != nullptr &&
        (raw->IsEmpty() || raw->FirstCharacter() == '.' ||
         raw->IsPrivateName() || raw->IsOneByteEqualTo("this"));
    EXPECT_EQ(expected_synthetic, debug_var.is_synthetic);
    // Keep in sync with Variable::IsReceiver() in src/ast/variables.h.
    bool expected_receiver = ast_var->IsParameter() && ast_var->IsReceiver();
    EXPECT_EQ(expected_receiver, debug_var.is_receiver);
    if (raw != nullptr) {
      ASSERT_FALSE(debug_var.name.is_null());
      EXPECT_TRUE(debug_var.name->Equals(*ast_var->name()));
    } else {
      EXPECT_TRUE(debug_var.name.is_null());
    }
  }

  Scope* ast_child = ast_scope->inner_scope();
  auto debug_child = debug_scope.first_child();

  while (ast_child != nullptr) {
    ASSERT_TRUE(debug_child.has_value());
    EXPECT_TRUE(debug_child->parent().has_value());
    EXPECT_EQ(debug_child->parent()->scope_index(), debug_scope.scope_index());
    VerifyScopeTreeParity(ast_child, *debug_child);
    ast_child = ast_child->sibling();
    debug_child = debug_child->next_sibling();
  }
  EXPECT_FALSE(debug_child.has_value());
}

}  // namespace

TEST_F(DebugScopeInfoTest, EmptyScript) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope root = DebugScriptScope::FromIndex(info, 0);
  EXPECT_EQ(root.scope_index(), 0);
  EXPECT_EQ(root.start_position(), 0);
  EXPECT_EQ(root.end_position(), 0);
  EXPECT_EQ(root.scope_type(), ScopeType::SCRIPT_SCOPE);
  EXPECT_TRUE(root.is_script_scope());
  EXPECT_FALSE(root.is_function_scope());
  EXPECT_FALSE(root.is_block_scope());
  EXPECT_TRUE(root.is_declaration_scope());
  EXPECT_FALSE(root.parent().has_value());
  EXPECT_FALSE(root.first_child().has_value());
  EXPECT_FALSE(root.next_sibling().has_value());

  VerifyScopeTreeParity(parsed.script_scope(), root);
}

TEST_F(DebugScopeInfoTest, SingleScriptScope) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("let x = 1;");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope root = DebugScriptScope::FromIndex(info, 0);
  EXPECT_EQ(root.scope_index(), 0);
  EXPECT_EQ(root.start_position(), 0);
  EXPECT_EQ(root.end_position(), 10);
  EXPECT_EQ(root.scope_type(), ScopeType::SCRIPT_SCOPE);
  EXPECT_TRUE(root.is_script_scope());
  EXPECT_FALSE(root.is_function_scope());
  EXPECT_FALSE(root.is_block_scope());
  EXPECT_TRUE(root.is_declaration_scope());
  EXPECT_FALSE(root.parent().has_value());
  EXPECT_FALSE(root.first_child().has_value());
  EXPECT_FALSE(root.next_sibling().has_value());

  VerifyScopeTreeParity(parsed.script_scope(), root);
}

TEST_F(DebugScopeInfoTest, NestedBlockScopes) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("{ let a = 1; } { let b = 2; }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  // Script scope (0)
  DebugScriptScope s0 = DebugScriptScope::FromIndex(info, 0);
  EXPECT_EQ(s0.scope_index(), 0);
  EXPECT_EQ(s0.start_position(), 0);
  EXPECT_EQ(s0.end_position(), 29);
  EXPECT_EQ(s0.scope_type(), ScopeType::SCRIPT_SCOPE);
  EXPECT_TRUE(s0.is_script_scope());
  EXPECT_FALSE(s0.is_block_scope());
  EXPECT_FALSE(s0.parent().has_value());
  EXPECT_FALSE(s0.next_sibling().has_value());
  ASSERT_TRUE(s0.first_child().has_value());
  EXPECT_EQ(s0.first_child()->scope_index(), 1);

  // Block 2 scope (1)
  DebugScriptScope s1 = DebugScriptScope::FromIndex(info, 1);
  EXPECT_EQ(s1.scope_index(), 1);
  EXPECT_EQ(s1.start_position(), 15);
  EXPECT_EQ(s1.end_position(), 29);
  EXPECT_EQ(s1.scope_type(), ScopeType::BLOCK_SCOPE);
  EXPECT_TRUE(s1.is_block_scope());
  EXPECT_FALSE(s1.is_script_scope());
  ASSERT_TRUE(s1.parent().has_value());
  EXPECT_EQ(s1.parent()->scope_index(), 0);
  EXPECT_FALSE(s1.first_child().has_value());
  ASSERT_TRUE(s1.next_sibling().has_value());
  EXPECT_EQ(s1.next_sibling()->scope_index(), 2);

  // Block 1 scope (2)
  DebugScriptScope s2 = DebugScriptScope::FromIndex(info, 2);
  EXPECT_EQ(s2.scope_index(), 2);
  EXPECT_EQ(s2.start_position(), 0);
  EXPECT_EQ(s2.end_position(), 14);
  EXPECT_EQ(s2.scope_type(), ScopeType::BLOCK_SCOPE);
  EXPECT_TRUE(s2.is_block_scope());
  EXPECT_FALSE(s2.is_script_scope());
  ASSERT_TRUE(s2.parent().has_value());
  EXPECT_EQ(s2.parent()->scope_index(), 0);
  EXPECT_FALSE(s2.first_child().has_value());
  EXPECT_FALSE(s2.next_sibling().has_value());

  VerifyScopeTreeParity(parsed.script_scope(), s0);
}

TEST_F(DebugScopeInfoTest, NestedFunctionScopes) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("function foo() { { let x = 1; } }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  // Script scope (0)
  DebugScriptScope s0 = DebugScriptScope::FromIndex(info, 0);
  EXPECT_EQ(s0.scope_index(), 0);
  EXPECT_EQ(s0.start_position(), 0);
  EXPECT_EQ(s0.end_position(), 33);
  EXPECT_EQ(s0.scope_type(), ScopeType::SCRIPT_SCOPE);
  EXPECT_TRUE(s0.is_script_scope());
  EXPECT_FALSE(s0.parent().has_value());
  EXPECT_FALSE(s0.next_sibling().has_value());
  ASSERT_TRUE(s0.first_child().has_value());
  EXPECT_EQ(s0.first_child()->scope_index(), 1);

  // Function foo scope (1)
  DebugScriptScope s1 = DebugScriptScope::FromIndex(info, 1);
  EXPECT_EQ(s1.scope_index(), 1);
  EXPECT_EQ(s1.start_position(), 12);
  EXPECT_EQ(s1.end_position(), 33);
  EXPECT_EQ(s1.scope_type(), ScopeType::FUNCTION_SCOPE);
  EXPECT_TRUE(s1.is_function_scope());
  EXPECT_TRUE(s1.is_declaration_scope());
  ASSERT_TRUE(s1.parent().has_value());
  EXPECT_EQ(s1.parent()->scope_index(), 0);
  EXPECT_FALSE(s1.next_sibling().has_value());
  ASSERT_TRUE(s1.first_child().has_value());
  EXPECT_EQ(s1.first_child()->scope_index(), 2);

  // Inner block scope (2)
  DebugScriptScope s2 = DebugScriptScope::FromIndex(info, 2);
  EXPECT_EQ(s2.scope_index(), 2);
  EXPECT_EQ(s2.start_position(), 17);
  EXPECT_EQ(s2.end_position(), 31);
  EXPECT_EQ(s2.scope_type(), ScopeType::BLOCK_SCOPE);
  EXPECT_TRUE(s2.is_block_scope());
  EXPECT_FALSE(s2.is_declaration_scope());
  ASSERT_TRUE(s2.parent().has_value());
  EXPECT_EQ(s2.parent()->scope_index(), 1);
  EXPECT_FALSE(s2.first_child().has_value());
  EXPECT_FALSE(s2.next_sibling().has_value());

  VerifyScopeTreeParity(parsed.script_scope(), s0);
}

TEST_F(DebugScopeInfoTest, DeeplyNestedScopes) {
  HandleScope scope(isolate());
  // Scopes: 0 = Script, 1 = Function outer, 2 = Block 1, 3 = Block 2
  ParsedScript parsed = ParseAndSerialize(
      "function outer() { { let a = 1; { let nested = 42; } } }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  EXPECT_EQ(script.scope_type(), ScopeType::SCRIPT_SCOPE);
  EXPECT_FALSE(script.parent().has_value());

  auto func = script.first_child();
  ASSERT_TRUE(func.has_value());
  EXPECT_EQ(func->scope_index(), 1);
  EXPECT_EQ(func->scope_type(), ScopeType::FUNCTION_SCOPE);
  EXPECT_TRUE(func->parent().has_value());
  EXPECT_EQ(func->parent()->scope_index(), 0);

  auto block1 = func->first_child();
  ASSERT_TRUE(block1.has_value());
  EXPECT_EQ(block1->scope_index(), 2);
  EXPECT_EQ(block1->scope_type(), ScopeType::BLOCK_SCOPE);
  EXPECT_TRUE(block1->parent().has_value());
  EXPECT_EQ(block1->parent()->scope_index(), 1);

  auto block2 = block1->first_child();
  ASSERT_TRUE(block2.has_value());
  EXPECT_EQ(block2->scope_index(), 3);
  EXPECT_EQ(block2->scope_type(), ScopeType::BLOCK_SCOPE);
  EXPECT_TRUE(block2->parent().has_value());
  EXPECT_EQ(block2->parent()->scope_index(), 2);
  EXPECT_FALSE(block2->first_child().has_value());
  EXPECT_FALSE(block2->next_sibling().has_value());

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

TEST_F(DebugScopeInfoTest, MultipleFunctionsAndSiblings) {
  HandleScope scope(isolate());
  // Scopes: 0 = Script, 1 = Function f1, 2 = Function f2
  ParsedScript parsed = ParseAndSerialize(
      "function f1() { let a = 1; } function f2() { let b = 2; }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  EXPECT_EQ(script.scope_type(), ScopeType::SCRIPT_SCOPE);

  auto f1 = script.first_child();
  ASSERT_TRUE(f1.has_value());
  EXPECT_EQ(f1->scope_index(), 1);
  EXPECT_EQ(f1->scope_type(), ScopeType::FUNCTION_SCOPE);
  EXPECT_TRUE(f1->parent().has_value());
  EXPECT_EQ(f1->parent()->scope_index(), 0);

  auto f2 = f1->next_sibling();
  ASSERT_TRUE(f2.has_value());
  EXPECT_EQ(f2->scope_index(), 2);
  EXPECT_EQ(f2->scope_type(), ScopeType::FUNCTION_SCOPE);
  EXPECT_TRUE(f2->parent().has_value());
  EXPECT_EQ(f2->parent()->scope_index(), 0);
  EXPECT_FALSE(f2->next_sibling().has_value());

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

TEST_F(DebugScopeInfoTest, ArrowAndNormalFunctionScope) {
  HandleScope scope(isolate());
  ParsedScript parsed =
      ParseAndSerialize("let fn = (x) => { return x + 1; }; function g() {}");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(script.is_script_scope());
  EXPECT_FALSE(script.is_arrow_scope());
  EXPECT_FALSE(script.has_this_declaration());

  // Function g scope (1) - child scopes are prepended in AST
  auto normal = script.first_child();
  ASSERT_TRUE(normal.has_value());
  EXPECT_TRUE(normal->is_function_scope());
  EXPECT_FALSE(normal->is_arrow_scope());
  EXPECT_TRUE(normal->has_this_declaration());

  // Arrow function fn scope (2)
  auto arrow = normal->next_sibling();
  ASSERT_TRUE(arrow.has_value());
  EXPECT_TRUE(arrow->is_function_scope());
  EXPECT_TRUE(arrow->is_arrow_scope());
  EXPECT_FALSE(arrow->has_this_declaration());
  EXPECT_TRUE(arrow->has_simple_parameters());

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

TEST_F(DebugScopeInfoTest, StrictAndNonSimpleParams) {
  HandleScope scope(isolate());
  ParsedScript parsed =
      ParseAndSerialize("\"use strict\"; function f(a = 1) { eval(\"\"); }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  EXPECT_EQ(script.language_mode(), LanguageMode::kStrict);

  auto func = script.first_child();
  ASSERT_TRUE(func.has_value());
  EXPECT_EQ(func->language_mode(), LanguageMode::kStrict);
  EXPECT_FALSE(func->has_simple_parameters());
  EXPECT_FALSE(func->sloppy_eval_can_extend_vars());

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

TEST_F(DebugScopeInfoTest, ComplexScopeTypes) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize(
      "class C { m() {} }\n"
      "try { } catch (e) { }\n"
      "with ({}) { }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  VerifyScopeTreeParity(parsed.script_scope(), script);
}

TEST_F(DebugScopeInfoTest, NeedsContextAndUniqueId) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize(
      "function outer() {\n"
      "  let x = 1;\n"
      "  { let y = 2; return () => x + y; }\n"
      "}\n"
      "function noContext() { let a = 1; return a; }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  VerifyScopeTreeParity(parsed.script_scope(), script);

  // Function noContext (1)
  auto no_ctx = script.first_child();
  ASSERT_TRUE(no_ctx.has_value());
  EXPECT_FALSE(no_ctx->needs_context());
  EXPECT_EQ(no_ctx->unique_id_in_script(), -3);

  // Function outer (2)
  auto outer = no_ctx->next_sibling();
  ASSERT_TRUE(outer.has_value());
  EXPECT_TRUE(outer->needs_context());
  EXPECT_GE(outer->unique_id_in_script(), 0);

  // Inner block scope (3) inside outer
  auto block = outer->first_child();
  ASSERT_TRUE(block.has_value());
  EXPECT_TRUE(block->needs_context());
  EXPECT_GE(block->unique_id_in_script(), 0);
  EXPECT_NE(block->unique_id_in_script(), outer->unique_id_in_script());
}

TEST_F(DebugScopeInfoTest, ReceiverAllocationInfo) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize(
      "function withThisContext() { return () => this; }\n"
      "function normalFunc() { return 1; }\n"
      "const arrow = () => 42;");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  VerifyScopeTreeParity(parsed.script_scope(), script);

  // Arrow function (1)
  auto arrow_scope = script.first_child();
  ASSERT_TRUE(arrow_scope.has_value());
  EXPECT_FALSE(arrow_scope->has_this_declaration());
  EXPECT_EQ(arrow_scope->receiver_info(),
            (std::pair{VariableAllocationInfo::NONE, -1}));

  // normalFunc (2)
  auto normal_scope = arrow_scope->next_sibling();
  ASSERT_TRUE(normal_scope.has_value());
  EXPECT_TRUE(normal_scope->has_this_declaration());
  EXPECT_EQ(normal_scope->receiver_info(),
            (std::pair{VariableAllocationInfo::STACK, -1}));

  // withThisContext (3)
  auto with_this = normal_scope->next_sibling();
  ASSERT_TRUE(with_this.has_value());
  EXPECT_TRUE(with_this->has_this_declaration());
  auto [this_loc, this_idx] = with_this->receiver_info();
  EXPECT_EQ(this_loc, VariableAllocationInfo::CONTEXT);
  EXPECT_GE(this_idx, 0);
}

TEST_F(DebugScopeInfoTest, ArgumentsAllocationInfo) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize(
      "function withArgsContext() { return () => arguments[0]; }\n"
      "function withArgsStack() { return arguments[0]; }\n"
      "function noArgs() { return 1; }\n"
      "const arrow = () => 42;");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  VerifyScopeTreeParity(parsed.script_scope(), script);

  // Arrow function (1)
  auto arrow_scope = script.first_child();
  ASSERT_TRUE(arrow_scope.has_value());
  EXPECT_FALSE(arrow_scope->has_arguments());
  EXPECT_EQ(arrow_scope->arguments_info(),
            (std::pair{VariableAllocationInfo::NONE, -1}));

  // noArgs (2)
  auto no_args = arrow_scope->next_sibling();
  ASSERT_TRUE(no_args.has_value());
  EXPECT_FALSE(no_args->has_arguments());
  EXPECT_EQ(no_args->arguments_info(),
            (std::pair{VariableAllocationInfo::NONE, -1}));

  // withArgsStack (3)
  auto with_args_stack = no_args->next_sibling();
  ASSERT_TRUE(with_args_stack.has_value());
  EXPECT_TRUE(with_args_stack->has_arguments());
  auto [stack_loc, stack_idx] = with_args_stack->arguments_info();
  EXPECT_EQ(stack_loc, VariableAllocationInfo::STACK);
  EXPECT_GE(stack_idx, 0);

  // withArgsContext (4)
  auto with_args_context = with_args_stack->next_sibling();
  ASSERT_TRUE(with_args_context.has_value());
  EXPECT_TRUE(with_args_context->has_arguments());
  auto [ctx_loc, ctx_idx] = with_args_context->arguments_info();
  EXPECT_EQ(ctx_loc, VariableAllocationInfo::CONTEXT);
  EXPECT_GE(ctx_idx, 0);
}

TEST_F(DebugScopeInfoTest, FunctionVariableInfo) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize(
      "const withFuncContext = function f1() { return () => f1; };\n"
      "const withFuncStack = function f2() { return f2; };\n"
      "const withFuncDup = function f1() { return f1; };\n"
      "const unusedName = function f3() { return 1; };\n"
      "function normalDecl() { return 1; }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;
  EXPECT_TRUE(IsFixedArray(info->string_table()));
  EXPECT_EQ(info->string_table()->length().value(), 11u);

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  VerifyScopeTreeParity(parsed.script_scope(), script);

  // normalDecl (1)
  auto normal_decl = script.first_child();
  ASSERT_TRUE(normal_decl.has_value());
  EXPECT_FALSE(normal_decl->has_function_variable());
  EXPECT_EQ(normal_decl->function_variable_info(),
            (std::pair{VariableAllocationInfo::NONE, -1}));
  EXPECT_TRUE(normal_decl->function_variable_name().is_null());

  // unusedName (2)
  auto unused_name = normal_decl->next_sibling();
  ASSERT_TRUE(unused_name.has_value());
  EXPECT_FALSE(unused_name->has_function_variable());
  EXPECT_EQ(unused_name->function_variable_info(),
            (std::pair{VariableAllocationInfo::NONE, -1}));
  EXPECT_TRUE(unused_name->function_variable_name().is_null());

  // withFuncDup (3)
  auto with_func_dup = unused_name->next_sibling();
  ASSERT_TRUE(with_func_dup.has_value());
  EXPECT_TRUE(with_func_dup->has_function_variable());
  auto [dup_loc, dup_idx] = with_func_dup->function_variable_info();
  EXPECT_EQ(dup_loc, VariableAllocationInfo::STACK);
  EXPECT_GE(dup_idx, 0);
  EXPECT_TRUE(with_func_dup->function_variable_name()->Equals(
      *isolate()->factory()->NewStringFromAsciiChecked("f1")));

  // withFuncStack (4)
  auto with_func_stack = with_func_dup->next_sibling();
  ASSERT_TRUE(with_func_stack.has_value());
  EXPECT_TRUE(with_func_stack->has_function_variable());
  auto [stack_loc, stack_idx] = with_func_stack->function_variable_info();
  EXPECT_EQ(stack_loc, VariableAllocationInfo::STACK);
  EXPECT_GE(stack_idx, 0);
  EXPECT_TRUE(with_func_stack->function_variable_name()->Equals(
      *isolate()->factory()->NewStringFromAsciiChecked("f2")));

  // withFuncContext (5)
  auto with_func_context = with_func_stack->next_sibling();
  ASSERT_TRUE(with_func_context.has_value());
  EXPECT_TRUE(with_func_context->has_function_variable());
  auto [ctx_loc, ctx_idx] = with_func_context->function_variable_info();
  EXPECT_EQ(ctx_loc, VariableAllocationInfo::CONTEXT);
  EXPECT_GE(ctx_idx, 0);
  EXPECT_TRUE(with_func_context->function_variable_name()->Equals(
      *isolate()->factory()->NewStringFromAsciiChecked("f1")));
  EXPECT_EQ(with_func_context->function_variable_name(),
            with_func_dup->function_variable_name());
}

TEST_F(DebugScopeInfoTest, ScopeVariables) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize(
      "let x = 1;\n"
      "const y = 2;\n"
      "var z = 3;\n"
      "function foo(param1, param2) {\n"
      "  let inner = () => x + param1;\n"
      "  { let blockVar = 4; }\n"
      "}");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;
  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  VerifyScopeTreeParity(parsed.script_scope(), script);

  EXPECT_GE(script.variable_count(), 4);
  bool found_x = false, found_y = false, found_z = false, found_foo = false;
  for (int i = 0; i < script.variable_count(); ++i) {
    DebugVariableInfo var = script.variable(i);
    if (!var.name.is_null()) {
      if (var.name->Equals(
              *isolate()->factory()->NewStringFromAsciiChecked("x"))) {
        found_x = true;
        EXPECT_EQ(var.mode, VariableMode::kLet);
      } else if (var.name->Equals(
                     *isolate()->factory()->NewStringFromAsciiChecked("y"))) {
        found_y = true;
        EXPECT_EQ(var.mode, VariableMode::kConst);
      } else if (var.name->Equals(
                     *isolate()->factory()->NewStringFromAsciiChecked("z"))) {
        found_z = true;
        EXPECT_EQ(var.mode, VariableMode::kVar);
      } else if (var.name->Equals(
                     *isolate()->factory()->NewStringFromAsciiChecked("foo"))) {
        found_foo = true;
      }
    }
  }
  EXPECT_TRUE(found_x);
  EXPECT_TRUE(found_y);
  EXPECT_TRUE(found_z);
  EXPECT_TRUE(found_foo);

  // foo function scope
  auto foo_scope = script.first_child();
  ASSERT_TRUE(foo_scope.has_value());
  EXPECT_GE(foo_scope->variable_count(), 3);
  bool found_param1 = false, found_param2 = false, found_inner = false;
  for (int i = 0; i < foo_scope->variable_count(); ++i) {
    DebugVariableInfo var = foo_scope->variable(i);
    if (!var.name.is_null()) {
      if (var.name->Equals(
              *isolate()->factory()->NewStringFromAsciiChecked("param1"))) {
        found_param1 = true;
        EXPECT_EQ(var.location, VariableLocation::CONTEXT);
      } else if (var.name->Equals(
                     *isolate()->factory()->NewStringFromAsciiChecked(
                         "param2"))) {
        found_param2 = true;
        EXPECT_EQ(var.location, VariableLocation::PARAMETER);
      } else if (var.name->Equals(
                     *isolate()->factory()->NewStringFromAsciiChecked(
                         "inner"))) {
        found_inner = true;
        EXPECT_EQ(var.mode, VariableMode::kLet);
      }
    }
  }
  EXPECT_TRUE(found_param1);
  EXPECT_TRUE(found_param2);
  EXPECT_TRUE(found_inner);
}

TEST_F(DebugScopeInfoTest, SyntheticAndReceiverVariables) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize(
      "let normalVar = 1;\n"
      "function normalFunc(param) { return this; }\n"
      "function withThisContext() { return () => this; }\n"
      "const arrowFunc = () => 42;\n"
      "function* gen() { yield 1; }\n"
      "class C { #priv() {} }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;
  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  VerifyScopeTreeParity(parsed.script_scope(), script);

  // Verify normal script variable is neither synthetic nor receiver.
  bool found_normal_var = false;
  for (int i = 0; i < script.variable_count(); ++i) {
    DebugVariableInfo var = script.variable(i);
    if (!var.name.is_null() &&
        var.name->Equals(
            *isolate()->factory()->NewStringFromAsciiChecked("normalVar"))) {
      found_normal_var = true;
      EXPECT_FALSE(var.is_synthetic);
      EXPECT_FALSE(var.is_receiver);
    }
  }
  EXPECT_TRUE(found_normal_var);

  bool found_normal_this = false;
  bool found_param = false;
  bool found_context_this = false;
  bool arrow_has_receiver = false;
  bool found_generator_obj = false;
  bool found_private_member = false;

  auto inspect_scope = [&](auto& self,
                           const DebugScriptScope& current) -> void {
    for (int i = 0; i < current.variable_count(); ++i) {
      DebugVariableInfo var = current.variable(i);
      if (var.name.is_null()) continue;

      if (var.name->Equals(*isolate()->factory()->this_string())) {
        EXPECT_TRUE(var.is_synthetic);
        if (var.is_receiver) {
          EXPECT_EQ(var.location, VariableLocation::PARAMETER);
          EXPECT_EQ(var.index, -1);
          found_normal_this = true;
        } else if (var.location == VariableLocation::CONTEXT) {
          EXPECT_GE(var.index, 0);
          found_context_this = true;
        }
      }

      if (var.name->Equals(
              *isolate()->factory()->NewStringFromAsciiChecked("param"))) {
        found_param = true;
        EXPECT_FALSE(var.is_synthetic);
        EXPECT_FALSE(var.is_receiver);
      }

      if (var.name->Equals(*isolate()->factory()->NewStringFromAsciiChecked(
              ".generator_object"))) {
        found_generator_obj = true;
        EXPECT_TRUE(var.is_synthetic);
        EXPECT_FALSE(var.is_receiver);
      }

      if (var.name->length() > 0 &&
          (var.name->Get(0) == '#' || var.name->Get(0) == '.')) {
        EXPECT_TRUE(var.is_synthetic);
        EXPECT_FALSE(var.is_receiver);
        if (var.name->Get(0) == '#') {
          found_private_member = true;
        }
      }
    }

    if (current.is_arrow_scope()) {
      for (int i = 0; i < current.variable_count(); ++i) {
        if (current.variable(i).is_receiver) {
          arrow_has_receiver = true;
        }
      }
    }

    for (auto child = current.first_child(); child.has_value();
         child = child->next_sibling()) {
      self(self, *child);
    }
  };

  inspect_scope(inspect_scope, script);

  EXPECT_TRUE(found_normal_this);
  EXPECT_TRUE(found_param);
  EXPECT_TRUE(found_context_this);
  EXPECT_FALSE(arrow_has_receiver);
  EXPECT_TRUE(found_generator_obj);
  EXPECT_TRUE(found_private_member);
}

TEST_F(DebugScopeInfoTest, DebugScriptScopeInfoSideTable) {
  HandleScope scope(isolate());
  ParsedScript parsed1 = ParseAndSerialize("let a = 1;");
  ParsedScript parsed2 = ParseAndSerialize("let b = 2;");

  Handle<String> src1 =
      isolate()->factory()->NewStringFromAsciiChecked("let a = 1;");
  Handle<Script> script1 = isolate()->factory()->NewScript(src1);

  Handle<String> src2 =
      isolate()->factory()->NewStringFromAsciiChecked("let b = 2;");
  Handle<Script> script2 = isolate()->factory()->NewScript(src2);

  Debug* debug = isolate()->debug();

  // Initially, no scope info in the debug side table.
  EXPECT_TRUE(debug->GetScriptScopeInfo(script1).is_null());
  EXPECT_TRUE(debug->GetScriptScopeInfo(script2).is_null());

  // Set scope infos.
  debug->SetScriptScopeInfo(script1, parsed1.scope_info);
  debug->SetScriptScopeInfo(script2, parsed2.scope_info);

  // Retrieve and verify cached scope infos.
  auto cached1 = debug->GetScriptScopeInfo(script1);
  ASSERT_FALSE(cached1.is_null());
  EXPECT_EQ(*cached1, *parsed1.scope_info);

  auto cached2 = debug->GetScriptScopeInfo(script2);
  ASSERT_FALSE(cached2.is_null());
  EXPECT_EQ(*cached2, *parsed2.scope_info);

  // Update existing entry.
  debug->SetScriptScopeInfo(script1, parsed2.scope_info);
  auto updated1 = debug->GetScriptScopeInfo(script1);
  ASSERT_FALSE(updated1.is_null());
  EXPECT_EQ(*updated1, *parsed2.scope_info);

  // ClearScriptScopeInfos clears all entries.
  debug->ClearScriptScopeInfos();
  EXPECT_TRUE(debug->GetScriptScopeInfo(script1).is_null());
  EXPECT_TRUE(debug->GetScriptScopeInfo(script2).is_null());
}

TEST_F(DebugScopeInfoTest, SideTableClearedOnUnload) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("let x = 42;");
  Handle<String> src =
      isolate()->factory()->NewStringFromAsciiChecked("let x = 42;");
  Handle<Script> script = isolate()->factory()->NewScript(src);

  Debug* debug = isolate()->debug();
  debug->SetScriptScopeInfo(script, parsed.scope_info);
  EXPECT_FALSE(debug->GetScriptScopeInfo(script).is_null());

  struct EmptyDelegate : public v8::debug::DebugDelegate {
  } delegate;
  v8::debug::SetDebugDelegate(v8_isolate(), &delegate);
  v8::debug::SetDebugDelegate(v8_isolate(), nullptr);

  EXPECT_TRUE(debug->GetScriptScopeInfo(script).is_null());
}

}  // namespace internal
}  // namespace v8
