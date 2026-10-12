// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <ostream>

#include "absl/container/flat_hash_map.h"
#include "include/v8-script.h"
#include "src/objects/js-objects-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/scope-info.h"
#include "src/profiler/heap-profiler.h"
#include "src/profiler/heap-snapshot-generator.h"
#include "test/unittests/profiler/heap-snapshot-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

namespace {

struct SnapshotScopeId {
  int script_node_index = -1;
  int scope_id = -1;

  bool operator==(const SnapshotScopeId& other) const = default;
  bool operator<(const SnapshotScopeId& other) const {
    if (script_node_index != other.script_node_index) {
      return script_node_index < other.script_node_index;
    }
    return scope_id < other.scope_id;
  }

  template <typename H>
  friend H AbslHashValue(H h, const SnapshotScopeId& s) {
    return H::combine(std::move(h), s.script_node_index, s.scope_id);
  }
};

struct VariableDefinition {
  std::string name;
  int slot_index = -1;
  SnapshotScopeId declaring_scope;
  std::vector<SnapshotScopeId> uses;
};

struct SnapshotSourceScopeData {
  const HeapEntry* script_entry = nullptr;
  int script_node_index = -1;
  int scope_id = -1;
  int depth = 0;
  SnapshotScopeId id;
  const SnapshotSourceScopeData* parent = nullptr;
  std::vector<const SnapshotSourceScopeData*> children;
  std::vector<std::pair<int, int>> uses;  // (declaring_scope_id, slot_index)
  std::vector<VariableDefinition> variables;

  SnapshotSourceScopeData() = default;
  SnapshotSourceScopeData(const SnapshotSourceScopeData&) = delete;
  SnapshotSourceScopeData& operator=(const SnapshotSourceScopeData&) = delete;
  SnapshotSourceScopeData(SnapshotSourceScopeData&&) = default;
  SnapshotSourceScopeData& operator=(SnapshotSourceScopeData&&) = default;

  const VariableDefinition* FindVariable(const std::string& name) const {
    for (const auto& var : variables) {
      if (var.name == name) return &var;
    }
    return nullptr;
  }
};

void AssertUses(
    const VariableDefinition* var,
    std::initializer_list<const SnapshotSourceScopeData*> expected_scopes) {
  ASSERT_NE(nullptr, var);
  std::vector<SnapshotScopeId> expected_ids;
  expected_ids.reserve(std::size(expected_scopes));
  for (const auto* scope : expected_scopes) {
    ASSERT_NE(nullptr, scope);
    expected_ids.push_back(scope->id);
  }
  std::vector<SnapshotScopeId> actual_ids = var->uses;
  std::sort(expected_ids.begin(), expected_ids.end());
  std::sort(actual_ids.begin(), actual_ids.end());
  EXPECT_EQ(actual_ids, expected_ids);
}

void LinkScopesAndVariables(std::vector<SnapshotSourceScopeData>& scopes) {
  absl::flat_hash_map<SnapshotScopeId, SnapshotSourceScopeData*> scope_lookup;
  for (auto& scope : scopes) {
    scope_lookup[scope.id] = &scope;
  }

  std::vector<SnapshotSourceScopeData*> ancestor_stack;
  for (auto& scope : scopes) {
    while (ancestor_stack.size() > static_cast<size_t>(scope.depth)) {
      ancestor_stack.pop_back();
    }
    if (!ancestor_stack.empty() &&
        ancestor_stack.back()->script_node_index != scope.script_node_index) {
      ancestor_stack.clear();
    }
    if (!ancestor_stack.empty()) {
      scope.parent = ancestor_stack.back();
      ancestor_stack.back()->children.push_back(&scope);
    }
    ancestor_stack.push_back(&scope);

    for (const auto& use : scope.uses) {
      SnapshotScopeId decl_id{scope.script_node_index, use.first};
      int slot_idx = use.second;
      auto it = scope_lookup.find(decl_id);
      if (it != scope_lookup.end() && slot_idx >= 0 &&
          static_cast<size_t>(slot_idx) < it->second->variables.size()) {
        it->second->variables[slot_idx].uses.push_back(scope.id);
      }
    }
  }
}

std::vector<SnapshotSourceScopeData> ExtractSourceScopes(
    const HeapSnapshot* snapshot) {
  std::vector<SnapshotSourceScopeData> result;
  const auto& scopes = snapshot->source_scopes();
  const auto& context_vars = snapshot->source_scope_context_vars();
  const auto& uses = snapshot->source_scope_uses();

  result.reserve(scopes.size());
  size_t cv_offset = 0;
  size_t uses_offset = 0;
  for (const auto& scope : scopes) {
    SnapshotSourceScopeData data;
    data.script_entry = scope.script_entry;
    data.script_node_index =
        scope.script_entry != nullptr ? scope.script_entry->index() : -1;
    data.scope_id = scope.scope_id;
    data.depth = scope.depth;
    data.id = SnapshotScopeId{data.script_node_index, data.scope_id};

    data.variables.reserve(scope.scope_context_vars_count);
    for (uint32_t c = 0; c < scope.scope_context_vars_count; ++c) {
      data.variables.push_back(VariableDefinition{
          context_vars[cv_offset + c], static_cast<int>(c), data.id, {}});
    }
    cv_offset += scope.scope_context_vars_count;

    for (uint32_t u = 0; u < scope.scope_uses_count; ++u) {
      const auto& use = uses[uses_offset + u];
      data.uses.push_back({use.declaring_scope_id, use.slot_index});
    }
    uses_offset += scope.scope_uses_count;

    result.push_back(std::move(data));
  }
  LinkScopesAndVariables(result);
  return result;
}

// Test fixture for the scope information recorded in heap snapshots. It takes
// a heap snapshot, extracts its scope tree and provides helpers to look up and
// validate the scopes of a given closure.
class HeapSnapshotScopesTest : public TestWithContext {
 public:
  void TakeHeapSnapshot() {
    HeapProfiler* heap_profiler = i_isolate()->heap()->heap_profiler();
    v8::HeapProfiler::HeapSnapshotOptions options;
    snapshot_ = heap_profiler->TakeSnapshot(options);
    scopes_ = ExtractSourceScopes(snapshot_);
  }

