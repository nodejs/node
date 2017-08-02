// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/scopes.h"
#include "src/compilation-info.h"
#include "src/compiler/ast-loop-assignment-analyzer.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/parsing/rewriter.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {
const int kBufferSize = 1024;

struct TestHelper : public HandleAndZoneScope {
  Handle<JSFunction> function;
  LoopAssignmentAnalysis* result;

  explicit TestHelper(const char* body)
      : function(Handle<JSFunction>::null()), result(NULL) {
    ScopedVector<char> program(kBufferSize);
    SNPrintF(program, "function f(a,b,c) { %s; } f;", body);
    v8::Local<v8::Value> v = CompileRun(program.start());
    Handle<Object> obj = v8::Utils::OpenHandle(*v);
    function = Handle<JSFunction>::cast(obj);
  }

  void CheckLoopAssignedCount(int expected, const char* var_name) {
    // TODO(titzer): don't scope analyze every single time.
    ParseInfo parse_info(handle(function->shared()));
    CompilationInfo info(parse_info.zone(), &parse_info, function->GetIsolate(),
                         function);

    CHECK(parsing::ParseFunction(&parse_info, info.isolate()));
    CHECK(Rewriter::Rewrite(&parse_info, function->GetIsolate()));
    DeclarationScope::Analyze(&parse_info, info.isolate(),
                              AnalyzeMode::kRegular);

    DeclarationScope* scope = info.literal()->scope();
    AstValueFactory* factory = parse_info.ast_value_factory();
    CHECK(scope);

    if (result == NULL) {
      AstLoopAssignmentAnalyzer analyzer(main_zone(), &info);
      result = analyzer.Analyze();
      CHECK(result);
    }

    const i::AstRawString* name = factory->GetOneByteString(var_name);

    i::Variable* var = scope->Lookup(name);
    CHECK(var);

    if (var->location() == VariableLocation::UNALLOCATED) {
      CHECK_EQ(0, expected);
    } else {
      CHECK(var->IsStackAllocated());
      CHECK_EQ(expected, result->GetAssignmentCountForTesting(scope, var));
    }
  }
};
}  // namespace


TEST(SimpleLoop1) {
  TestHelper f("var x = 0; while (x) ;");

  f.CheckLoopAssignedCount(0, "x");
}

TEST(ForIn1) {
  const char* loops[] = {"for(x in 0) { }"};

  for (size_t i = 0; i < arraysize(loops); i++) {
    TestHelper f(loops[i]);
    f.CheckLoopAssignedCount(0, "x");
  }
}


TEST(Param1) {
  TestHelper f("while (1) a = 0;");

  f.CheckLoopAssignedCount(1, "a");
  f.CheckLoopAssignedCount(0, "b");
  f.CheckLoopAssignedCount(0, "c");
}


TEST(Param2) {
  TestHelper f("for (;;) b = 0;");

  f.CheckLoopAssignedCount(0, "a");
  f.CheckLoopAssignedCount(1, "b");
  f.CheckLoopAssignedCount(0, "c");
}


TEST(Param2b) {
  TestHelper f("a; b; c; for (;;) b = 0;");

  f.CheckLoopAssignedCount(0, "a");
  f.CheckLoopAssignedCount(1, "b");
  f.CheckLoopAssignedCount(0, "c");
}


TEST(Param3) {
  TestHelper f("for(x in 0) c = 0;");

  f.CheckLoopAssignedCount(0, "a");
  f.CheckLoopAssignedCount(0, "b");
  f.CheckLoopAssignedCount(1, "c");
}


TEST(Param3b) {
  TestHelper f("a; b; c; for(x in 0) c = 0;");

  f.CheckLoopAssignedCount(0, "a");
  f.CheckLoopAssignedCount(0, "b");
  f.CheckLoopAssignedCount(1, "c");
}


TEST(NestedLoop1) {
  TestHelper f("while (x) { while (x) { var x = 0; } }");

  f.CheckLoopAssignedCount(2, "x");
}


TEST(NestedLoop2) {
  TestHelper f("while (0) { while (0) { var x = 0; } }");

  f.CheckLoopAssignedCount(2, "x");
}


TEST(NestedLoop3) {
  TestHelper f("while (0) { var y = 1; while (0) { var x = 0; } }");

  f.CheckLoopAssignedCount(2, "x");
  f.CheckLoopAssignedCount(1, "y");
}


