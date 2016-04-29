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
#include "test/cctest/cctest.h"
#include "test/cctest/expression-type-collector.h"
#include "test/cctest/expression-type-collector-macros.h"

using namespace v8::internal;

namespace {

static void CollectTypes(HandleAndZoneScope* handles, const char* source,
                         ZoneVector<ExpressionTypeEntry>* dst) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();

  i::Handle<i::String> source_code =
      factory->NewStringFromUtf8(i::CStrVector(source)).ToHandleChecked();

  i::Handle<i::Script> script = factory->NewScript(source_code);

  i::ParseInfo info(handles->main_zone(), script);
  i::Parser parser(&info);
  parser.set_allow_harmony_sloppy(true);
  info.set_global();
  info.set_lazy(false);
  info.set_allow_lazy_parsing(false);
  info.set_toplevel(true);

  CHECK(i::Compiler::ParseAndAnalyze(&info));

  ExpressionTypeCollector(
      isolate,
      info.scope()->declarations()->at(0)->AsFunctionDeclaration()->fun(), dst)
      .Run();
}

}  // namespace


TEST(VisitExpressions) {
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  ZoneVector<ExpressionTypeEntry> types(handles.main_zone());
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

  CollectTypes(&handles, test_function, &types);
  CHECK_TYPES_BEGIN {
    // function logSum
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
      CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(start, Bounds::Unbounded());
          CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
            CHECK_VAR(start, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
        }
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(end, Bounds::Unbounded());
          CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
            CHECK_VAR(end, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
        }
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(sum, Bounds::Unbounded());
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(p, Bounds::Unbounded());
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(q, Bounds::Unbounded());
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
        // for (p = start << 3, q = end << 3;
        CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
          CHECK_EXPR(Assignment, Bounds::Unbounded()) {
            CHECK_VAR(p, Bounds::Unbounded());
            CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
              CHECK_VAR(start, Bounds::Unbounded());
              CHECK_EXPR(Literal, Bounds::Unbounded());
            }
          }
          CHECK_EXPR(Assignment, Bounds::Unbounded()) {
            CHECK_VAR(q, Bounds::Unbounded());
            CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
              CHECK_VAR(end, Bounds::Unbounded());
              CHECK_EXPR(Literal, Bounds::Unbounded());
            }
          }
        }
        // (p|0) < (q|0);
        CHECK_EXPR(CompareOperation, Bounds::Unbounded()) {
          CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
            CHECK_VAR(p, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
          CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
            CHECK_VAR(q, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
        }
        // p = (p + 8)|0) {\n"
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(p, Bounds::Unbounded());
          CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
            CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
              CHECK_VAR(p, Bounds::Unbounded());
              CHECK_EXPR(Literal, Bounds::Unbounded());
            }
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
        }
        // sum = sum + +log(values[p>>3]);
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(sum, Bounds::Unbounded());
          CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
            CHECK_VAR(sum, Bounds::Unbounded());
            CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
              CHECK_EXPR(Call, Bounds::Unbounded()) {
                CHECK_VAR(log, Bounds::Unbounded());
                CHECK_EXPR(Property, Bounds::Unbounded()) {
                  CHECK_VAR(values, Bounds::Unbounded());
                  CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
                    CHECK_VAR(p, Bounds::Unbounded());
                    CHECK_EXPR(Literal, Bounds::Unbounded());
                  }
                }
              }
              CHECK_EXPR(Literal, Bounds::Unbounded());
            }
          }
        }
        // return +sum;
        CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
          CHECK_VAR(sum, Bounds::Unbounded());
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
      }
      // function geometricMean
      CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(start, Bounds::Unbounded());
          CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
            CHECK_VAR(start, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
        }
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(end, Bounds::Unbounded());
          CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
            CHECK_VAR(end, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
        }
        // return +exp(+logSum(start, end) / +((end - start)|0));
        CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
          CHECK_EXPR(Call, Bounds::Unbounded()) {
            CHECK_VAR(exp, Bounds::Unbounded());
            CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
              CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
                CHECK_EXPR(Call, Bounds::Unbounded()) {
                  CHECK_VAR(logSum, Bounds::Unbounded());
                  CHECK_VAR(start, Bounds::Unbounded());
                  CHECK_VAR(end, Bounds::Unbounded());
                }
                CHECK_EXPR(Literal, Bounds::Unbounded());
              }
              CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
                CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
                  CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
                    CHECK_VAR(end, Bounds::Unbounded());
                    CHECK_VAR(start, Bounds::Unbounded());
                  }
                  CHECK_EXPR(Literal, Bounds::Unbounded());
                }
                CHECK_EXPR(Literal, Bounds::Unbounded());
              }
            }
          }
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
      }
      // "use asm";
      CHECK_EXPR(Literal, Bounds::Unbounded());
      // var exp = stdlib.Math.exp;
      CHECK_EXPR(Assignment, Bounds::Unbounded()) {
        CHECK_VAR(exp, Bounds::Unbounded());
        CHECK_EXPR(Property, Bounds::Unbounded()) {
          CHECK_EXPR(Property, Bounds::Unbounded()) {
            CHECK_VAR(stdlib, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
      }
      // var log = stdlib.Math.log;
      CHECK_EXPR(Assignment, Bounds::Unbounded()) {
        CHECK_VAR(log, Bounds::Unbounded());
        CHECK_EXPR(Property, Bounds::Unbounded()) {
          CHECK_EXPR(Property, Bounds::Unbounded()) {
            CHECK_VAR(stdlib, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
      }
      // var values = new stdlib.Float64Array(buffer);
      CHECK_EXPR(Assignment, Bounds::Unbounded()) {
        CHECK_VAR(values, Bounds::Unbounded());
        CHECK_EXPR(CallNew, Bounds::Unbounded()) {
          CHECK_EXPR(Property, Bounds::Unbounded()) {
            CHECK_VAR(stdlib, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
          CHECK_VAR(buffer, Bounds::Unbounded());
        }
      }
      // return { geometricMean: geometricMean };
      CHECK_EXPR(ObjectLiteral, Bounds::Unbounded()) {
        CHECK_VAR(geometricMean, Bounds::Unbounded());
      }
    }
  }
  CHECK_TYPES_END
}


TEST(VisitConditional) {
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  ZoneVector<ExpressionTypeEntry> types(handles.main_zone());
  // Check that traversing the ternary operator works.
  const char test_function[] =
      "function foo() {\n"
      "  var a, b, c;\n"
      "  var x = a ? b : c;\n"
      "}\n";
  CollectTypes(&handles, test_function, &types);
  CHECK_TYPES_BEGIN {
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
      CHECK_EXPR(Assignment, Bounds::Unbounded()) {
        CHECK_VAR(x, Bounds::Unbounded());
        CHECK_EXPR(Conditional, Bounds::Unbounded()) {
          CHECK_VAR(a, Bounds::Unbounded());
          CHECK_VAR(b, Bounds::Unbounded());
          CHECK_VAR(c, Bounds::Unbounded());
        }
      }
    }
  }
  CHECK_TYPES_END
}


TEST(VisitEmptyForStatment) {
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  ZoneVector<ExpressionTypeEntry> types(handles.main_zone());
  // Check that traversing an empty for statement works.
  const char test_function[] =
      "function foo() {\n"
      "  for (;;) {}\n"
      "}\n";
  CollectTypes(&handles, test_function, &types);
  CHECK_TYPES_BEGIN {
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {}
  }
  CHECK_TYPES_END
}


TEST(VisitSwitchStatment) {
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  ZoneVector<ExpressionTypeEntry> types(handles.main_zone());
  // Check that traversing a switch with a default works.
  const char test_function[] =
      "function foo() {\n"
      "  switch (0) { case 1: break; default: break; }\n"
      "}\n";
  CollectTypes(&handles, test_function, &types);
  CHECK_TYPES_BEGIN {
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
      CHECK_EXPR(Assignment, Bounds::Unbounded()) {
        CHECK_VAR(.switch_tag, Bounds::Unbounded());
        CHECK_EXPR(Literal, Bounds::Unbounded());
      }
      CHECK_EXPR(Literal, Bounds::Unbounded());
      CHECK_VAR(.switch_tag, Bounds::Unbounded());
      CHECK_EXPR(Literal, Bounds::Unbounded());
    }
  }
  CHECK_TYPES_END
}


TEST(VisitThrow) {
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  ZoneVector<ExpressionTypeEntry> types(handles.main_zone());
  const char test_function[] =
      "function foo() {\n"
      "  throw 123;\n"
      "}\n";
  CollectTypes(&handles, test_function, &types);
  CHECK_TYPES_BEGIN {
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
      CHECK_EXPR(Throw, Bounds::Unbounded()) {
        CHECK_EXPR(Literal, Bounds::Unbounded());
      }
    }
  }
  CHECK_TYPES_END
}


TEST(VisitYield) {
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  ZoneVector<ExpressionTypeEntry> types(handles.main_zone());
  const char test_function[] =
      "function* foo() {\n"
      "  yield 123;\n"
      "}\n";
  CollectTypes(&handles, test_function, &types);
  CHECK_TYPES_BEGIN {
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
      // Implicit initial yield
      CHECK_EXPR(Yield, Bounds::Unbounded()) {
        CHECK_VAR(.generator_object, Bounds::Unbounded());
        CHECK_EXPR(Assignment, Bounds::Unbounded()) {
          CHECK_VAR(.generator_object, Bounds::Unbounded());
          CHECK_EXPR(CallRuntime, Bounds::Unbounded());
        }
      }
      // Explicit yield (argument wrapped with CreateIterResultObject)
      CHECK_EXPR(Yield, Bounds::Unbounded()) {
        CHECK_VAR(.generator_object, Bounds::Unbounded());
        CHECK_EXPR(CallRuntime, Bounds::Unbounded()) {
          CHECK_EXPR(Literal, Bounds::Unbounded());
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
      }
      // Argument to implicit final return
      CHECK_EXPR(CallRuntime, Bounds::Unbounded()) {  // CreateIterResultObject
        CHECK_EXPR(Literal, Bounds::Unbounded());
        CHECK_EXPR(Literal, Bounds::Unbounded());
      }
      // Implicit finally clause
      CHECK_EXPR(CallRuntime, Bounds::Unbounded()) {
        CHECK_VAR(.generator_object, Bounds::Unbounded());
      }
    }
  }
  CHECK_TYPES_END
}


TEST(VisitSkipping) {
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  ZoneVector<ExpressionTypeEntry> types(handles.main_zone());
  const char test_function[] =
      "function foo(x) {\n"
      "  return (x + x) + 1;\n"
      "}\n";
  CollectTypes(&handles, test_function, &types);
  CHECK_TYPES_BEGIN {
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
      CHECK_EXPR(BinaryOperation, Bounds::Unbounded()) {
        // Skip x + x
        CHECK_SKIP();
        CHECK_EXPR(Literal, Bounds::Unbounded());
      }
    }
  }
  CHECK_TYPES_END
}