  template <typename T = JSFunction>
  DirectHandle<T> RunJSForObject(const char* source) {
    v8::Local<v8::Value> val = RunJS(source);
    return Cast<T>(Utils::OpenHandle(*val));
  }

  DirectHandle<JSFunction> RunJSForClosure(const char* source) {
    return RunJSForObject<JSFunction>(source);
  }

  static v8::MaybeLocal<v8::Module> ResolveModuleCallback(
      v8::Local<v8::Context> context, v8::Local<v8::String> specifier,
      v8::Local<v8::FixedArray> import_attributes,
      v8::Local<v8::Module> referrer) {
    return v8::MaybeLocal<v8::Module>();
  }

  v8::Local<v8::Module> RunModule(const char* source) {
    v8::Local<v8::String> source_string =
        v8::String::NewFromUtf8(isolate(), source).ToLocalChecked();
    v8::ScriptOrigin origin(
        v8::String::NewFromUtf8(isolate(), "module.js").ToLocalChecked(), 0, 0,
        false, -1, v8::Local<v8::Value>(), false, false, true);
    v8::ScriptCompiler::Source script_source(source_string, origin);
    v8::Local<v8::Module> module =
        v8::ScriptCompiler::CompileModule(isolate(), &script_source)
            .ToLocalChecked();
    CHECK(
        module->InstantiateModule(context(), ResolveModuleCallback).FromJust());
    v8::MaybeLocal<v8::Promise> promise = module->Evaluate(context());
    CHECK(!promise.IsEmpty());
    CHECK_EQ(promise.ToLocalChecked()->State(), v8::Promise::kFulfilled);
    return module;
  }

  template <typename T = JSFunction>
  DirectHandle<T> GetModuleExport(v8::Local<v8::Module> module,
                                  const char* export_name) {
    v8::Local<v8::Object> ns = module->GetModuleNamespace().As<v8::Object>();
    v8::Local<v8::Value> export_val =
        ns->Get(
              context(),
              v8::String::NewFromUtf8(isolate(), export_name).ToLocalChecked())
            .ToLocalChecked();
    return Cast<T>(Utils::OpenHandle(*export_val));
  }

  DirectHandle<JSFunction> RunModuleForClosure(const char* source,
                                               const char* export_name) {
    v8::Local<v8::Module> module = RunModule(source);
    return GetModuleExport<JSFunction>(module, export_name);
  }

  SnapshotScopeId GetScopeId(DirectHandle<JSFunction> function) {
    return GetScopeId(*function);
  }

  SnapshotScopeId GetScopeId(Tagged<JSFunction> function) {
    const HeapEntry* entry = GetEntryFor(i_isolate(), snapshot_, function);
    CHECK_NOT_NULL(entry);
    return GetScopeId(entry);
  }

  const SnapshotSourceScopeData* GetScopeForClosure(
      DirectHandle<JSFunction> function) {
    return GetScopeForClosure(*function);
  }

  const SnapshotSourceScopeData* GetScopeForClosure(
      Tagged<JSFunction> function) {
    CHECK_NOT_NULL(snapshot_);
    SnapshotScopeId scope_id = GetScopeId(function);
    const SnapshotSourceScopeData* scope = FindScopeById(scope_id);
    CHECK_NOT_NULL(scope);
    return scope;
  }

  const SnapshotSourceScopeData* FindScopeWithContextVarInScript(
      const SnapshotSourceScopeData* scope_in_script,
      const std::string& var_name) const {
    CHECK_NOT_NULL(scope_in_script);
    const HeapEntry* script_entry = scope_in_script->script_entry;
    CHECK_NOT_NULL(script_entry);
    for (const auto& scope : scopes_) {
      if (scope.script_entry == script_entry &&
          scope.FindVariable(var_name) != nullptr) {
        return &scope;
      }
    }
    return nullptr;
  }

  void CheckContextSlots(DirectHandle<JSFunction> function) {
    CheckContextSlots(*function);
  }

  void CheckContextSlots(Tagged<JSFunction> function) {
    CHECK_NOT_NULL(snapshot_);
    const SnapshotSourceScopeData* func_scope = GetScopeForClosure(function);
    ASSERT_NE(nullptr, func_scope);
    const SnapshotSourceScopeData* curr_scope = func_scope->parent;

    DisallowGarbageCollection no_gc;
    for (Tagged<Context> ctx = function->context();
         !ctx.is_null() && !IsNativeContext(ctx); ctx = ctx->previous()) {
      Tagged<ScopeInfo> scope_info = ctx->scope_info();
      int ctx_scope_id = scope_info->UniqueIdInScript();

      while (curr_scope != nullptr && curr_scope->scope_id != ctx_scope_id) {
        EXPECT_TRUE(curr_scope->variables.empty());
        curr_scope = curr_scope->parent;
      }
      ASSERT_NE(nullptr, curr_scope);
      EXPECT_EQ(curr_scope->scope_id, ctx_scope_id);

      if (curr_scope->variables.empty()) {
        // Context variable emission was disabled for this scope (e.g., class
        // scope or a scope containing/enclosing direct eval).
      } else {
        int local_count = scope_info->ContextLocalCount();
        EXPECT_EQ(curr_scope->variables.size(),
                  static_cast<size_t>(local_count));

        // Check that the variables from the source scopes match exactly the
        // order of context fields in the corresponding ScopeInfo.
        for (int i = 0; i < local_count; ++i) {
          const VariableDefinition& var_def = curr_scope->variables[i];
          EXPECT_EQ(var_def.slot_index, i);
          EXPECT_EQ(var_def.name,
                    scope_info->ContextInlinedLocalName(i)->ToCString().get());

          int slot_index = scope_info->ContextHeaderLength() + i;
          EXPECT_LT(slot_index, ctx->length());
        }
      }

      const HeapEntry* ctx_entry = GetEntryFor(i_isolate(), snapshot_, ctx);
      ASSERT_NE(nullptr, ctx_entry);
      std::vector<const HeapGraphEdge*> context_var_edges;
      for (int i = 0; i < ctx_entry->children_count(); ++i) {
        const HeapGraphEdge* edge = ctx_entry->child(i);
        if (edge->type() == HeapGraphEdge::kContextVariable) {
          context_var_edges.push_back(edge);
        }
      }
      if (!curr_scope->variables.empty()) {
        ASSERT_EQ(curr_scope->variables.size(), context_var_edges.size());
        for (size_t i = 0; i < context_var_edges.size(); ++i) {
          EXPECT_EQ(curr_scope->variables[i].name,
                    context_var_edges[i]->name());
        }
      }
      if (!IsNativeContext(ctx->previous())) {
        const HeapGraphEdge* prev_edge = GetNamedEdge(*ctx_entry, "previous");
        ASSERT_NE(nullptr, prev_edge);
        const HeapEntry* prev_entry =
            GetEntryFor(i_isolate(), snapshot_, ctx->previous());
        EXPECT_EQ(prev_entry, prev_edge->to());
      }

      curr_scope = curr_scope->parent;
    }

    while (curr_scope != nullptr) {
      EXPECT_TRUE(curr_scope->variables.empty());
      curr_scope = curr_scope->parent;
    }
  }

