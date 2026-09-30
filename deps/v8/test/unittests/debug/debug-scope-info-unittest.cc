// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-scope-info.h"

#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "include/v8-function.h"
#include "src/api/api-inl.h"
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

class DebugScopeInfoTest : public TestWithNativeContext {
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

  int ast_var_count = base::checked_cast<int>(
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

// ParserBase::NewVarblockScope() creates a DeclarationScope with BLOCK_SCOPE
// type for a function body with non-simple parameters, so is_declaration_scope
// cannot be derived from the scope type. The body has to declare something,
// otherwise Scope::FinalizeBlockScope() drops the scope again.
TEST_F(DebugScopeInfoTest, VarblockScopeIsDeclarationScope) {
  HandleScope scope(isolate());
  ParsedScript parsed =
      ParseAndSerialize("function f(a = 1) { let x = 2; return a + x; }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  auto func = script.first_child();
  ASSERT_TRUE(func.has_value());
  ASSERT_TRUE(func->is_function_scope());

  auto varblock = func->first_child();
  ASSERT_TRUE(varblock.has_value());
  EXPECT_EQ(varblock->scope_type(), ScopeType::BLOCK_SCOPE);
  EXPECT_TRUE(varblock->is_block_scope());
  EXPECT_TRUE(varblock->is_declaration_scope());

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

// The other NewVarblockScope() call site: class static initializer blocks.
TEST_F(DebugScopeInfoTest, ClassStaticBlockIsDeclarationScope) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("class C { static { let x = 1; } }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  bool found_declaration_block_scope = false;
  auto walk = [&](auto& self, const DebugScriptScope& current) -> void {
    if (current.scope_type() == ScopeType::BLOCK_SCOPE &&
        current.is_declaration_scope()) {
      found_declaration_block_scope = true;
    }
    for (auto child = current.first_child(); child.has_value();
         child = child->next_sibling()) {
      self(self, *child);
    }
  };
  walk(walk, script);
  EXPECT_TRUE(found_declaration_block_scope);

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

// A varblock scope can also survive FinalizeBlockScope() without declaring
// anything, via sloppy_eval_can_extend_vars().
TEST_F(DebugScopeInfoTest, VarblockScopeWithSloppyEval) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("function f(a = 1) { eval(''); }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  auto func = script.first_child();
  ASSERT_TRUE(func.has_value());

  auto varblock = func->first_child();
  ASSERT_TRUE(varblock.has_value());
  EXPECT_EQ(varblock->scope_type(), ScopeType::BLOCK_SCOPE);
  EXPECT_TRUE(varblock->is_declaration_scope());
  EXPECT_TRUE(varblock->sloppy_eval_can_extend_vars());

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

namespace {

// Builds a source string by repeating `pattern` `count` times, substituting the
// current index for each "%d".
std::string Repeat(const char* pattern, int count) {
  std::string result;
  for (int i = 0; i < count; ++i) {
    for (const char* p = pattern; *p != '\0'; ++p) {
      if (p[0] == '%' && p[1] == 'd') {
        result += std::to_string(i);
        ++p;
      } else {
        result += *p;
      }
    }
  }
  return result;
}

// How many declarations each test generates. Chosen to be large enough that
// the resulting indices and counts need the full 32-bit fields.
constexpr int kManyContextVariables = 9000;
constexpr int kManyParameters = 40000;
constexpr int kManyLocals = 70000;

}  // namespace

// A sloppy direct eval forces context allocation of every variable, and
// DeclarationScope::AllocateLocals() allocates the function variable last, so
// it ends up with the highest context slot index in the scope.
TEST_F(DebugScopeInfoTest, FunctionVariableWithLargeContextIndex) {
  HandleScope scope(isolate());
  std::string source = "var g = (function foo() {\n";
  source += Repeat("  var v%d = %d;\n", kManyContextVariables);
  source += "  eval(\"\");\n  return foo;\n});";
  ParsedScript parsed = ParseAndSerialize(source.c_str());
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  auto func = script.first_child();
  ASSERT_TRUE(func.has_value());
  ASSERT_TRUE(func->has_function_variable());

  auto [alloc_info, index] = func->function_variable_info();
  EXPECT_EQ(alloc_info, VariableAllocationInfo::CONTEXT);
  EXPECT_GT(index, kManyContextVariables);

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

// Same, for the 'arguments' variable, which is allocated after all parameters.
TEST_F(DebugScopeInfoTest, ArgumentsWithLargeContextIndex) {
  HandleScope scope(isolate());
  std::string source = "function g(";
  source += Repeat("q%d,", kManyContextVariables);
  source += "qLast) { eval(\"\"); return arguments; }";
  ParsedScript parsed = ParseAndSerialize(source.c_str());
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  auto func = script.first_child();
  ASSERT_TRUE(func.has_value());
  ASSERT_TRUE(func->has_arguments());

  auto [alloc_info, index] = func->arguments_info();
  EXPECT_EQ(alloc_info, VariableAllocationInfo::CONTEXT);
  EXPECT_GT(index, kManyContextVariables);

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

// Parameters are part of Scope::locals() and their index goes up to
// Code::kMaxArguments - 1.
TEST_F(DebugScopeInfoTest, VariableWithLargeParameterSlotIndex) {
  HandleScope scope(isolate());
  std::string source = "function f(";
  source += Repeat("p%d,", kManyParameters);
  source += "pLast) { return p0; }";
  ParsedScript parsed = ParseAndSerialize(source.c_str());
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  auto func = script.first_child();
  ASSERT_TRUE(func.has_value());

  int max_index = -1;
  for (int i = 0; i < func->variable_count(); ++i) {
    DebugVariableInfo var = func->variable(i);
    if (var.location == VariableLocation::PARAMETER) {
      if (var.index > max_index) max_index = var.index;
    }
  }
  EXPECT_GE(max_index, kManyParameters);

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

// A single scope can declare a very large number of locals.
TEST_F(DebugScopeInfoTest, ScopeWithVeryManyVariables) {
  HandleScope scope(isolate());
  std::string source = "function h() {\n";
  source += Repeat("var w%d;\n", kManyLocals);
  source += "return w0;\n}";
  ParsedScript parsed = ParseAndSerialize(source.c_str());
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  auto func = script.first_child();
  ASSERT_TRUE(func.has_value());
  EXPECT_GE(func->variable_count(), kManyLocals);

  VerifyScopeTreeParity(parsed.script_scope(), script);
}

// Same declarations, but a sloppy direct eval forces all of them into the
// context. The highest context slot index then lands beyond 2^16, which is
// what requires slot_index to be wider than 16 bits. (In the test above the
// locals are unused and stay unallocated at index -1.)
TEST_F(DebugScopeInfoTest, ContextSlotIndexAboveUint16Max) {
  HandleScope scope(isolate());
  std::string source = "function h() {\n";
  source += Repeat("var w%d;\n", kManyLocals);
  source += "eval(\"\");\nreturn w0;\n}";
  ParsedScript parsed = ParseAndSerialize(source.c_str());
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script = DebugScriptScope::FromIndex(info, 0);
  auto func = script.first_child();
  ASSERT_TRUE(func.has_value());

  int max_index = -1;
  for (int i = 0; i < func->variable_count(); ++i) {
    DebugVariableInfo var = func->variable(i);
    if (var.location == VariableLocation::CONTEXT) {
      if (var.index > max_index) max_index = var.index;
    }
  }
  EXPECT_GT(max_index, 0xFFFF);

  VerifyScopeTreeParity(parsed.script_scope(), script);
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

TEST_F(DebugScopeInfoTest, EnsureDebugScriptScopeInfo) {
  HandleScope scope(isolate());
  Handle<JSFunction> foo = RunJS<JSFunction>(
      "function foo() { let a = 1; function bar() { return a; } return bar; } "
      "foo;");
  DirectHandle<Script> script(Cast<Script>(foo->shared()->script()), isolate());

  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(script).is_null());
  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), script);
  ASSERT_FALSE(info.is_null());

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);

  EXPECT_EQ(*info, *EnsureDebugScriptScopeInfo(isolate(), script));
  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(script).is_null());
}

TEST_F(DebugScopeInfoTest, EnsureDebugScriptScopeInfo_DirectEval) {
  HandleScope scope(isolate());
  Handle<JSFunction> inner = RunJS<JSFunction>(
      "function outer() {\n"
      "  'use strict';\n"
      "  let outer_lexical = 42;\n"
      "  return eval('function inner() { return outer_lexical; } inner;');\n"
      "}\n"
      "outer();");
  DirectHandle<Script> eval_script(Cast<Script>(inner->shared()->script()),
                                   isolate());
  EXPECT_TRUE(eval_script->is_eval());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(eval_script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), eval_script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kStrict);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(eval_script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest, EnsureDebugScriptScopeInfo_IndirectEval) {
  HandleScope scope(isolate());
  Handle<JSFunction> indirect_fn = RunJS<JSFunction>(
      "function outer() {\n"
      "  'use strict';\n"
      "  return (0, eval)('function indirectInner() { return 1; } "
      "indirectInner;');\n"
      "}\n"
      "outer();");
  DirectHandle<Script> eval_script(
      Cast<Script>(indirect_fn->shared()->script()), isolate());
  EXPECT_TRUE(eval_script->is_eval());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(eval_script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), eval_script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kSloppy);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(eval_script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest,
       EnsureDebugScriptScopeInfo_DirectEval_StrictCallerNoContext) {
  HandleScope scope(isolate());
  Handle<JSFunction> inner = RunJS<JSFunction>(
      "'use strict';\n"
      "eval('function inner() { return 1; } inner;');");
  DirectHandle<Script> eval_script(Cast<Script>(inner->shared()->script()),
                                   isolate());
  EXPECT_TRUE(eval_script->is_eval());
  EXPECT_FALSE(eval_script->has_eval_from_scope_info());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(eval_script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), eval_script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kStrict);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(eval_script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest,
       EnsureDebugScriptScopeInfo_DirectEval_StrictFunctionNoContext) {
  HandleScope scope(isolate());
  Handle<JSFunction> inner = RunJS<JSFunction>(
      "function outer() {\n"
      "  'use strict';\n"
      "  return eval('function inner() { return 1; } inner;');\n"
      "}\n"
      "outer();");
  DirectHandle<Script> eval_script(Cast<Script>(inner->shared()->script()),
                                   isolate());
  EXPECT_TRUE(eval_script->is_eval());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(eval_script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), eval_script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kStrict);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(eval_script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest,
       EnsureDebugScriptScopeInfo_DirectEval_SloppyCallerSloppyEval) {
  HandleScope scope(isolate());
  Handle<JSFunction> inner = RunJS<JSFunction>(
      "function outer() {\n"
      "  var x = 1;\n"
      "  return eval('function inner() { return x; } inner;');\n"
      "}\n"
      "outer();");
  DirectHandle<Script> eval_script(Cast<Script>(inner->shared()->script()),
                                   isolate());
  EXPECT_TRUE(eval_script->is_eval());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(eval_script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), eval_script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kSloppy);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(eval_script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest,
       EnsureDebugScriptScopeInfo_DirectEval_SloppyCallerStrictEval) {
  HandleScope scope(isolate());
  Handle<JSFunction> inner = RunJS<JSFunction>(
      "function outer() {\n"
      "  var x = 1;\n"
      "  return eval('\"use strict\"; function inner() { return x; } "
      "inner;');\n"
      "}\n"
      "outer();");
  DirectHandle<Script> eval_script(Cast<Script>(inner->shared()->script()),
                                   isolate());
  EXPECT_TRUE(eval_script->is_eval());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(eval_script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), eval_script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kStrict);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(eval_script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest, EnsureDebugScriptScopeInfo_IndirectEval_StrictBody) {
  HandleScope scope(isolate());
  Handle<JSFunction> indirect_fn = RunJS<JSFunction>(
      "(0, eval)('\"use strict\"; function indirectInner() { return 1; } "
      "indirectInner;');");
  DirectHandle<Script> eval_script(
      Cast<Script>(indirect_fn->shared()->script()), isolate());
  EXPECT_TRUE(eval_script->is_eval());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(eval_script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), eval_script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kStrict);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(eval_script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest, EnsureDebugScriptScopeInfo_DirectEval_PostGC) {
  HandleScope scope(isolate());
  Handle<JSFunction> inner = RunJS<JSFunction>(
      "function outer() {\n"
      "  'use strict';\n"
      "  let outer_lexical = 42;\n"
      "  return eval('function inner() { return outer_lexical; } inner;');\n"
      "}\n"
      "outer();");
  DirectHandle<Script> eval_script(Cast<Script>(inner->shared()->script()),
                                   isolate());
  EXPECT_TRUE(eval_script->is_eval());

  isolate()->debug()->ClearScriptScopeInfos();
  isolate()->heap()->CollectAllGarbage(i::GCFlag::kNoFlags,
                                       i::GarbageCollectionReason::kTesting);

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), eval_script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kStrict);
}

TEST_F(DebugScopeInfoTest, EnsureDebugScriptScopeInfo_WrappedScript) {
  v8::HandleScope scope(v8_isolate());
  v8::ScriptCompiler::Source script_source(
      NewString("let wrapped_x = 10; return wrapped_x;"));
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunction(context(), &script_source)
          .ToLocalChecked();
  Handle<JSFunction> function = Cast<JSFunction>(Utils::OpenHandle(*fun));
  DirectHandle<Script> script(Cast<Script>(function->shared()->script()),
                              isolate());
  EXPECT_TRUE(script->is_wrapped());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kSloppy);

  auto inner_scope = root_scope.first_child();
  ASSERT_TRUE(inner_scope.has_value());
  EXPECT_TRUE(inner_scope->is_function_scope());
  EXPECT_EQ(inner_scope->language_mode(), LanguageMode::kSloppy);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest,
       EnsureDebugScriptScopeInfo_WrappedScript_StrictMode) {
  v8::HandleScope scope(v8_isolate());
  v8::ScriptCompiler::Source script_source(
      NewString("'use strict'; let wrapped_x = 10; return wrapped_x;"));
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunction(context(), &script_source)
          .ToLocalChecked();
  Handle<JSFunction> function = Cast<JSFunction>(Utils::OpenHandle(*fun));
  DirectHandle<Script> script(Cast<Script>(function->shared()->script()),
                              isolate());
  EXPECT_TRUE(script->is_wrapped());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), script);
  ASSERT_FALSE(info.is_null());