TEST(NestedInc1) {
  const char* loops[] = {
      "while (1) a(b++);",
      "while (1) a(0, b++);",
      "while (1) a(0, 0, b++);",
      "while (1) a(b++, 1, 1);",
      "while (1) a(++b);",
      "while (1) a + (b++);",
      "while (1) (b++) + a;",
      "while (1) a + c(b++);",
      "while (1) throw b++;",
      "while (1) switch (b++) {} ;",
      "while (1) switch (a) {case (b++): 0; } ;",
      "while (1) switch (a) {case b: b++; } ;",
      "while (1) a == (b++);",
      "while (1) a === (b++);",
      "while (1) +(b++);",
      "while (1) ~(b++);",
      "while (1) new a(b++);",
      "while (1) (b++).f;",
      "while (1) a[b++];",
      "while (1) (b++)();",
      "while (1) [b++];",
      "while (1) [0,b++];",
      "while (1) var y = [11,b++,12];",
      "while (1) var y = {f:11,g:(b++),h:12};",
      "while (1) try {b++;} finally {};",
      "while (1) try {} finally {b++};",
      "while (1) try {b++;} catch (e) {};",
      "while (1) try {} catch (e) {b++};",
      "while (1) return b++;",
      "while (1) (b++) ? b : b;",
      "while (1) b ? (b++) : b;",
      "while (1) b ? b : (b++);",
  };

  for (size_t i = 0; i < arraysize(loops); i++) {
    TestHelper f(loops[i]);
    f.CheckLoopAssignedCount(1, "b");
  }
}


TEST(NestedAssign1) {
  const char* loops[] = {
      "while (1) a(b=1);",
      "while (1) a(0, b=1);",
      "while (1) a(0, 0, b=1);",
      "while (1) a(b=1, 1, 1);",
      "while (1) a + (b=1);",
      "while (1) (b=1) + a;",
      "while (1) a + c(b=1);",
      "while (1) throw b=1;",
      "while (1) switch (b=1) {} ;",
      "while (1) switch (a) {case b=1: 0; } ;",
      "while (1) switch (a) {case b: b=1; } ;",
      "while (1) a == (b=1);",
      "while (1) a === (b=1);",
      "while (1) +(b=1);",
      "while (1) ~(b=1);",
      "while (1) new a(b=1);",
      "while (1) (b=1).f;",
      "while (1) a[b=1];",
      "while (1) (b=1)();",
      "while (1) [b=1];",
      "while (1) [0,b=1];",
      "while (1) var z = [11,b=1,12];",
      "while (1) var y = {f:11,g:(b=1),h:12};",
      "while (1) try {b=1;} finally {};",
      "while (1) try {} finally {b=1};",
      "while (1) try {b=1;} catch (e) {};",
      "while (1) try {} catch (e) {b=1};",
      "while (1) return b=1;",
      "while (1) (b=1) ? b : b;",
      "while (1) b ? (b=1) : b;",
      "while (1) b ? b : (b=1);",
  };

  for (size_t i = 0; i < arraysize(loops); i++) {
    TestHelper f(loops[i]);
    f.CheckLoopAssignedCount(1, "b");
  }
}


TEST(NestedLoops3) {
  TestHelper f("var x, y, z, w; while (x++) while (y++) while (z++) ; w;");

  f.CheckLoopAssignedCount(1, "x");
  f.CheckLoopAssignedCount(2, "y");
  f.CheckLoopAssignedCount(3, "z");
  f.CheckLoopAssignedCount(0, "w");
}


TEST(NestedLoops3b) {
  TestHelper f(
      "var x, y, z, w;"
      "while (1) { x=1; while (1) { y=1; while (1) z=1; } }"
      "w;");

  f.CheckLoopAssignedCount(1, "x");
  f.CheckLoopAssignedCount(2, "y");
  f.CheckLoopAssignedCount(3, "z");
  f.CheckLoopAssignedCount(0, "w");
}


TEST(NestedLoops3c) {
  TestHelper f(
      "var x, y, z, w;"
      "while (1) {"
      "  x++;"
      "  while (1) {"
      "    y++;"
      "    while (1) z++;"
      "  }"
      "  while (1) {"
      "    y++;"
      "    while (1) z++;"
      "  }"
      "}"
      "w;");

  f.CheckLoopAssignedCount(1, "x");
  f.CheckLoopAssignedCount(3, "y");
  f.CheckLoopAssignedCount(5, "z");
  f.CheckLoopAssignedCount(0, "w");
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
