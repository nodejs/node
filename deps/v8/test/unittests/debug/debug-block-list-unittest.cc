// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-block-list.h"

#include "include/v8-function.h"
#include "src/api/api-inl.h"
#include "src/debug/debug-scope-info.h"
#include "src/execution/isolate-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/scope-info-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/string-set-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

class DebugBlockListTest : public TestWithNativeContext {
 public:
  DirectHandle<SharedFunctionInfo> GetSharedFunctionInfo(const char* name) {
    DirectHandle<Object> val = RunJS(name);
    DirectHandle<JSFunction> func = Cast<JSFunction>(val);
    return direct_handle(func->shared(), isolate());
  }

  bool HasInBlockList(DirectHandle<StringSet> blocklist, const char* name) {
    DirectHandle<String> str =
        isolate()->factory()->InternalizeUtf8String(name);
    return blocklist->Has(isolate(), str);
  }
};

TEST_F(DebugBlockListTest, TopLevelFunctionCollectsOwnStackLocals) {
  RunJS(R"(
    function f(a) {
      let local = 1;
      return a + local;
    }
    function empty() {}
    var arrow = () => {};
  )");

  DirectHandle<SharedFunctionInfo> sfi = GetSharedFunctionInfo("f");
  Handle<StringSet> blocklist = EnsureLocalsBlockList(isolate(), sfi);
  EXPECT_EQ(blocklist->NumberOfElements(), 3u);
  EXPECT_TRUE(HasInBlockList(blocklist, "a"));
  EXPECT_TRUE(HasInBlockList(blocklist, "local"));
  EXPECT_TRUE(HasInBlockList(blocklist, "this"));

  DirectHandle<SharedFunctionInfo> empty_sfi = GetSharedFunctionInfo("empty");
  Handle<StringSet> empty_bl = EnsureLocalsBlockList(isolate(), empty_sfi);
  EXPECT_EQ(empty_bl->NumberOfElements(), 1u);
  EXPECT_TRUE(HasInBlockList(empty_bl, "this"));

  DirectHandle<SharedFunctionInfo> arrow_sfi = GetSharedFunctionInfo("arrow");
  Handle<StringSet> arrow_bl = EnsureLocalsBlockList(isolate(), arrow_sfi);
  EXPECT_EQ(arrow_bl->NumberOfElements(), 0u);
  EXPECT_FALSE(HasInBlockList(arrow_bl, "this"));
}

TEST_F(DebugBlockListTest, CollectsStackAllocatedLocalsAndParametersFromOuter) {
  RunJS(R"(
    function outer(param_stack, param_ctx) {
      let stack_let = 1;
      var stack_var = 2;
      let ctx_let = 3;
      return function inner(inner_stack) {
        return param_ctx + ctx_let + inner_stack;
      };
    }
    var inner_fn = outer(10, 20);
  )");

  DirectHandle<SharedFunctionInfo> inner_sfi =
      GetSharedFunctionInfo("inner_fn");
  Handle<StringSet> inner_bl = EnsureLocalsBlockList(isolate(), inner_sfi);
  EXPECT_TRUE(HasInBlockList(inner_bl, "inner_stack"));
  EXPECT_FALSE(HasInBlockList(inner_bl, "param_stack"));

  DirectHandle<ScopeInfo> outer_scope_info(
      inner_sfi->scope_info()->OuterScopeInfo(), isolate());
  Tagged<UnionOf<TheHole, StringSet>> maybe_outer_bl =
      isolate()->LocalsBlockListCacheGet(outer_scope_info);
  ASSERT_TRUE(IsStringSet(maybe_outer_bl));
  DirectHandle<StringSet> outer_bl(Cast<StringSet>(maybe_outer_bl), isolate());

  // Stack-allocated outer variables and parameters must be in outer's
  // blocklist.
  EXPECT_TRUE(HasInBlockList(outer_bl, "param_stack"));
  EXPECT_TRUE(HasInBlockList(outer_bl, "stack_let"));
  EXPECT_TRUE(HasInBlockList(outer_bl, "stack_var"));

  // Context-allocated outer variables and parameters must not be in the
  // blocklist.
  EXPECT_FALSE(HasInBlockList(outer_bl, "param_ctx"));
  EXPECT_FALSE(HasInBlockList(outer_bl, "ctx_let"));
}