  // The outer wrapper declaration scope (eval scope) is always sloppy mode.
  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kSloppy);

  // The inner wrapped function inherits the 'use strict' directive.
  auto inner_scope = root_scope.first_child();
  ASSERT_TRUE(inner_scope.has_value());
  EXPECT_TRUE(inner_scope->is_function_scope());
  EXPECT_EQ(inner_scope->language_mode(), LanguageMode::kStrict);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest,
       EnsureDebugScriptScopeInfo_WrappedScript_ContextExtension) {
  v8::HandleScope scope(v8_isolate());
  RunJS("var ext_obj = {x: 42};");
  v8::Local<v8::Object> ext[1];
  ext[0] =
      v8::Local<v8::Object>::Cast(context()
                                      ->Global()
                                      ->Get(context(), NewString("ext_obj"))
                                      .ToLocalChecked());
  v8::ScriptCompiler::Source script_source(
      NewString("let local_y = 100; return x + local_y;"));
  v8::Local<v8::Function> fun =
      v8::ScriptCompiler::CompileFunction(context(), &script_source, 0, nullptr,
                                          1, ext)
          .ToLocalChecked();
  Handle<JSFunction> function = Cast<JSFunction>(Utils::OpenHandle(*fun));
  DirectHandle<Script> script(Cast<Script>(function->shared()->script()),
                              isolate());
  EXPECT_TRUE(script->is_wrapped());

  DirectHandle<Context> fn_context(function->context(), isolate());
  EXPECT_FALSE(IsNativeContext(*fn_context));

  // Verify runtime execution resolves `x` from the context extension.
  v8::Local<v8::Value> call_result =
      fun->Call(context(), context()->Global(), 0, nullptr).ToLocalChecked();
  EXPECT_EQ(142, call_result->Int32Value(context()).ToChecked());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(script).is_null());

  // Reparsing does not require the runtime context extension; it faithfully
  // extracts internal AST scope topology and local variables without it.
  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kSloppy);

  auto inner_scope = root_scope.first_child();
  ASSERT_TRUE(inner_scope.has_value());
  EXPECT_TRUE(inner_scope->is_function_scope());
  EXPECT_EQ(inner_scope->language_mode(), LanguageMode::kSloppy);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest, EnsureDebugScriptScopeInfo_CacheMissDefaultArgs) {
  HandleScope scope(isolate());
  Handle<JSFunction> foo = RunJS<JSFunction>(
      "function foo() { let a = 1; function bar() { return a; } return bar; } "
      "foo;");
  DirectHandle<Script> script(Cast<Script>(foo->shared()->script()), isolate());

  // Verify on regular script without any handles.
  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), script);
  ASSERT_FALSE(info.is_null());

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);

  // Also verify cache miss with default args on direct eval script.
  Handle<JSFunction> inner = RunJS<JSFunction>(
      "function outer() {\n"
      "  'use strict';\n"
      "  let outer_lexical = 42;\n"
      "  return eval('function inner() { return outer_lexical; } inner;');\n"
      "}\n"
      "outer();");
  DirectHandle<Script> eval_script(Cast<Script>(inner->shared()->script()),
                                   isolate());
  EXPECT_TRUE(eval_script->is_eval());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(eval_script).is_null());

  Handle<DebugScriptScopeInfo> eval_info =
      EnsureDebugScriptScopeInfo(isolate(), eval_script);
  ASSERT_FALSE(eval_info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(eval_info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kStrict);
}