  HeapSnapshot* snapshot() const { return snapshot_; }
  const std::vector<SnapshotSourceScopeData>& scopes() const { return scopes_; }
  std::vector<SnapshotSourceScopeData>& scopes() { return scopes_; }

 private:
  const SnapshotSourceScopeData* FindScopeById(SnapshotScopeId id) const {
    for (const auto& scope : scopes_) {
      if (scope.id == id) return &scope;
    }
    return nullptr;
  }

  SnapshotScopeId GetScopeId(const HeapEntry* function_entry) {
    const HeapGraphEdge* shared_edge = GetNamedEdge(*function_entry, "shared");
    const HeapEntry* shared_entry =
        shared_edge != nullptr ? shared_edge->to() : function_entry;

    const HeapGraphEdge* script_edge = GetNamedEdge(*shared_entry, "script");
    CHECK_NOT_NULL(script_edge);
    const HeapEntry* script_entry = script_edge->to();

    std::optional<int> scope_id = GetIntEdge(shared_entry, "scope_id");
    CHECK(scope_id.has_value());
    return SnapshotScopeId{script_entry->index(), *scope_id};
  }

  HeapSnapshot* snapshot_ = nullptr;
  std::vector<SnapshotSourceScopeData> scopes_;
};

}  // namespace

TEST_F(HeapSnapshotScopesTest, Basic) {
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "function outer(a) {\n"
      "  let b = 1;\n"
      "  b = 2;\n"
      "  {\n"
      "    let blockScoped = 3;\n"
      "    b = blockScoped;\n"
      "  }\n"
      "  function inner(c) {\n"
      "    let d = 2;\n"
      "    return a + b + c + d;\n"
      "  }\n"
      "  return inner;\n"
      "}\n"
      "outer(10);\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* inner = GetScopeForClosure(*inner_fn);
  ASSERT_NE(nullptr, inner->script_entry);

  const SnapshotSourceScopeData* outer = inner->parent;
  ASSERT_NE(nullptr, outer);
  ASSERT_EQ(2u, outer->children.size());
  EXPECT_EQ(inner, outer->children[0]);
  const SnapshotSourceScopeData* block_scope = outer->children[1];

  const VariableDefinition* var_a = outer->FindVariable("a");
  const VariableDefinition* var_b = outer->FindVariable("b");
  AssertUses(var_a, {inner});
  // Currently all uses of a context variable are added. Even if its within the
  // defining function.
  AssertUses(var_b, {outer, block_scope, inner});
  CheckContextSlots(inner_fn);
}

TEST_F(HeapSnapshotScopesTest, ClosesOverVariable) {
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "function outer() {\n"
      "  let captured = 1;\n"
      "  let blockCaptured = 2;\n"
      "  let stackOnly = 3;\n"
      "  return function inner() {\n"
      "    {\n"
      "      let blockScoped = 4;\n"
      "      blockCaptured += blockScoped;\n"
      "    }\n"
      "    return captured;\n"
      "  };\n"
      "}\n"
      "outer();\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* inner_scope = GetScopeForClosure(*inner_fn);

  ASSERT_EQ(1u, inner_scope->children.size());
  const SnapshotSourceScopeData* block_scope = inner_scope->children[0];

  const SnapshotSourceScopeData* outer = inner_scope->parent;
  ASSERT_NE(nullptr, outer);
  const VariableDefinition* captured = outer->FindVariable("captured");
  EXPECT_EQ(0, captured->slot_index);
  AssertUses(captured, {inner_scope});

  const VariableDefinition* block_captured =
      outer->FindVariable("blockCaptured");
  EXPECT_EQ(1, block_captured->slot_index);
  AssertUses(block_captured, {block_scope});

  EXPECT_EQ(nullptr, outer->FindVariable("stackOnly"));
  EXPECT_EQ(nullptr, block_scope->FindVariable("blockScoped"));
  CheckContextSlots(inner_fn);
}

TEST_F(HeapSnapshotScopesTest, UsesInDifferentClosures) {
  DirectHandle<JSArray> funcs = RunJSForObject<JSArray>(
      "function outer() {\n"
      "  let captured = 1;\n"
      "  function first() {\n"
      "    return captured + captured;\n"
      "  }\n"
      "  const second = () => captured;\n"
      "  return [first, second];\n"
      "}\n"
      "outer();\n");

  TakeHeapSnapshot();
  DirectHandle<JSFunction> first_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), funcs, 0).ToHandleChecked());
  DirectHandle<JSFunction> second_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), funcs, 1).ToHandleChecked());

  const SnapshotSourceScopeData* first = GetScopeForClosure(*first_fn);
  const SnapshotSourceScopeData* second = GetScopeForClosure(*second_fn);

  EXPECT_EQ(first->parent, second->parent);
  const SnapshotSourceScopeData* outer = first->parent;
  ASSERT_NE(nullptr, outer);

  const VariableDefinition* captured = outer->FindVariable("captured");
  AssertUses(captured, {first, first, second});
  CheckContextSlots(first_fn);
  CheckContextSlots(second_fn);
}