TEST_F(DebugBlockListTest, SplitsBlockListsAcrossMultipleContextBoundaries) {
  RunJS(R"(
    function grandOuter(go_stack, go_ctx) {
      let go_local = 1;
      return function middle(mid_stack, mid_ctx) {
        let mid_local = 2;
        {
          let block_stack = 3;
          let block_ctx = 4;
          return function inner(in_stack) {
            return go_ctx + mid_ctx + block_ctx + in_stack;
          };
        }
      };
    }
    var inner_fn = grandOuter(1, 2)(3, 4);
  )");

  DirectHandle<SharedFunctionInfo> inner_sfi =
      GetSharedFunctionInfo("inner_fn");
  Handle<StringSet> inner_blocklist =
      EnsureLocalsBlockList(isolate(), inner_sfi);

  // 1. `inner`'s blocklist covers [inner, block) -> `in_stack`.
  EXPECT_TRUE(HasInBlockList(inner_blocklist, "in_stack"));
  EXPECT_FALSE(HasInBlockList(inner_blocklist, "block_stack"));
  EXPECT_FALSE(HasInBlockList(inner_blocklist, "block_ctx"));

  // 2. `block`'s ScopeInfo is `inner->scope_info()->OuterScopeInfo()`.
  // Its blocklist covers [block, middle) -> `block_stack`.
  DirectHandle<ScopeInfo> block_scope_info(
      inner_sfi->scope_info()->OuterScopeInfo(), isolate());
  Tagged<UnionOf<TheHole, StringSet>> maybe_block_bl =
      isolate()->LocalsBlockListCacheGet(block_scope_info);
  ASSERT_TRUE(IsStringSet(maybe_block_bl));
  DirectHandle<StringSet> block_bl(Cast<StringSet>(maybe_block_bl), isolate());
  EXPECT_TRUE(HasInBlockList(block_bl, "block_stack"));
  EXPECT_FALSE(HasInBlockList(block_bl, "block_ctx"));
  EXPECT_FALSE(HasInBlockList(block_bl, "mid_stack"));

  // 3. `middle`'s ScopeInfo is `block_scope_info->OuterScopeInfo()`.
  // Its blocklist covers [middle, grandOuter) -> `mid_stack` and `mid_local`.
  DirectHandle<ScopeInfo> mid_scope_info(block_scope_info->OuterScopeInfo(),
                                         isolate());
  Tagged<UnionOf<TheHole, StringSet>> maybe_mid_bl =
      isolate()->LocalsBlockListCacheGet(mid_scope_info);
  ASSERT_TRUE(IsStringSet(maybe_mid_bl));
  DirectHandle<StringSet> mid_bl(Cast<StringSet>(maybe_mid_bl), isolate());
  EXPECT_TRUE(HasInBlockList(mid_bl, "mid_stack"));
  EXPECT_TRUE(HasInBlockList(mid_bl, "mid_local"));
  EXPECT_FALSE(HasInBlockList(mid_bl, "mid_ctx"));
  EXPECT_FALSE(HasInBlockList(mid_bl, "go_stack"));

  // 4. `grandOuter`'s ScopeInfo is `mid_scope_info->OuterScopeInfo()`.
  // Its blocklist covers [grandOuter, script) -> `go_stack` and `go_local`.
  DirectHandle<ScopeInfo> go_scope_info(mid_scope_info->OuterScopeInfo(),
                                        isolate());
  Tagged<UnionOf<TheHole, StringSet>> maybe_go_bl =
      isolate()->LocalsBlockListCacheGet(go_scope_info);
  ASSERT_TRUE(IsStringSet(maybe_go_bl));
  DirectHandle<StringSet> go_bl(Cast<StringSet>(maybe_go_bl), isolate());
  EXPECT_TRUE(HasInBlockList(go_bl, "go_stack"));
  EXPECT_TRUE(HasInBlockList(go_bl, "go_local"));
  EXPECT_FALSE(HasInBlockList(go_bl, "go_ctx"));
}

TEST_F(DebugBlockListTest, AccumulatesThroughNonContextAllocatingScopes) {
  RunJS(R"(
    function outer(outer_ctx) {
      let outer_stack = 1;
      function middleNoContext(mid_stack) {
        let mid_local = 2;
        {
          let block_stack = 3;
          return function inner(in_stack) {
            return outer_ctx + in_stack;
          };
        }
      }
      return middleNoContext(10);
    }
    var inner_fn = outer(42);
  )");

  DirectHandle<SharedFunctionInfo> inner_sfi =
      GetSharedFunctionInfo("inner_fn");
  Handle<StringSet> inner_blocklist =
      EnsureLocalsBlockList(isolate(), inner_sfi);

  // Neither `block` nor `middleNoContext` allocates a context, so `inner`'s
  // blocklist [inner, outer) accumulates stack variables from `inner`,
  // `block`, and `middleNoContext`.
  EXPECT_TRUE(HasInBlockList(inner_blocklist, "in_stack"));
  EXPECT_TRUE(HasInBlockList(inner_blocklist, "block_stack"));
  EXPECT_TRUE(HasInBlockList(inner_blocklist, "mid_stack"));
  EXPECT_TRUE(HasInBlockList(inner_blocklist, "mid_local"));
  EXPECT_FALSE(HasInBlockList(inner_blocklist, "outer_stack"));
  EXPECT_FALSE(HasInBlockList(inner_blocklist, "outer_ctx"));

  // `outer`'s blocklist [outer, script) contains `outer_stack`.
  DirectHandle<ScopeInfo> outer_scope_info(
      inner_sfi->scope_info()->OuterScopeInfo(), isolate());
  Tagged<UnionOf<TheHole, StringSet>> maybe_outer_bl =
      isolate()->LocalsBlockListCacheGet(outer_scope_info);
  ASSERT_TRUE(IsStringSet(maybe_outer_bl));
  DirectHandle<StringSet> outer_bl(Cast<StringSet>(maybe_outer_bl), isolate());
  EXPECT_TRUE(HasInBlockList(outer_bl, "outer_stack"));
  EXPECT_FALSE(HasInBlockList(outer_bl, "outer_ctx"));
}