TEST_F(DebugScopeInfoTest, EnsureDebugScriptScopeInfo_FunctionConstructor) {
  HandleScope scope(isolate());
  Handle<JSFunction> fun =
      RunJS<JSFunction>("new Function('a', 'let b = 2; return a + b;');");
  DirectHandle<Script> script(Cast<Script>(fun->shared()->script()), isolate());
  EXPECT_EQ(script->compilation_kind(),
            Script::CompilationKind::kFunctionConstructor);

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_eval_scope());

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

TEST_F(DebugScopeInfoTest, EnsureDebugScriptScopeInfo_Module) {
  v8::HandleScope scope(v8_isolate());
  v8::ScriptOrigin origin(NewString("module.js"), 0, 0, false, -1,
                          v8::Local<v8::Value>(), false, false,
                          true /* is_module */);
  v8::ScriptCompiler::Source script_source(
      NewString("let module_x = 42;\n"
                "export function getX() { return module_x; }"),
      origin);
  v8::Local<v8::Module> module =
      v8::ScriptCompiler::CompileModule(v8_isolate(), &script_source)
          .ToLocalChecked();
  DirectHandle<SharedFunctionInfo> sfi =
      Utils::OpenHandle(*module->GetUnboundModuleScript());
  DirectHandle<Script> script(Cast<Script>(sfi->script()), isolate());
  EXPECT_TRUE(script->origin_options().IsModule());

  isolate()->debug()->ClearScriptScopeInfos();
  EXPECT_TRUE(isolate()->debug()->GetScriptScopeInfo(script).is_null());

  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root_scope = DebugScriptScope::FromIndex(info, 0);
  EXPECT_TRUE(root_scope.is_module_scope());
  EXPECT_EQ(root_scope.language_mode(), LanguageMode::kStrict);

  auto inner_scope = root_scope.first_child();
  ASSERT_TRUE(inner_scope.has_value());
  EXPECT_TRUE(inner_scope->is_function_scope());
  EXPECT_EQ(inner_scope->language_mode(), LanguageMode::kStrict);

  auto cached_info = isolate()->debug()->GetScriptScopeInfo(script);
  ASSERT_FALSE(cached_info.is_null());
  EXPECT_EQ(*info, *cached_info);
}

