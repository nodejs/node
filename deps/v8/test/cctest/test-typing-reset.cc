// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"

#include "src/ast/ast.h"
#include "src/ast/ast-expression-visitor.h"
#include "src/ast/scopes.h"
#include "src/parsing/parser.h"
#include "src/parsing/rewriter.h"
#include "src/typing-reset.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/function-tester.h"
#include "test/cctest/expression-type-collector.h"
#include "test/cctest/expression-type-collector-macros.h"

#define INT32_TYPE Bounds(Type::Signed32(), Type::Signed32())

using namespace v8::internal;

namespace {

class TypeSetter : public AstExpressionVisitor {
 public:
  TypeSetter(Isolate* isolate, FunctionLiteral* root)
      : AstExpressionVisitor(isolate, root) {}

 protected:
  void VisitExpression(Expression* expression) {
    expression->set_bounds(INT32_TYPE);
  }
};


void CheckAllSame(ZoneVector<ExpressionTypeEntry>& types,
                  Bounds expected_type) {
  CHECK_TYPES_BEGIN {
    // function logSum
    CHECK_EXPR(FunctionLiteral, expected_type) {
      CHECK_EXPR(FunctionLiteral, expected_type) {
        CHECK_EXPR(Assignment, expected_type) {
          CHECK_VAR(start, expected_type);
          CHECK_EXPR(BinaryOperation, expected_type) {
            CHECK_VAR(start, expected_type);
            CHECK_EXPR(Literal, expected_type);
          }
        }
        CHECK_EXPR(Assignment, expected_type) {
          CHECK_VAR(end, expected_type);
          CHECK_EXPR(BinaryOperation, expected_type) {
            CHECK_VAR(end, expected_type);
            CHECK_EXPR(Literal, expected_type);
          }
        }
        CHECK_EXPR(Assignment, expected_type) {
          CHECK_VAR(sum, expected_type);
          CHECK_EXPR(Literal, expected_type);
        }
        CHECK_EXPR(Assignment, expected_type) {
          CHECK_VAR(p, expected_type);
          CHECK_EXPR(Literal, expected_type);
        }
        CHECK_EXPR(Assignment, expected_type) {
          CHECK_VAR(q, expected_type);
          CHECK_EXPR(Literal, expected_type);
        }
        // for (p = start << 3, q = end << 3;
        CHECK_EXPR(BinaryOperation, expected_type) {
          CHECK_EXPR(Assignment, expected_type) {
            CHECK_VAR(p, expected_type);
            CHECK_EXPR(BinaryOperation, expected_type) {
              CHECK_VAR(start, expected_type);
              CHECK_EXPR(Literal, expected_type);
            }
          }
          CHECK_EXPR(Assignment, expected_type) {
            CHECK_VAR(q, expected_type);
            CHECK_EXPR(BinaryOperation, expected_type) {
              CHECK_VAR(end, expected_type);
              CHECK_EXPR(Literal, expected_type);
            }
          }
        }
        // (p|0) < (q|0);
        CHECK_EXPR(CompareOperation, expected_type) {
          CHECK_EXPR(BinaryOperation, expected_type) {
            CHECK_VAR(p, expected_type);
            CHECK_EXPR(Literal, expected_type);
          }
          CHECK_EXPR(BinaryOperation, expected_type) {
            CHECK_VAR(q, expected_type);
            CHECK_EXPR(Literal, expected_type);
          }
        }
        // p = (p + 8)|0) {\n"
        CHECK_EXPR(Assignment, expected_type) {
          CHECK_VAR(p, expected_type);
          CHECK_EXPR(BinaryOperation, expected_type) {
            CHECK_EXPR(BinaryOperation, expected_type) {
              CHECK_VAR(p, expected_type);
              CHECK_EXPR(Literal, expected_type);
            }
            CHECK_EXPR(Literal, expected_type);
          }
        }
        // sum = sum + +log(values[p>>3]);
        CHECK_EXPR(Assignment, expected_type) {
          CHECK_VAR(sum, expected_type);
          CHECK_EXPR(BinaryOperation, expected_type) {
            CHECK_VAR(sum, expected_type);
            CHECK_EXPR(BinaryOperation, expected_type) {
              CHECK_EXPR(Call, expected_type) {
                CHECK_VAR(log, expected_type);
                CHECK_EXPR(Property, expected_type) {
                  CHECK_VAR(values, expected_type);
                  CHECK_EXPR(BinaryOperation, expected_type) {
                    CHECK_VAR(p, expected_type);
                    CHECK_EXPR(Literal, expected_type);
                  }
                }
              }
              CHECK_EXPR(Literal, expected_type);
            }
          }
        }
        // return +sum;
        CHECK_EXPR(BinaryOperation, expected_type) {
          CHECK_VAR(sum, expected_type);
          CHECK_EXPR(Literal, expected_type);
        }
      }
      // function geometricMean
      CHECK_EXPR(FunctionLiteral, expected_type) {
        CHECK_EXPR(Assignment, expected_type) {
          CHECK_VAR(start, expected_type);
          CHECK_EXPR(BinaryOperation, expected_type) {
            CHECK_VAR(start, expected_type);
            CHECK_EXPR(Literal, expected_type);
          }
        }
        CHECK_EXPR(Assignment, expected_type) {
          CHECK_VAR(end, expected_type);
          CHECK_EXPR(BinaryOperation, expected_type) {
            CHECK_VAR(end, expected_type);
            CHECK_EXPR(Literal, expected_type);
          }
        }
        // return +exp(+logSum(start, end) / +((end - start)|0));
        CHECK_EXPR(BinaryOperation, expected_type) {
          CHECK_EXPR(Call, expected_type) {
            CHECK_VAR(exp, expected_type);
            CHECK_EXPR(BinaryOperation, expected_type) {
              CHECK_EXPR(BinaryOperation, expected_type) {
                CHECK_EXPR(Call, expected_type) {
                  CHECK_VAR(logSum, expected_type);
                  CHECK_VAR(start, expected_type);
                  CHECK_VAR(end, expected_type);
                }
                CHECK_EXPR(Literal, expected_type);
              }
              CHECK_EXPR(BinaryOperation, expected_type) {
                CHECK_EXPR(BinaryOperation, expected_type) {
                  CHECK_EXPR(BinaryOperation, expected_type) {
                    CHECK_VAR(end, expected_type);
                    CHECK_VAR(start, expected_type);
                  }
                  CHECK_EXPR(Literal, expected_type);
                }
                CHECK_EXPR(Literal, expected_type);
              }
            }
          }
          CHECK_EXPR(Literal, expected_type);
        }
      }
      // "use asm";
      CHECK_EXPR(Literal, expected_type);
      // var exp = stdlib.Math.exp;
      CHECK_EXPR(Assignment, expected_type) {
        CHECK_VAR(exp, expected_type);
        CHECK_EXPR(Property, expected_type) {
          CHECK_EXPR(Property, expected_type) {
            CHECK_VAR(stdlib, expected_type);
            CHECK_EXPR(Literal, expected_type);
          }
          CHECK_EXPR(Literal, expected_type);
        }
      }
      // var log = stdlib.Math.log;
      CHECK_EXPR(Assignment, expected_type) {
        CHECK_VAR(log, expected_type);
        CHECK_EXPR(Property, expected_type) {
          CHECK_EXPR(Property, expected_type) {
            CHECK_VAR(stdlib, expected_type);
            CHECK_EXPR(Literal, expected_type);
          }
          CHECK_EXPR(Literal, expected_type);
        }
      }
      // var values = new stdlib.Float64Array(buffer);
      CHECK_EXPR(Assignment, expected_type) {
        CHECK_VAR(values, expected_type);
        CHECK_EXPR(CallNew, expected_type) {
          CHECK_EXPR(Property, expected_type) {
            CHECK_VAR(stdlib, expected_type);
            CHECK_EXPR(Literal, expected_type);
          }
          CHECK_VAR(buffer, expected_type);
        }
      }
      // return { geometricMean: geometricMean };
      CHECK_EXPR(ObjectLiteral, expected_type) {
        CHECK_VAR(geometricMean, expected_type);
      }
    }
  }
  CHECK_TYPES_END
}

}  // namespace