TEST_F(HeapSnapshotScopesTest, FreeVariables) {
  DirectHandle<JSFunction> arrow = RunJSForClosure(
      "function outer() {\n"
      "  return () => missing + missing;\n"
      "}\n"
      "outer();\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* arrow_scope = GetScopeForClosure(*arrow);
  EXPECT_TRUE(arrow_scope->variables.empty());
  EXPECT_TRUE(arrow_scope->uses.empty());

  // Free variable `missing` should not be allocated as a context variable in
  // outer or the arrow function.
  EXPECT_EQ(nullptr, FindScopeWithContextVarInScript(arrow_scope, "missing"));
  CheckContextSlots(arrow);
}

TEST_F(HeapSnapshotScopesTest, GlobalVariables) {
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "let scriptLet = 1;\n"
      "const scriptConst = 2;\n"
      "var scriptVar = 3;\n"
      "function inner() {\n"
      "  return scriptLet + scriptConst + scriptVar;\n"
      "}\n"
      "inner;\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* inner = GetScopeForClosure(*inner_fn);

  // Script scope contains lexical variables (let and const) in context vars.
  const SnapshotSourceScopeData* script_scope = inner->parent;
  ASSERT_NE(nullptr, script_scope);
  EXPECT_EQ(0, script_scope->depth);
  EXPECT_EQ(nullptr, script_scope->parent);
  EXPECT_NE(nullptr, script_scope->FindVariable("scriptConst"));
  // Global var is on the global object, not in script context.
  EXPECT_EQ(nullptr, script_scope->FindVariable("scriptVar"));

  const VariableDefinition* let_var = script_scope->FindVariable("scriptLet");
  const VariableDefinition* const_var =
      script_scope->FindVariable("scriptConst");
  AssertUses(let_var, {inner});
  AssertUses(const_var, {inner});
  CheckContextSlots(inner_fn);
}

TEST_F(HeapSnapshotScopesTest, FunctionVarCaptured) {
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "function outer() {\n"
      "  var varCaptured = 1;\n"
      "  let letCaptured = 2;\n"
      "  const constCaptured = 3;\n"
      "  var varUnused = 4;\n"
      "  return function inner() {\n"
      "    return varCaptured + letCaptured + constCaptured;\n"
      "  };\n"
      "}\n"
      "outer();\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* inner = GetScopeForClosure(*inner_fn);

  const SnapshotSourceScopeData* outer = inner->parent;
  ASSERT_NE(nullptr, outer);
  const VariableDefinition* var = outer->FindVariable("varCaptured");
  const VariableDefinition* let_var = outer->FindVariable("letCaptured");
  const VariableDefinition* const_var = outer->FindVariable("constCaptured");
  EXPECT_EQ(nullptr, outer->FindVariable("varUnused"));

  AssertUses(var, {inner});
  AssertUses(let_var, {inner});
  AssertUses(const_var, {inner});
  CheckContextSlots(inner_fn);
}

TEST_F(HeapSnapshotScopesTest, ForOfBodyScope) {
  DirectHandle<JSArray> closures = RunJSForObject<JSArray>(
      "function makeClosures() {\n"
      "  const closures = [];\n"
      "  for (const item of [1, 2]) {\n"
      "    const captured = {item};\n"
      "    const unused = 42;\n"
      "    closures.push(function blockInner() {\n"
      "      return captured;\n"
      "    });\n"
      "  }\n"
      "  return closures;\n"
      "}\n"
      "makeClosures();\n");

  TakeHeapSnapshot();
  DirectHandle<JSFunction> block_inner_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), closures, 0).ToHandleChecked());
  const SnapshotSourceScopeData* block_inner =
      GetScopeForClosure(*block_inner_fn);

  const SnapshotSourceScopeData* body_scope = block_inner->parent;
  ASSERT_NE(nullptr, body_scope);
  const VariableDefinition* captured = body_scope->FindVariable("captured");
  AssertUses(captured, {block_inner});
  EXPECT_EQ(nullptr, body_scope->FindVariable("unused"));

  // `item` is only used locally to initialize `captured` and is not closed
  // over, so it should not be context-allocated.
  const SnapshotSourceScopeData* loop_scope = body_scope->parent;
  ASSERT_NE(nullptr, loop_scope);
  EXPECT_EQ(nullptr, loop_scope->FindVariable("item"));
  EXPECT_EQ(nullptr, FindScopeWithContextVarInScript(body_scope, "item"));
  CheckContextSlots(block_inner_fn);
}

TEST_F(HeapSnapshotScopesTest, ForOfIterationVariable) {
  DirectHandle<JSArray> closures = RunJSForObject<JSArray>(
      "function makeClosures(values) {\n"
      "  const closures = [];\n"
      "  for (const item of values) {\n"
      "    closures.push(() => item);\n"
      "  }\n"
      "  return closures;\n"
      "}\n"
      "makeClosures([1, 2]);\n");

  TakeHeapSnapshot();
  DirectHandle<JSFunction> arrow_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), closures, 0).ToHandleChecked());
  const SnapshotSourceScopeData* arrow = GetScopeForClosure(*arrow_fn);

  const SnapshotSourceScopeData* loop_scope = arrow->parent;
  ASSERT_NE(nullptr, loop_scope);
  const VariableDefinition* item_var = loop_scope->FindVariable("item");
  AssertUses(item_var, {arrow});
  CheckContextSlots(arrow_fn);
}