namespace {

// Collects the AST scopes rooted at `scope` in the same DFS pre-order that
// SerializeDebugScriptScopeInfo() uses, so that an index into the result
// matches the scope index in the serialized DebugScriptScopeInfo.
void CollectScopesInPreOrder(Scope* scope, std::vector<Scope*>* scopes) {
  scopes->push_back(scope);
  for (Scope* inner = scope->inner_scope(); inner != nullptr;
       inner = inner->sibling()) {
    CollectScopesInPreOrder(inner, scopes);
  }
}

std::vector<Scope*> CollectScopesInPreOrder(Scope* scope) {
  std::vector<Scope*> scopes;
  CollectScopesInPreOrder(scope, &scopes);
  return scopes;
}

std::optional<DebugScriptScope> FindFirstScopeOfType(
    DirectHandle<DebugScriptScopeInfo> info, ScopeType type) {
  for (int i = 0; i < DebugScriptScopeCount(*info); ++i) {
    DebugScriptScope scope = DebugScriptScope::FromIndex(info, i);
    if (scope.scope_type() == type) return scope;
  }
  return std::nullopt;
}

// Returns the source position of `needle` in `source`, so that tests can spell
// out break positions as source snippets instead of magic numbers.
int PositionOf(const char* source, const char* needle) {
  std::string_view haystack(source);
  size_t position = haystack.find(needle);
  CHECK_NE(position, std::string_view::npos);
  return static_cast<int>(position);
}

}  // namespace