TEST(ResetTypingInfo) {
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

  v8::V8::Initialize();
  HandleAndZoneScope handles;

  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();

  i::Handle<i::String> source_code =
      factory->NewStringFromUtf8(i::CStrVector(test_function))
          .ToHandleChecked();

  i::Handle<i::Script> script = factory->NewScript(source_code);

  i::ParseInfo info(handles.main_zone(), script);
  i::Parser parser(&info);
  parser.set_allow_harmony_sloppy(true);
  info.set_global();
  info.set_lazy(false);
  info.set_allow_lazy_parsing(false);
  info.set_toplevel(true);

  CHECK(i::Compiler::ParseAndAnalyze(&info));
  FunctionLiteral* root =
      info.scope()->declarations()->at(0)->AsFunctionDeclaration()->fun();

  // Core of the test.
  ZoneVector<ExpressionTypeEntry> types(handles.main_zone());
  ExpressionTypeCollector(isolate, root, &types).Run();
  CheckAllSame(types, Bounds::Unbounded());

  TypeSetter(isolate, root).Run();

  ExpressionTypeCollector(isolate, root, &types).Run();
  CheckAllSame(types, INT32_TYPE);

  TypingReseter(isolate, root).Run();

  ExpressionTypeCollector(isolate, root, &types).Run();
  CheckAllSame(types, Bounds::Unbounded());
}