TEST_F(HeapSnapshotScopesTest, ClassicForLoop) {
  DirectHandle<JSArray> callbacks = RunJSForObject<JSArray>(
      "function makeCallbacks() {\n"
      "  const callbacks = [];\n"
      "  for (let index = 0; index < 2; ++index) {\n"
      "    callbacks.push(() => index);\n"
      "  }\n"
      "  return callbacks;\n"
      "}\n"
      "makeCallbacks();\n");

  TakeHeapSnapshot();
  DirectHandle<JSFunction> arrow_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), callbacks, 0).ToHandleChecked());
  const SnapshotSourceScopeData* arrow = GetScopeForClosure(*arrow_fn);

  const SnapshotSourceScopeData* loop_body_scope = arrow->parent;
  ASSERT_NE(nullptr, loop_body_scope);
  const VariableDefinition* index_var = loop_body_scope->FindVariable("index");

  // The iteration scope itself uses index (in condition/update), and the
  // inner arrow closure also uses index.
  AssertUses(index_var, {loop_body_scope, loop_body_scope, arrow});
  CheckContextSlots(arrow_fn);
}

TEST_F(HeapSnapshotScopesTest, Catch) {
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "function makeHandler() {\n"
      "  try {\n"
      "    throw new Error('boom');\n"
      "  } catch (error) {\n"
      "    const detail = error.message;\n"
      "    const unused = 42;\n"
      "    return function inner() {\n"
      "      return [error, detail];\n"
      "    };\n"
      "  }\n"
      "}\n"
      "makeHandler();\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* inner = GetScopeForClosure(*inner_fn);

  const SnapshotSourceScopeData* catch_block = inner->parent;
  ASSERT_NE(nullptr, catch_block);

  const SnapshotSourceScopeData* catch_scope = catch_block->parent;
  ASSERT_NE(nullptr, catch_scope);

  EXPECT_EQ(nullptr, catch_block->FindVariable("unused"));

  const VariableDefinition* error_var = catch_scope->FindVariable("error");
  const VariableDefinition* detail_var = catch_block->FindVariable("detail");

  AssertUses(error_var, {catch_block, inner});
  AssertUses(detail_var, {inner});
  CheckContextSlots(inner_fn);
}