TEST_F(DebugScopeInfoTest, ContainsPosition) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("function foo() { let a = 1; }");
  DebugScriptScope foo = DebugScriptScope::FromIndex(parsed.scope_info, 1);
  ASSERT_TRUE(foo.is_function_scope());

  const int start = foo.start_position();
  const int end = foo.end_position();

  EXPECT_FALSE(foo.ContainsPosition(start - 1, /*is_closure_found=*/true));
  EXPECT_FALSE(foo.ContainsPosition(start, /*is_closure_found=*/true));
  EXPECT_TRUE(foo.ContainsPosition(start + 1, /*is_closure_found=*/true));
  EXPECT_TRUE(foo.ContainsPosition(end - 1, /*is_closure_found=*/true));
  EXPECT_FALSE(foo.ContainsPosition(end, /*is_closure_found=*/true));
  EXPECT_FALSE(foo.ContainsPosition(end + 1, /*is_closure_found=*/true));

  // As long as the closure scope hasn't been found the end position is
  // inclusive, because nested arrow functions can share it.
  EXPECT_TRUE(foo.ContainsPosition(end, /*is_closure_found=*/false));
  EXPECT_FALSE(foo.ContainsPosition(end + 1, /*is_closure_found=*/false));
}

TEST_F(DebugScopeInfoTest, ContainsPositionClassScopeIncludesStart) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("class C { x = 1; }");
  std::optional<DebugScriptScope> class_scope =
      FindFirstScopeOfType(parsed.scope_info, ScopeType::CLASS_SCOPE);
  ASSERT_TRUE(class_scope.has_value());

  // The source position points at Token::kClass while the class context is
  // already pushed, so the start position has to be part of the scope.
  EXPECT_TRUE(class_scope->ContainsPosition(class_scope->start_position(),
                                            /*is_closure_found=*/true));
  EXPECT_FALSE(class_scope->ContainsPosition(class_scope->start_position() - 1,
                                             /*is_closure_found=*/true));
}

