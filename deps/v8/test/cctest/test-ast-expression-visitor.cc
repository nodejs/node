// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <map>

#include "src/v8.h"

#include "src/ast/ast.h"
#include "src/ast/ast-expression-visitor.h"
#include "src/ast/scopes.h"
#include "src/parsing/parser.h"
#include "src/parsing/rewriter.h"
#include "test/cctest/cctest.h"
#include "test/cctest/expression-type-collector.h"
#include "test/cctest/expression-type-collector-macros.h"

using namespace v8::internal;

namespace {

class NodeTypeCounter : public AstExpressionVisitor {
 public:
  typedef std::map<AstNode::NodeType, int> Counters;

  NodeTypeCounter(Isolate* isolate, Expression* expr, Counters* counts)
      : AstExpressionVisitor(isolate, expr), counts_(counts) {}

 protected:
  void VisitExpression(Expression* expr) override {
    (*counts_)[expr->node_type()]++;
  }

 private:
  Counters* counts_;
};

}  // namespace

TEST(VisitExpression) {
  const char test_function[] =
      "function GeometricMean(stdlib, foreign, buffer) {\n"
      "  \"use asm\";\n"
      "\n"
      "  var exp = stdlib.Math.exp;\n"
      "  var log = stdlib.Math.log;\n"
      "  var values = new stdlib.Float64Array(buffer);\n"
      "\n"
      "  function logSum(start, end) {\n"
      "    start = start|0;\n"
      "    end = end|0;\n"
      "\n"
      "    var sum = 0.0, p = 0, q = 0;\n"
      "\n"
      "    // asm.js forces byte addressing of the heap by requiring shifting "
      "by 3\n"
      "    for (p = start << 3, q = end << 3; (p|0) < (q|0); p = (p + 8)|0) {\n"
      "      sum = sum + +log(values[p>>3]);\n"
      "    }\n"
      "\n"
      "    return +sum;\n"
      "  }\n"
      "\n"
      " function geometricMean(start, end) {\n"
      "    start = start|0;\n"
      "    end = end|0;\n"
      "\n"
      "    return +exp(+logSum(start, end) / +((end - start)|0));\n"
      "  }\n"
      "\n"
      "  return { geometricMean: geometricMean };\n"
      "}\n";

  // Parse + compile test_function, and extract the AST node for it.
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  i::Isolate* isolate = CcTest::i_isolate();
  i::Handle<i::String> source_code =
      isolate->factory()
          ->NewStringFromUtf8(i::CStrVector(test_function))
          .ToHandleChecked();
  i::Handle<i::Script> script = isolate->factory()->NewScript(source_code);
  i::ParseInfo info(handles.main_zone(), script);
  i::Parser parser(&info);
  info.set_global();
  info.set_lazy(false);
  info.set_allow_lazy_parsing(false);
  info.set_toplevel(true);
  CHECK(i::Compiler::ParseAndAnalyze(&info));
  Expression* test_function_expr =
      info.scope()->declarations()->at(0)->AsFunctionDeclaration()->fun();

  // Run NodeTypeCounter and sanity check counts for 3 expression types,
  // and for overall # of types found.
  NodeTypeCounter::Counters counts;
  NodeTypeCounter(isolate, test_function_expr, &counts).Run();
  CHECK_EQ(21, counts[AstNode::kBinaryOperation]);
  CHECK_EQ(26, counts[AstNode::kLiteral]);
  CHECK_EQ(3, counts[AstNode::kFunctionLiteral]);
  CHECK_EQ(10, counts.size());
}