TEST_F(HeapSnapshotScopesTest, NestedFunctions) {
  DirectHandle<JSFunction> nested_inner_fn = RunJSForClosure(
      "function outer() {\n"
      "  let outerCaptured = 1;\n"
      "  return function inner() {\n"
      "    let innerCaptured = 2;\n"
      "    return function nested_inner() {\n"
      "      return outerCaptured + innerCaptured;\n"
      "    };\n"
      "  };\n"
      "}\n"
      "outer()();\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* nested_inner =
      GetScopeForClosure(*nested_inner_fn);

  const SnapshotSourceScopeData* inner = nested_inner->parent;
  ASSERT_NE(nullptr, inner);
  const SnapshotSourceScopeData* outer = inner->parent;
  ASSERT_NE(nullptr, outer);

  const VariableDefinition* outer_captured =
      outer->FindVariable("outerCaptured");
  const VariableDefinition* inner_captured =
      inner->FindVariable("innerCaptured");

  // inner does not use outerCaptured directly; nested_inner uses both.
  AssertUses(outer_captured, {nested_inner});
  AssertUses(inner_captured, {nested_inner});
  CheckContextSlots(nested_inner_fn);
}

TEST_F(HeapSnapshotScopesTest, BlockAndShadowingNoContextUses) {
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "function outer() {\n"
      "  let value = 1;\n"
      "  { value++; }\n"
      "  return function inner(value) {\n"
      "    return value;\n"
      "  };\n"
      "}\n"
      "outer();\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* inner = GetScopeForClosure(*inner_fn);

  // Neither outer's value nor inner's value should be context-allocated.
  EXPECT_EQ(nullptr, FindScopeWithContextVarInScript(inner, "value"));
  CheckContextSlots(inner_fn);
}

TEST_F(HeapSnapshotScopesTest, ShadowingCaptured) {
  DirectHandle<JSFunction> arrow_fn = RunJSForClosure(
      "function outer() {\n"
      "  let value = 1;\n"
      "  function inner(value) {\n"
      "    return () => value;\n"
      "  }\n"
      "  return inner(2);\n"
      "}\n"
      "outer();\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* arrow = GetScopeForClosure(*arrow_fn);

  const SnapshotSourceScopeData* inner = arrow->parent;
  ASSERT_NE(nullptr, inner);
  const SnapshotSourceScopeData* outer = inner->parent;
  ASSERT_NE(nullptr, outer);
  EXPECT_EQ(nullptr, outer->FindVariable("value"));

  const VariableDefinition* value_var = inner->FindVariable("value");
  ASSERT_NE(nullptr, value_var);
  EXPECT_EQ(inner, arrow->parent);
  AssertUses(value_var, {arrow});
  CheckContextSlots(arrow_fn);
}

TEST_F(HeapSnapshotScopesTest, ClassMethods) {
  RunJS(
      "function outer() {\n"
      "  let instanceCaptured = 1;\n"
      "  let staticCaptured = 2;\n"
      "  let unused = 3;\n"
      "  class Target {\n"
      "    instanceMethod() {\n"
      "      return instanceCaptured;\n"
      "    }\n"
      "    static staticMethod() {\n"
      "      return staticCaptured;\n"
      "    }\n"
      "  }\n"
      "  return Target;\n"
      "}\n"
      "var Target = outer();\n");

  DirectHandle<JSFunction> instance_method_fn =
      RunJSForClosure("new Target().instanceMethod");
  DirectHandle<JSFunction> static_method_fn =
      RunJSForClosure("Target.staticMethod");

  TakeHeapSnapshot();

  const SnapshotSourceScopeData* instance_method =
      GetScopeForClosure(*instance_method_fn);
  const SnapshotSourceScopeData* static_method =
      GetScopeForClosure(*static_method_fn);
  EXPECT_NE(instance_method, static_method);
  EXPECT_EQ(instance_method->parent, static_method->parent);

  const SnapshotSourceScopeData* class_scope = instance_method->parent;
  ASSERT_NE(nullptr, class_scope);
  const SnapshotSourceScopeData* outer = class_scope->parent;
  ASSERT_NE(nullptr, outer);

  const VariableDefinition* instance_captured =
      outer->FindVariable("instanceCaptured");
  const VariableDefinition* static_captured =
      outer->FindVariable("staticCaptured");
  EXPECT_EQ(nullptr, outer->FindVariable("unused"));

  AssertUses(instance_captured, {instance_method});
  AssertUses(static_captured, {static_method});
  CheckContextSlots(instance_method_fn);
  CheckContextSlots(static_method_fn);
}

TEST_F(HeapSnapshotScopesTest, ClassFieldInitializer) {
  DirectHandle<JSFunction> arrow = RunJSForClosure(
      "class Target {\n"
      "  instanceField = (() => {\n"
      "    let captured = 1;\n"
      "    let unused = 2;\n"
      "    return () => captured;\n"
      "  })();\n"
      "}\n"
      "new Target().instanceField;\n");

  TakeHeapSnapshot();

  const SnapshotSourceScopeData* inner_arrow = GetScopeForClosure(*arrow);
  const SnapshotSourceScopeData* iife = inner_arrow->parent;
  ASSERT_NE(nullptr, iife);
  const VariableDefinition* captured = iife->FindVariable("captured");
  EXPECT_EQ(nullptr, iife->FindVariable("unused"));

  AssertUses(captured, {inner_arrow});
  CheckContextSlots(arrow);
}

TEST_F(HeapSnapshotScopesTest, ClassPrivateFields) {
  RunJS(
      "function outer() {\n"
      "  let outerCaptured = 1;\n"
      "  let outerUnused = 2;\n"
      "  class Target {\n"
      "    #x = 10;\n"
      "    #y = 20;\n"
      "    method() {\n"
      "      return this.#x + outerCaptured;\n"
      "    }\n"
      "  }\n"
      "  return Target;\n"
      "}\n"
      "var Target = outer();\n");

  DirectHandle<JSFunction> method_fn = RunJSForClosure("new Target().method");

  TakeHeapSnapshot();

  const SnapshotSourceScopeData* method_scope = GetScopeForClosure(*method_fn);
  ASSERT_NE(nullptr, method_scope);

  const SnapshotSourceScopeData* class_scope = method_scope->parent;
  ASSERT_NE(nullptr, class_scope);
  const SnapshotSourceScopeData* outer_scope = class_scope->parent;
  ASSERT_NE(nullptr, outer_scope);

  const VariableDefinition* outer_captured =
      outer_scope->FindVariable("outerCaptured");
  ASSERT_NE(nullptr, outer_captured);
  // Class scopes contain private fields and methods (#x, #y) for which uses
  // across closures are currently not tracked. Therefore, context variables
  // are omitted for class scopes to disable dead context analysis.
  EXPECT_EQ(nullptr, class_scope->FindVariable("#x"));
  EXPECT_EQ(nullptr, class_scope->FindVariable("#y"));

  AssertUses(outer_captured, {method_scope});

  CheckContextSlots(method_fn);
}

TEST_F(HeapSnapshotScopesTest, DestructuringCaptured) {
  DirectHandle<JSFunction> arrow = RunJSForClosure(
      "function outer() {\n"
      "  let {a, b: [c], d = 4} = {a: 1, b: [2]};\n"
      "  let unused = 5;\n"
      "  return () => a + c + d;\n"
      "}\n"
      "outer();\n");

  TakeHeapSnapshot();
  const SnapshotSourceScopeData* arrow_scope = GetScopeForClosure(*arrow);

  const SnapshotSourceScopeData* outer = arrow_scope->parent;
  ASSERT_NE(nullptr, outer);

  const VariableDefinition* a_var = outer->FindVariable("a");
  const VariableDefinition* c_var = outer->FindVariable("c");
  const VariableDefinition* d_var = outer->FindVariable("d");
  EXPECT_EQ(nullptr, outer->FindVariable("unused"));

  AssertUses(a_var, {arrow_scope});
  AssertUses(c_var, {arrow_scope});
  AssertUses(d_var, {arrow_scope});
  CheckContextSlots(arrow);
}

TEST_F(HeapSnapshotScopesTest, ModuleScope) {
  v8::Local<v8::Module> module = RunModule(
      "let moduleVar = 1;\n"
      "let unused = 2;\n"
      "export function outer() {\n"
      "  let outerVar = 3;\n"
      "  function inner() {\n"
      "    return moduleVar + outerVar;\n"
      "  }\n"
      "  return inner;\n"
      "}\n"
      "export const innerFn = outer();\n");

  DirectHandle<JSFunction> outer_fn =
      GetModuleExport<JSFunction>(module, "outer");
  DirectHandle<JSFunction> inner_fn =
      GetModuleExport<JSFunction>(module, "innerFn");

  TakeHeapSnapshot();

  const SnapshotSourceScopeData* outer_scope = GetScopeForClosure(*outer_fn);
  ASSERT_NE(nullptr, outer_scope);

  const SnapshotSourceScopeData* inner_scope = GetScopeForClosure(*inner_fn);
  ASSERT_NE(nullptr, inner_scope);

  const SnapshotSourceScopeData* module_scope = outer_scope->parent;
  ASSERT_NE(nullptr, module_scope);
  EXPECT_EQ(-2, module_scope->scope_id);
  EXPECT_EQ(0, module_scope->depth);
  EXPECT_EQ(nullptr, module_scope->parent);

  EXPECT_EQ(outer_scope, inner_scope->parent);

  const VariableDefinition* module_var =
      module_scope->FindVariable("moduleVar");
  ASSERT_NE(nullptr, module_var);
  EXPECT_EQ(0, module_var->slot_index);
  AssertUses(module_var, {inner_scope});

  const VariableDefinition* outer_var = outer_scope->FindVariable("outerVar");
  ASSERT_NE(nullptr, outer_var);
  EXPECT_EQ(0, outer_var->slot_index);
  AssertUses(outer_var, {inner_scope});

  EXPECT_EQ(nullptr, module_scope->FindVariable("unused"));
  CheckContextSlots(outer_fn);
  CheckContextSlots(inner_fn);
}

TEST_F(HeapSnapshotScopesTest, CapturedThis) {
  DirectHandle<JSArray> funcs = RunJSForObject<JSArray>(
      "function outer() {\n"
      "  const first = () => this;\n"
      "  const second = () => this;\n"
      "  return [first, second];\n"
      "}\n"
      "outer.call({a: 1});\n");

  TakeHeapSnapshot();

  DirectHandle<JSFunction> first_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), funcs, 0).ToHandleChecked());
  DirectHandle<JSFunction> second_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), funcs, 1).ToHandleChecked());

  const SnapshotSourceScopeData* first_scope = GetScopeForClosure(*first_fn);
  ASSERT_NE(nullptr, first_scope);
  const SnapshotSourceScopeData* second_scope = GetScopeForClosure(*second_fn);
  ASSERT_NE(nullptr, second_scope);

  EXPECT_EQ(first_scope->parent, second_scope->parent);
  const SnapshotSourceScopeData* outer_scope = first_scope->parent;
  ASSERT_NE(nullptr, outer_scope);

  const VariableDefinition* this_var = outer_scope->FindVariable("this");
  ASSERT_NE(nullptr, this_var);
  EXPECT_EQ(0, this_var->slot_index);
  AssertUses(this_var, {first_scope, second_scope});

  CheckContextSlots(first_fn);
  CheckContextSlots(second_fn);
}