TEST_F(DebugScopeInfoTest, ContainsPositionWithScopeIncludesStart) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("with ({}) { let a = 1; }");
  std::optional<DebugScriptScope> with_scope =
      FindFirstScopeOfType(parsed.scope_info, ScopeType::WITH_SCOPE);
  ASSERT_TRUE(with_scope.has_value());

  EXPECT_TRUE(with_scope->ContainsPosition(with_scope->start_position(),
                                           /*is_closure_found=*/true));
  EXPECT_FALSE(with_scope->ContainsPosition(with_scope->start_position() - 1,
                                            /*is_closure_found=*/true));
}

TEST_F(DebugScopeInfoTest, FindClosureScopeForFunction) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("function foo() { let a = 1; }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  std::vector<Scope*> ast_scopes =
      CollectScopesInPreOrder(parsed.script_scope());
  ASSERT_EQ(ast_scopes.size(), 2u);
  Scope* foo = ast_scopes[1];
  ASSERT_TRUE(foo->is_function_scope());

  std::optional<DebugScriptScope> found = FindClosureScope(
      info, foo->start_position(), foo->end_position(), foo->scope_type());
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->scope_index(), 1);
  EXPECT_EQ(found->start_position(), foo->start_position());
  EXPECT_EQ(found->end_position(), foo->end_position());
  EXPECT_EQ(found->scope_type(), ScopeType::FUNCTION_SCOPE);
}

TEST_F(DebugScopeInfoTest, FindClosureScopeForScriptScope) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("let a = 1;");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;
  DebugScriptScope root = DebugScriptScope::FromIndex(info, 0);

  std::optional<DebugScriptScope> found =
      FindClosureScope(info, root.start_position(), root.end_position(),
                       ScopeType::SCRIPT_SCOPE);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->scope_index(), 0);
}

TEST_F(DebugScopeInfoTest, FindClosureScopeDistinguishesScopeTypes) {
  HandleScope scope(isolate());
  // A class declaration that spans the whole script gives the class scope the
  // exact same positions as the script scope, so the scope type is the only
  // thing that tells the two apart.
  ParsedScript parsed = ParseAndSerialize("class C { x = 1; }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script_scope = DebugScriptScope::FromIndex(info, 0);
  DebugScriptScope class_scope = DebugScriptScope::FromIndex(info, 1);
  ASSERT_TRUE(script_scope.is_script_scope());
  ASSERT_TRUE(class_scope.is_class_scope());
  ASSERT_EQ(script_scope.start_position(), class_scope.start_position());
  ASSERT_EQ(script_scope.end_position(), class_scope.end_position());

  std::optional<DebugScriptScope> found_script =
      FindClosureScope(info, script_scope.start_position(),
                       script_scope.end_position(), ScopeType::SCRIPT_SCOPE);
  ASSERT_TRUE(found_script.has_value());
  EXPECT_EQ(found_script->scope_index(), 0);

  std::optional<DebugScriptScope> found_class =
      FindClosureScope(info, class_scope.start_position(),
                       class_scope.end_position(), ScopeType::CLASS_SCOPE);
  ASSERT_TRUE(found_class.has_value());
  EXPECT_EQ(found_class->scope_index(), 1);
}

TEST_F(DebugScopeInfoTest, FindClosureScopeNestedArrowFunctions) {
  HandleScope scope(isolate());
  // Both arrow functions end at the same position.
  ParsedScript parsed = ParseAndSerialize("const f = a => b => a + b;");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  std::vector<Scope*> ast_scopes =
      CollectScopesInPreOrder(parsed.script_scope());
  ASSERT_EQ(ast_scopes.size(), 3u);
  Scope* outer_arrow = ast_scopes[1];
  Scope* inner_arrow = ast_scopes[2];
  ASSERT_TRUE(outer_arrow->is_function_scope());
  ASSERT_TRUE(inner_arrow->is_function_scope());
  ASSERT_EQ(outer_arrow->end_position(), inner_arrow->end_position());
  ASSERT_NE(outer_arrow->start_position(), inner_arrow->start_position());

  std::optional<DebugScriptScope> found_outer =
      FindClosureScope(info, outer_arrow->start_position(),
                       outer_arrow->end_position(), ScopeType::FUNCTION_SCOPE);
  ASSERT_TRUE(found_outer.has_value());
  EXPECT_EQ(found_outer->scope_index(), 1);

  std::optional<DebugScriptScope> found_inner =
      FindClosureScope(info, inner_arrow->start_position(),
                       inner_arrow->end_position(), ScopeType::FUNCTION_SCOPE);
  ASSERT_TRUE(found_inner.has_value());
  EXPECT_EQ(found_inner->scope_index(), 2);
}

TEST_F(DebugScopeInfoTest, FindClosureScopeNoMatch) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize("function foo() { let a = 1; }");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  std::vector<Scope*> ast_scopes =
      CollectScopesInPreOrder(parsed.script_scope());
  Scope* foo = ast_scopes[1];

  // Positions that don't belong to any scope.
  EXPECT_FALSE(FindClosureScope(info, foo->start_position() + 1,
                                foo->end_position(), ScopeType::FUNCTION_SCOPE)
                   .has_value());
  // Matching positions but the wrong scope type.
  EXPECT_FALSE(FindClosureScope(info, foo->start_position(),
                                foo->end_position(), ScopeType::BLOCK_SCOPE)
                   .has_value());
}