TEST_F(DebugBlockListTest, CachesResultOnSubsequentCalls) {
  RunJS(R"(
    function outer(a) {
      let b = 1;
      return function inner() { return a; };
    }
    var inner_fn = outer(1);
  )");

  DirectHandle<SharedFunctionInfo> inner_sfi =
      GetSharedFunctionInfo("inner_fn");
  Handle<StringSet> first = EnsureLocalsBlockList(isolate(), inner_sfi);
  Handle<StringSet> second = EnsureLocalsBlockList(isolate(), inner_sfi);
  EXPECT_EQ(*first, *second);
}

TEST_F(DebugBlockListTest, SiblingInnerBlockNotIncludedInBlockLists) {
  RunJS(R"(
    function f() {
      let a = 42;
      let b = 21;
      () => b;
      {
        const c = 'foo';
      }
      return function g() {};
    }
    var g_fn = f();
  )");

  DirectHandle<SharedFunctionInfo> g_sfi = GetSharedFunctionInfo("g_fn");
  Handle<StringSet> g_bl = EnsureLocalsBlockList(isolate(), g_sfi);
  EXPECT_FALSE(HasInBlockList(g_bl, "a"));
  EXPECT_FALSE(HasInBlockList(g_bl, "b"));
  EXPECT_FALSE(HasInBlockList(g_bl, "c"));

  DirectHandle<ScopeInfo> f_scope_info(g_sfi->scope_info()->OuterScopeInfo(),
                                       isolate());
  Tagged<UnionOf<TheHole, StringSet>> maybe_f_bl =
      isolate()->LocalsBlockListCacheGet(f_scope_info);
  ASSERT_TRUE(IsStringSet(maybe_f_bl));
  DirectHandle<StringSet> f_bl(Cast<StringSet>(maybe_f_bl), isolate());
  EXPECT_TRUE(HasInBlockList(f_bl, "a"));
  EXPECT_FALSE(HasInBlockList(f_bl, "b"));
  EXPECT_FALSE(HasInBlockList(f_bl, "c"));
}

TEST_F(
    DebugBlockListTest,
    EnclosingInnerBlockSeparatedFromOuterFunctionAndCalculateScopeBlockList) {
  RunJS(R"(
    function f() {
      let a = 42;
      let b = 21;
      () => b;
      {
        const c = 'foo';
        return function g() {};
      }
    }
    var g_fn = f();
  )");

  DirectHandle<SharedFunctionInfo> g_sfi = GetSharedFunctionInfo("g_fn");
  Handle<StringSet> g_bl = EnsureLocalsBlockList(isolate(), g_sfi);

  // `g`'s blocklist [g, f) contains `c` from the enclosing inner block,
  // but not `a` or `b` from `f`.
  EXPECT_TRUE(HasInBlockList(g_bl, "c"));
  EXPECT_FALSE(HasInBlockList(g_bl, "a"));
  EXPECT_FALSE(HasInBlockList(g_bl, "b"));

  // `f`'s blocklist [f, script) contains `a`, and must NOT contain `c` from
  // the inner block.
  DirectHandle<ScopeInfo> f_scope_info(g_sfi->scope_info()->OuterScopeInfo(),
                                       isolate());
  Tagged<UnionOf<TheHole, StringSet>> maybe_f_bl =
      isolate()->LocalsBlockListCacheGet(f_scope_info);
  ASSERT_TRUE(IsStringSet(maybe_f_bl));
  DirectHandle<StringSet> f_bl(Cast<StringSet>(maybe_f_bl), isolate());
  EXPECT_TRUE(HasInBlockList(f_bl, "a"));
  EXPECT_FALSE(HasInBlockList(f_bl, "b"));
  EXPECT_FALSE(HasInBlockList(f_bl, "c"));

  // Verify `CalculateScopeBlockList` on the inner block scope directly.
  DirectHandle<Script> script(Cast<Script>(g_sfi->script()), isolate());
  Handle<DebugScriptScopeInfo> debug_scope_info =
      EnsureDebugScriptScopeInfo(isolate(), script);
  std::optional<DebugScriptScope> g_scope =
      FindClosureScope(debug_scope_info, g_sfi->StartPosition(),
                       g_sfi->EndPosition(), FUNCTION_SCOPE);
  ASSERT_TRUE(g_scope.has_value());
  std::optional<DebugScriptScope> inner_block_scope = g_scope->parent();
  ASSERT_TRUE(inner_block_scope.has_value());
  EXPECT_FALSE(inner_block_scope->needs_context());

  Handle<StringSet> block_bl =
      CalculateScopeBlockList(isolate(), *inner_block_scope);
  EXPECT_TRUE(HasInBlockList(block_bl, "c"));
  EXPECT_FALSE(HasInBlockList(block_bl, "a"));
  EXPECT_FALSE(HasInBlockList(block_bl, "b"));
}

}  // namespace internal
}  // namespace v8