TEST_F(HeapSnapshotScopesTest, CapturedThisMultipleUses) {
  DirectHandle<JSFunction> arrow_fn = RunJSForClosure(
      "function outer() {\n"
      "  return () => [this, this];\n"
      "}\n"
      "outer.call({a: 1});\n");

  TakeHeapSnapshot();

  const SnapshotSourceScopeData* arrow_scope = GetScopeForClosure(*arrow_fn);
  ASSERT_NE(nullptr, arrow_scope);

  const SnapshotSourceScopeData* outer_scope = arrow_scope->parent;
  ASSERT_NE(nullptr, outer_scope);

  const VariableDefinition* this_var = outer_scope->FindVariable("this");
  ASSERT_NE(nullptr, this_var);
  EXPECT_EQ(0, this_var->slot_index);
  // The arrow function is only recorded once here (not both uses).
  AssertUses(this_var, {arrow_scope});

  CheckContextSlots(arrow_fn);
}

TEST_F(HeapSnapshotScopesTest, EvalScript) {
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "eval(\n"
      "    'function evalOuter() {\\n' +\n"
      "    '  let evalVar = 1;\\n' +\n"
      "    '  let unused = 2;\\n' +\n"
      "    '  return function evalInner() {\\n' +\n"
      "    '    return evalVar;\\n' +\n"
      "    '  };\\n' +\n"
      "    '}\\n' +\n"
      "    'evalOuter();\\n');\n");

  TakeHeapSnapshot();

  const SnapshotSourceScopeData* inner_scope = GetScopeForClosure(*inner_fn);
  ASSERT_NE(nullptr, inner_scope);

  const SnapshotSourceScopeData* outer_scope = inner_scope->parent;
  ASSERT_NE(nullptr, outer_scope);

  const SnapshotSourceScopeData* eval_scope = outer_scope->parent;
  ASSERT_NE(nullptr, eval_scope);
  EXPECT_EQ(-2, eval_scope->scope_id);
  EXPECT_EQ(0, eval_scope->depth);
  EXPECT_EQ(nullptr, eval_scope->parent);

  const VariableDefinition* eval_var = outer_scope->FindVariable("evalVar");
  ASSERT_NE(nullptr, eval_var);
  EXPECT_EQ(0, eval_var->slot_index);
  AssertUses(eval_var, {inner_scope});
  EXPECT_EQ(nullptr, outer_scope->FindVariable("unused"));

  CheckContextSlots(inner_fn);
}

TEST_F(HeapSnapshotScopesTest, EvalScriptWithEnclosingScope) {
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "function outer() {\n"
      "  let outerVar = 1;\n"
      "  return eval(\n"
      "      'let evalVar = 2;\\n' +\n"
      "      'function inner() {\\n' +\n"
      "      '  return outerVar + evalVar;\\n' +\n"
      "      '}\\n' +\n"
      "      'inner;\\n');\n"
      "}\n"
      "outer();\n");

  TakeHeapSnapshot();

  const SnapshotSourceScopeData* inner_scope = GetScopeForClosure(*inner_fn);
  ASSERT_NE(nullptr, inner_scope);

  const SnapshotSourceScopeData* eval_scope = inner_scope->parent;
  ASSERT_NE(nullptr, eval_scope);
  EXPECT_EQ(-2, eval_scope->scope_id);
  EXPECT_EQ(0, eval_scope->depth);
  EXPECT_EQ(nullptr, eval_scope->parent);

  const VariableDefinition* eval_var = eval_scope->FindVariable("evalVar");
  ASSERT_NE(nullptr, eval_var);
  EXPECT_EQ(0, eval_var->slot_index);
  AssertUses(eval_var, {inner_scope});

  DirectHandle<JSFunction> outer_fn = RunJSForClosure("outer");
  const SnapshotSourceScopeData* outer_scope = GetScopeForClosure(*outer_fn);
  ASSERT_NE(nullptr, outer_scope);

  // Since outer calls eval directly, variable emission is disabled for its
  // scope to disable dead context analysis.
  EXPECT_EQ(0u, outer_scope->variables.size());
  EXPECT_EQ(nullptr, outer_scope->FindVariable("outerVar"));

  // Since eval scripts are parsed self-contained during heap snapshot
  // generation without deserializing the outer ScopeInfo chain, inner only
  // resolves uses of variables declared within the eval script (evalVar).
  // References to outerVar are unresolved / treated as dynamic globals.
  std::vector<std::pair<int, int>> expected_uses = {
      {eval_scope->scope_id, eval_var->slot_index},
  };
  EXPECT_EQ(inner_scope->uses, expected_uses);

  CheckContextSlots(outer_fn);
}