TEST_F(DebugScopeInfoTest, FindClosureScopeMatchesSerializationOrder) {
  HandleScope scope(isolate());
  ParsedScript parsed = ParseAndSerialize(
      "function outer(a = 1) {"
      "  let x = 2;"
      "  class C { static { let y = 3; } m() { return a; } }"
      "  try { } catch (e) { with ({}) { let z = 4; } }"
      "  return () => x;"
      "}");
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  std::vector<Scope*> ast_scopes =
      CollectScopesInPreOrder(parsed.script_scope());
  ASSERT_EQ(static_cast<int>(ast_scopes.size()), DebugScriptScopeCount(*info));

  for (size_t i = 0; i < ast_scopes.size(); ++i) {
    Scope* ast_scope = ast_scopes[i];
    std::optional<DebugScriptScope> found =
        FindClosureScope(info, ast_scope->start_position(),
                         ast_scope->end_position(), ast_scope->scope_type());
    ASSERT_TRUE(found.has_value()) << "no match for AST scope " << i;
    // Scopes are not unique in (start, end, type), so the lookup returns the
    // first match in serialization order.
    EXPECT_LE(found->scope_index(), static_cast<int>(i));
    EXPECT_EQ(found->start_position(), ast_scope->start_position());
    EXPECT_EQ(found->end_position(), ast_scope->end_position());
    EXPECT_EQ(found->scope_type(), ast_scope->scope_type());
  }
}

TEST_F(DebugScopeInfoTest, FindClosureScopeForModuleScope) {
  v8::HandleScope scope(v8_isolate());
  v8::ScriptOrigin origin(NewString("module.js"), 0, 0, false, -1,
                          v8::Local<v8::Value>(), false, false,
                          true /* is_module */);
  v8::ScriptCompiler::Source script_source(
      NewString("let module_x = 42;\n"
                "export function getX() { return module_x; }"),
      origin);
  v8::Local<v8::Module> module =
      v8::ScriptCompiler::CompileModule(v8_isolate(), &script_source)
          .ToLocalChecked();
  DirectHandle<SharedFunctionInfo> sfi =
      Utils::OpenHandle(*module->GetUnboundModuleScript());
  DirectHandle<Script> script(Cast<Script>(sfi->script()), isolate());

  isolate()->debug()->ClearScriptScopeInfos();
  Handle<DebugScriptScopeInfo> info =
      EnsureDebugScriptScopeInfo(isolate(), script);
  ASSERT_FALSE(info.is_null());

  DebugScriptScope root = DebugScriptScope::FromIndex(info, 0);
  ASSERT_TRUE(root.is_module_scope());

  std::optional<DebugScriptScope> found =
      FindClosureScope(info, root.start_position(), root.end_position(),
                       ScopeType::MODULE_SCOPE);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->scope_index(), 0);
}

TEST_F(DebugScopeInfoTest, FindInnermostScopeNestedBlocks) {
  HandleScope scope(isolate());
  const char* source =
      "function foo() { let a = 1; { let b = 2; { let c = 3; } } }";
  ParsedScript parsed = ParseAndSerialize(source);
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope foo = DebugScriptScope::FromIndex(info, 1);
  ASSERT_TRUE(foo.is_function_scope());

  DebugScriptScope outer_block =
      FindInnermostScope(foo, PositionOf(source, "let b") + 1);
  EXPECT_EQ(outer_block.scope_index(), 2);
  EXPECT_TRUE(outer_block.is_block_scope());

  DebugScriptScope inner_block =
      FindInnermostScope(foo, PositionOf(source, "let c") + 1);
  EXPECT_EQ(inner_block.scope_index(), 3);
  EXPECT_TRUE(inner_block.is_block_scope());

  // Positions outside of any block resolve to the closure scope itself.
  DebugScriptScope function_scope =
      FindInnermostScope(foo, PositionOf(source, "let a") + 1);
  EXPECT_EQ(function_scope.scope_index(), foo.scope_index());
}

TEST_F(DebugScopeInfoTest, FindInnermostScopeStartsAtClosureScope) {
  HandleScope scope(isolate());
  // The block scope of `bar` is not a descendant of `foo`, so searching from
  // `foo` for a position inside `bar` must not leave `foo`.
  const char* source =
      "function foo() { let a = 1; } function bar() { { let b = 2; } }";
  ParsedScript parsed = ParseAndSerialize(source);
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script_scope = DebugScriptScope::FromIndex(info, 0);
  DebugScriptScope foo =
      FindInnermostScope(script_scope, PositionOf(source, "let a") + 1);
  ASSERT_TRUE(foo.is_function_scope());

  DebugScriptScope found =
      FindInnermostScope(foo, PositionOf(source, "let b") + 1);
  EXPECT_EQ(found.scope_index(), foo.scope_index());
}

TEST_F(DebugScopeInfoTest, FindInnermostScopeCatchScope) {
  HandleScope scope(isolate());
  const char* source = "function foo() { try { } catch (e) { let a = 1; } }";
  ParsedScript parsed = ParseAndSerialize(source);
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope foo = DebugScriptScope::FromIndex(info, 1);
  ASSERT_TRUE(foo.is_function_scope());

  DebugScriptScope found =
      FindInnermostScope(foo, PositionOf(source, "let a") + 1);
  // The catch scope holds `e` and has the catch body block scope as its child.
  EXPECT_TRUE(found.is_block_scope() || found.is_catch_scope());
  std::optional<DebugScriptScope> catch_scope =
      FindFirstScopeOfType(info, ScopeType::CATCH_SCOPE);
  ASSERT_TRUE(catch_scope.has_value());
  EXPECT_GE(found.start_position(), catch_scope->start_position());
  EXPECT_LE(found.end_position(), catch_scope->end_position());
}

TEST_F(DebugScopeInfoTest, FindInnermostScopeWithScope) {
  HandleScope scope(isolate());
  const char* source = "function foo() { with ({}) { let a = 1; } }";
  ParsedScript parsed = ParseAndSerialize(source);
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope foo = DebugScriptScope::FromIndex(info, 1);
  std::optional<DebugScriptScope> with_scope =
      FindFirstScopeOfType(info, ScopeType::WITH_SCOPE);
  ASSERT_TRUE(with_scope.has_value());

  // "with" scopes accept their start position, because the context can already
  // be pushed while the source position still points at the header.
  DebugScriptScope found =
      FindInnermostScope(foo, with_scope->start_position());
  EXPECT_EQ(found.scope_index(), with_scope->scope_index());
}

TEST_F(DebugScopeInfoTest, FindInnermostScopeClassScope) {
  HandleScope scope(isolate());
  const char* source = "function foo() { class C { m() {} } }";
  ParsedScript parsed = ParseAndSerialize(source);
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope foo = DebugScriptScope::FromIndex(info, 1);
  std::optional<DebugScriptScope> class_scope =
      FindFirstScopeOfType(info, ScopeType::CLASS_SCOPE);
  ASSERT_TRUE(class_scope.has_value());

  // Class scopes accept their start position, because the class context is
  // already pushed while the source position still points at Token::kClass.
  DebugScriptScope found =
      FindInnermostScope(foo, class_scope->start_position());
  EXPECT_EQ(found.scope_index(), class_scope->scope_index());
}

TEST_F(DebugScopeInfoTest, FindInnermostScopeNestedArrowFunctions) {
  HandleScope scope(isolate());
  // Both arrow functions end at the same position, so the tightest fit is
  // decided by the start position.
  const char* source = "const f = a => b => a + b;";
  ParsedScript parsed = ParseAndSerialize(source);
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope script_scope = DebugScriptScope::FromIndex(info, 0);
  DebugScriptScope found =
      FindInnermostScope(script_scope, PositionOf(source, "a + b") + 1);
  EXPECT_EQ(found.scope_index(), 2);
}

TEST_F(DebugScopeInfoTest, FindInnermostScopeOutsideOfClosureScope) {
  HandleScope scope(isolate());
  const char* source = "function foo() { let a = 1; } let b = 2;";
  ParsedScript parsed = ParseAndSerialize(source);
  DirectHandle<DebugScriptScopeInfo> info = parsed.scope_info;

  DebugScriptScope foo = DebugScriptScope::FromIndex(info, 1);
  // A position that isn't covered by `foo` at all still returns `foo`, there
  // is nothing tighter to descend into.
  DebugScriptScope found =
      FindInnermostScope(foo, PositionOf(source, "let b") + 1);
  EXPECT_EQ(found.scope_index(), foo.scope_index());
}

}  // namespace internal
}  // namespace v8