TEST_F(HeapSnapshotScopesTest, StrictEvalScript) {
  // Eval inside a strict function inherits strict mode from the caller.
  // In strict mode eval, 'var' declarations are scoped to the eval scope
  // itself rather than leaking out, and can be allocated to context slots.
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "function outer() {\n"
      "  'use strict';\n"
      "  return eval(\n"
      "      'var evalVar = 1;\\n' +\n"
      "      'function inner() {\\n' +\n"
      "      '  return evalVar;\\n' +\n"
      "      '}\\n' +\n"
      "      'inner;\\n');\n"
      "}\n"
      "outer();\n");

  TakeHeapSnapshot();

  const SnapshotSourceScopeData* inner_scope = GetScopeForClosure(*inner_fn);
  ASSERT_NE(nullptr, inner_scope);

  const SnapshotSourceScopeData* eval_scope = inner_scope->parent;
  ASSERT_NE(nullptr, eval_scope);
  EXPECT_EQ(-2, eval_scope->scope_id);
  EXPECT_EQ(0, eval_scope->depth);
  EXPECT_EQ(nullptr, eval_scope->parent);

  // In strict eval, 'var evalVar' is a local of the eval scope and allocated
  // to a context slot since 'inner' closes over it.
  const VariableDefinition* eval_var = eval_scope->FindVariable("evalVar");
  ASSERT_NE(nullptr, eval_var);
  EXPECT_EQ(0, eval_var->slot_index);
  AssertUses(eval_var, {inner_scope});

  DirectHandle<JSFunction> outer_fn = RunJSForClosure("outer");
  CheckContextSlots(outer_fn);
}

TEST_F(HeapSnapshotScopesTest, DirectEvalDisablesVariableEmission) {
  // Test that direct eval in an inner scope disables variable emission
  // for both the inner scope and its enclosing outer scopes, while an
  // unaffected sibling function still has its context variables emitted.
  DirectHandle<JSArray> funcs = RunJSForObject<JSArray>(
      "function enclosing() {\n"
      "  let outerVar = 1;\n"
      "  function middle() {\n"
      "    let middleVar = 2;\n"
      "    eval('middleVar');\n"
      "  }\n"
      "  function sibling() {\n"
      "    let siblingVar = 3;\n"
      "    return function innerOfSibling() {\n"
      "      return siblingVar;\n"
      "    };\n"
      "  }\n"
      "  middle();\n"
      "  const inner = sibling();\n"
      "  return [enclosing, middle, sibling, inner];\n"
      "}\n"
      "enclosing();\n");

  TakeHeapSnapshot();

  DirectHandle<JSFunction> enclosing_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), funcs, 0).ToHandleChecked());
  DirectHandle<JSFunction> middle_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), funcs, 1).ToHandleChecked());
  DirectHandle<JSFunction> sibling_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), funcs, 2).ToHandleChecked());
  DirectHandle<JSFunction> inner_fn = Cast<JSFunction>(
      JSReceiver::GetElement(i_isolate(), funcs, 3).ToHandleChecked());

  const SnapshotSourceScopeData* enclosing_scope =
      GetScopeForClosure(*enclosing_fn);
  ASSERT_NE(nullptr, enclosing_scope);

  // Both enclosing and middle contain or have inner scope calling eval,
  // so variable emission must be disabled for both.
  EXPECT_EQ(0u, enclosing_scope->variables.size());
  EXPECT_EQ(nullptr, enclosing_scope->FindVariable("outerVar"));

  const SnapshotSourceScopeData* middle_scope = GetScopeForClosure(*middle_fn);
  ASSERT_NE(nullptr, middle_scope);
  EXPECT_EQ(enclosing_scope, middle_scope->parent);
  EXPECT_EQ(0u, middle_scope->variables.size());
  EXPECT_EQ(nullptr, middle_scope->FindVariable("middleVar"));

  // The sibling function does not call eval and none of its inner scopes
  // call eval, so its context variables must still be emitted.
  const SnapshotSourceScopeData* sibling_scope =
      GetScopeForClosure(*sibling_fn);
  ASSERT_NE(nullptr, sibling_scope);
  EXPECT_EQ(enclosing_scope, sibling_scope->parent);

  const VariableDefinition* sibling_var =
      sibling_scope->FindVariable("siblingVar");
  ASSERT_NE(nullptr, sibling_var);
  EXPECT_EQ(0, sibling_var->slot_index);

  const SnapshotSourceScopeData* inner_scope = GetScopeForClosure(*inner_fn);
  ASSERT_NE(nullptr, inner_scope);
  EXPECT_EQ(sibling_scope, inner_scope->parent);
  AssertUses(sibling_var, {inner_scope});

  CheckContextSlots(enclosing_fn);
  CheckContextSlots(middle_fn);
  CheckContextSlots(sibling_fn);
  CheckContextSlots(inner_fn);
}

TEST_F(HeapSnapshotScopesTest, WithStatement) {
  DirectHandle<JSFunction> inner_fn = RunJSForClosure(
      "function outer(obj) {\n"
      "  let outerCaptured = 1;\n"
      "  let outerUnused = 2;\n"
      "  with (obj) {\n"
      "    return function inner() {\n"
      "      return outerCaptured;\n"
      "    };\n"
      "  }\n"
      "}\n"
      "outer({a: 10});\n");

  TakeHeapSnapshot();

  const SnapshotSourceScopeData* inner_scope = GetScopeForClosure(*inner_fn);
  ASSERT_NE(nullptr, inner_scope);

  const SnapshotSourceScopeData* with_scope = inner_scope->parent;
  ASSERT_NE(nullptr, with_scope);
  EXPECT_TRUE(with_scope->variables.empty());

  const SnapshotSourceScopeData* outer_scope = with_scope->parent;
  ASSERT_NE(nullptr, outer_scope);

  const VariableDefinition* outer_captured =
      outer_scope->FindVariable("outerCaptured");
  ASSERT_NE(nullptr, outer_captured);
  EXPECT_EQ(0, outer_captured->slot_index);
  EXPECT_EQ(nullptr, outer_scope->FindVariable("outerUnused"));

  // The potential use of outerCaptured should be reported.
  AssertUses(outer_captured, {inner_scope});

  CheckContextSlots(inner_fn);
}

}  // namespace v8::internal
