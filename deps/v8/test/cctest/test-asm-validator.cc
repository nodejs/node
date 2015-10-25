// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/ast.h"
#include "src/ast-expression-visitor.h"
#include "src/parser.h"
#include "src/rewriter.h"
#include "src/scopes.h"
#include "src/typing-asm.h"
#include "src/zone-type-cache.h"
#include "test/cctest/cctest.h"
#include "test/cctest/expression-type-collector.h"
#include "test/cctest/expression-type-collector-macros.h"

// Macros for function types.
#define FUNC_V_TYPE Bounds(Type::Function(Type::Undefined(), zone))
#define FUNC_I_TYPE Bounds(Type::Function(cache.kInt32, zone))
#define FUNC_F_TYPE Bounds(Type::Function(cache.kFloat32, zone))
#define FUNC_D_TYPE Bounds(Type::Function(cache.kFloat64, zone))
#define FUNC_D2D_TYPE \
  Bounds(Type::Function(cache.kFloat64, cache.kFloat64, zone))
#define FUNC_N2F_TYPE \
  Bounds(Type::Function(cache.kFloat32, Type::Number(), zone))
#define FUNC_I2I_TYPE Bounds(Type::Function(cache.kInt32, cache.kInt32, zone))
#define FUNC_II2D_TYPE \
  Bounds(Type::Function(cache.kFloat64, cache.kInt32, cache.kInt32, zone))
#define FUNC_II2I_TYPE \
  Bounds(Type::Function(cache.kInt32, cache.kInt32, cache.kInt32, zone))
#define FUNC_DD2D_TYPE \
  Bounds(Type::Function(cache.kFloat64, cache.kFloat64, cache.kFloat64, zone))
#define FUNC_N2N_TYPE \
  Bounds(Type::Function(Type::Number(), Type::Number(), zone))

// Macros for array types.
#define FLOAT64_ARRAY_TYPE Bounds(Type::Array(cache.kFloat64, zone))
#define FUNC_I2I_ARRAY_TYPE \
  Bounds(Type::Array(Type::Function(cache.kInt32, cache.kInt32, zone), zone))

using namespace v8::internal;

namespace {

std::string Validate(Zone* zone, const char* source,
                     ZoneVector<ExpressionTypeEntry>* types) {
  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();

  i::Handle<i::String> source_code =
      factory->NewStringFromUtf8(i::CStrVector(source)).ToHandleChecked();

  i::Handle<i::Script> script = factory->NewScript(source_code);

  i::ParseInfo info(zone, script);
  i::Parser parser(&info);
  parser.set_allow_harmony_arrow_functions(true);
  parser.set_allow_harmony_sloppy(true);
  info.set_global();
  info.set_lazy(false);
  info.set_allow_lazy_parsing(false);
  info.set_toplevel(true);

  CHECK(i::Compiler::ParseAndAnalyze(&info));

  FunctionLiteral* root =
      info.scope()->declarations()->at(0)->AsFunctionDeclaration()->fun();
  AsmTyper typer(isolate, zone, *script, root);
  if (typer.Validate()) {
    ExpressionTypeCollector(isolate, zone, root, types).Run();
    return "";
  } else {
    return typer.error_message();
  }
}
}


TEST(ValidateMinimum) {
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
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("", Validate(zone, test_function, &types));
  ZoneTypeCache cache;

  CHECK_TYPES_BEGIN {
    // Module.
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
      // function logSum
      CHECK_EXPR(FunctionLiteral, FUNC_II2D_TYPE) {
        CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
          CHECK_VAR(start, Bounds(cache.kInt32));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
            CHECK_VAR(start, Bounds(cache.kInt32));
            CHECK_EXPR(Literal, Bounds(cache.kInt32));
          }
        }
        CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
          CHECK_VAR(end, Bounds(cache.kInt32));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
            CHECK_VAR(end, Bounds(cache.kInt32));
            CHECK_EXPR(Literal, Bounds(cache.kInt32));
          }
        }
        CHECK_EXPR(Assignment, Bounds(cache.kFloat64)) {
          CHECK_VAR(sum, Bounds(cache.kFloat64));
          CHECK_EXPR(Literal, Bounds(cache.kFloat64));
        }
        CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
          CHECK_VAR(p, Bounds(cache.kInt32));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
        CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
          CHECK_VAR(q, Bounds(cache.kInt32));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
        // for (p = start << 3, q = end << 3;
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
            CHECK_VAR(p, Bounds(cache.kInt32));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
              CHECK_VAR(start, Bounds(cache.kInt32));
              CHECK_EXPR(Literal, Bounds(cache.kInt32));
            }
          }
          CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
            CHECK_VAR(q, Bounds(cache.kInt32));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
              CHECK_VAR(end, Bounds(cache.kInt32));
              CHECK_EXPR(Literal, Bounds(cache.kInt32));
            }
          }
        }
        // (p|0) < (q|0);
        CHECK_EXPR(CompareOperation, Bounds(cache.kInt32)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
            CHECK_VAR(p, Bounds(cache.kInt32));
            CHECK_EXPR(Literal, Bounds(cache.kInt32));
          }
          CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
            CHECK_VAR(q, Bounds(cache.kInt32));
            CHECK_EXPR(Literal, Bounds(cache.kInt32));
          }
        }
        // p = (p + 8)|0) {\n"
        CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
          CHECK_VAR(p, Bounds(cache.kInt32));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
            CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
              CHECK_VAR(p, Bounds(cache.kInt32));
              CHECK_EXPR(Literal, Bounds(cache.kInt32));
            }
            CHECK_EXPR(Literal, Bounds(cache.kInt32));
          }
        }
        // sum = sum + +log(values[p>>3]);
        CHECK_EXPR(Assignment, Bounds(cache.kFloat64)) {
          CHECK_VAR(sum, Bounds(cache.kFloat64));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kFloat64)) {
            CHECK_VAR(sum, Bounds(cache.kFloat64));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kFloat64)) {
              CHECK_EXPR(Call, Bounds(cache.kFloat64)) {
                CHECK_VAR(log, FUNC_D2D_TYPE);
                CHECK_EXPR(Property, Bounds(cache.kFloat64)) {
                  CHECK_VAR(values, FLOAT64_ARRAY_TYPE);
                  CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
                    CHECK_VAR(p, Bounds(cache.kInt32));
                    CHECK_EXPR(Literal, Bounds(cache.kInt32));
                  }
                }
              }
              CHECK_EXPR(Literal, Bounds(cache.kFloat64));
            }
          }
        }
        // return +sum;
        CHECK_EXPR(BinaryOperation, Bounds(cache.kFloat64)) {
          CHECK_VAR(sum, Bounds(cache.kFloat64));
          CHECK_EXPR(Literal, Bounds(cache.kFloat64));
        }
      }
      // function geometricMean
      CHECK_EXPR(FunctionLiteral, FUNC_II2D_TYPE) {
        CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
          CHECK_VAR(start, Bounds(cache.kInt32));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
            CHECK_VAR(start, Bounds(cache.kInt32));
            CHECK_EXPR(Literal, Bounds(cache.kInt32));
          }
        }
        CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
          CHECK_VAR(end, Bounds(cache.kInt32));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
            CHECK_VAR(end, Bounds(cache.kInt32));
            CHECK_EXPR(Literal, Bounds(cache.kInt32));
          }
        }
        // return +exp(+logSum(start, end) / +((end - start)|0));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kFloat64)) {
          CHECK_EXPR(Call, Bounds(cache.kFloat64)) {
            CHECK_VAR(exp, FUNC_D2D_TYPE);
            CHECK_EXPR(BinaryOperation, Bounds(cache.kFloat64)) {
              CHECK_EXPR(BinaryOperation, Bounds(cache.kFloat64)) {
                CHECK_EXPR(Call, Bounds(cache.kFloat64)) {
                  CHECK_VAR(logSum, FUNC_II2D_TYPE);
                  CHECK_VAR(start, Bounds(cache.kInt32));
                  CHECK_VAR(end, Bounds(cache.kInt32));
                }
                CHECK_EXPR(Literal, Bounds(cache.kFloat64));
              }
              CHECK_EXPR(BinaryOperation, Bounds(cache.kFloat64)) {
                CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
                  CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
                    CHECK_VAR(end, Bounds(cache.kInt32));
                    CHECK_VAR(start, Bounds(cache.kInt32));
                  }
                  CHECK_EXPR(Literal, Bounds(cache.kInt32));
                }
                CHECK_EXPR(Literal, Bounds(cache.kFloat64));
              }
            }
          }
          CHECK_EXPR(Literal, Bounds(cache.kFloat64));
        }
      }
      // "use asm";
      CHECK_EXPR(Literal, Bounds(Type::String()));
      // var exp = stdlib.Math.exp;
      CHECK_EXPR(Assignment, FUNC_D2D_TYPE) {
        CHECK_VAR(exp, FUNC_D2D_TYPE);
        CHECK_EXPR(Property, FUNC_D2D_TYPE) {
          CHECK_EXPR(Property, Bounds::Unbounded()) {
            CHECK_VAR(stdlib, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
      }
      // var log = stdlib.Math.log;
      CHECK_EXPR(Assignment, FUNC_D2D_TYPE) {
        CHECK_VAR(log, FUNC_D2D_TYPE);
        CHECK_EXPR(Property, FUNC_D2D_TYPE) {
          CHECK_EXPR(Property, Bounds::Unbounded()) {
            CHECK_VAR(stdlib, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
      }
      // var values = new stdlib.Float64Array(buffer);
      CHECK_EXPR(Assignment, FLOAT64_ARRAY_TYPE) {
        CHECK_VAR(values, FLOAT64_ARRAY_TYPE);
        CHECK_EXPR(CallNew, FLOAT64_ARRAY_TYPE) {
          CHECK_EXPR(Property, Bounds::Unbounded()) {
            CHECK_VAR(stdlib, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
          CHECK_VAR(buffer, Bounds::Unbounded());
        }
      }
      // return { geometricMean: geometricMean };
      CHECK_EXPR(ObjectLiteral, Bounds::Unbounded()) {
        CHECK_VAR(geometricMean, FUNC_II2D_TYPE);
      }
    }
  }
  CHECK_TYPES_END
}


#define HARNESS_STDLIB()                 \
  "var Infinity = stdlib.Infinity;\n"    \
  "var NaN = stdlib.NaN;\n"              \
  "var acos = stdlib.Math.acos;\n"       \
  "var asin = stdlib.Math.asin;\n"       \
  "var atan = stdlib.Math.atan;\n"       \
  "var cos = stdlib.Math.cos;\n"         \
  "var sin = stdlib.Math.sin;\n"         \
  "var tan = stdlib.Math.tan;\n"         \
  "var exp = stdlib.Math.exp;\n"         \
  "var log = stdlib.Math.log;\n"         \
  "var ceil = stdlib.Math.ceil;\n"       \
  "var floor = stdlib.Math.floor;\n"     \
  "var sqrt = stdlib.Math.sqrt;\n"       \
  "var min = stdlib.Math.min;\n"         \
  "var max = stdlib.Math.max;\n"         \
  "var atan2 = stdlib.Math.atan2;\n"     \
  "var pow = stdlib.Math.pow;\n"         \
  "var abs = stdlib.Math.abs;\n"         \
  "var imul = stdlib.Math.imul;\n"       \
  "var fround = stdlib.Math.fround;\n"   \
  "var E = stdlib.Math.E;\n"             \
  "var LN10 = stdlib.Math.LN10;\n"       \
  "var LN2 = stdlib.Math.LN2;\n"         \
  "var LOG2E = stdlib.Math.LOG2E;\n"     \
  "var LOG10E = stdlib.Math.LOG10E;\n"   \
  "var PI = stdlib.Math.PI;\n"           \
  "var SQRT1_2 = stdlib.Math.SQRT1_2;\n" \
  "var SQRT2 = stdlib.Math.SQRT2;\n"


#define HARNESS_HEAP()                           \
  "var u8 = new stdlib.Uint8Array(buffer);\n"    \
  "var i8 = new stdlib.Int8Array(buffer);\n"     \
  "var u16 = new stdlib.Uint16Array(buffer);\n"  \
  "var i16 = new stdlib.Int16Array(buffer);\n"   \
  "var u32 = new stdlib.Uint32Array(buffer);\n"  \
  "var i32 = new stdlib.Int32Array(buffer);\n"   \
  "var f32 = new stdlib.Float32Array(buffer);\n" \
  "var f64 = new stdlib.Float64Array(buffer);\n"


#define HARNESS_PREAMBLE()                           \
  const char test_function[] =                       \
      "function Module(stdlib, foreign, buffer) {\n" \
      "\"use asm\";\n" HARNESS_STDLIB() HARNESS_HEAP()


#define HARNESS_POSTAMBLE() \
  "return { foo: foo };\n"  \
  "}\n";


#define CHECK_VAR_MATH_SHORTCUT(name, type)       \
  CHECK_EXPR(Assignment, type) {                  \
    CHECK_VAR(name, type);                        \
    CHECK_EXPR(Property, type) {                  \
      CHECK_EXPR(Property, Bounds::Unbounded()) { \
        CHECK_VAR(stdlib, Bounds::Unbounded());   \
        CHECK_EXPR(Literal, Bounds::Unbounded()); \
      }                                           \
      CHECK_EXPR(Literal, Bounds::Unbounded());   \
    }                                             \
  }


#define CHECK_VAR_SHORTCUT(name, type)          \
  CHECK_EXPR(Assignment, type) {                \
    CHECK_VAR(name, type);                      \
    CHECK_EXPR(Property, type) {                \
      CHECK_VAR(stdlib, Bounds::Unbounded());   \
      CHECK_EXPR(Literal, Bounds::Unbounded()); \
    }                                           \
  }


#define CHECK_VAR_NEW_SHORTCUT(name, type)        \
  CHECK_EXPR(Assignment, type) {                  \
    CHECK_VAR(name, type);                        \
    CHECK_EXPR(CallNew, type) {                   \
      CHECK_EXPR(Property, Bounds::Unbounded()) { \
        CHECK_VAR(stdlib, Bounds::Unbounded());   \
        CHECK_EXPR(Literal, Bounds::Unbounded()); \
      }                                           \
      CHECK_VAR(buffer, Bounds::Unbounded());     \
    }                                             \
  }


namespace {

void CheckStdlibShortcuts(Zone* zone, ZoneVector<ExpressionTypeEntry>& types,
                          size_t& index, int& depth, ZoneTypeCache& cache) {
  // var exp = stdlib.*; (D * 12)
  CHECK_VAR_SHORTCUT(Infinity, Bounds(cache.kFloat64));
  CHECK_VAR_SHORTCUT(NaN, Bounds(cache.kFloat64));
  // var x = stdlib.Math.x;  D2D
  CHECK_VAR_MATH_SHORTCUT(acos, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(asin, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(atan, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(cos, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(sin, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(tan, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(exp, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(log, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(ceil, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(floor, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(sqrt, FUNC_D2D_TYPE);
  // var exp = stdlib.Math.*; (DD2D * 12)
  CHECK_VAR_MATH_SHORTCUT(min, FUNC_DD2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(max, FUNC_DD2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(atan2, FUNC_DD2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(pow, FUNC_DD2D_TYPE);
  // Special ones.
  CHECK_VAR_MATH_SHORTCUT(abs, FUNC_N2N_TYPE);
  CHECK_VAR_MATH_SHORTCUT(imul, FUNC_II2I_TYPE);
  CHECK_VAR_MATH_SHORTCUT(fround, FUNC_N2F_TYPE);
  // var exp = stdlib.Math.*; (D * 12)
  CHECK_VAR_MATH_SHORTCUT(E, Bounds(cache.kFloat64));
  CHECK_VAR_MATH_SHORTCUT(LN10, Bounds(cache.kFloat64));
  CHECK_VAR_MATH_SHORTCUT(LN2, Bounds(cache.kFloat64));
  CHECK_VAR_MATH_SHORTCUT(LOG2E, Bounds(cache.kFloat64));
  CHECK_VAR_MATH_SHORTCUT(LOG10E, Bounds(cache.kFloat64));
  CHECK_VAR_MATH_SHORTCUT(PI, Bounds(cache.kFloat64));
  CHECK_VAR_MATH_SHORTCUT(SQRT1_2, Bounds(cache.kFloat64));
  CHECK_VAR_MATH_SHORTCUT(SQRT2, Bounds(cache.kFloat64));
  // var values = new stdlib.*Array(buffer);
  CHECK_VAR_NEW_SHORTCUT(u8, Bounds(cache.kUint8Array));
  CHECK_VAR_NEW_SHORTCUT(i8, Bounds(cache.kInt8Array));
  CHECK_VAR_NEW_SHORTCUT(u16, Bounds(cache.kUint16Array));
  CHECK_VAR_NEW_SHORTCUT(i16, Bounds(cache.kInt16Array));
  CHECK_VAR_NEW_SHORTCUT(u32, Bounds(cache.kUint32Array));
  CHECK_VAR_NEW_SHORTCUT(i32, Bounds(cache.kInt32Array));
  CHECK_VAR_NEW_SHORTCUT(f32, Bounds(cache.kFloat32Array));
  CHECK_VAR_NEW_SHORTCUT(f64, Bounds(cache.kFloat64Array));
}
}


#define CHECK_FUNC_TYPES_BEGIN(func)                   \
  HARNESS_PREAMBLE()                                   \
  func "\n" HARNESS_POSTAMBLE();                       \
                                                       \
  v8::V8::Initialize();                                \
  HandleAndZoneScope handles;                          \
  Zone* zone = handles.main_zone();                    \
  ZoneVector<ExpressionTypeEntry> types(zone);         \
  CHECK_EQ("", Validate(zone, test_function, &types)); \
  ZoneTypeCache cache;                                 \
                                                       \
  CHECK_TYPES_BEGIN {                                  \
    /* Module. */                                      \
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
#define CHECK_FUNC_TYPES_END_1()               \
  /* "use asm"; */                             \
  CHECK_EXPR(Literal, Bounds(Type::String())); \
  /* stdlib shortcuts. */                      \
  CheckStdlibShortcuts(zone, types, index, depth, cache);


#define CHECK_FUNC_TYPES_END_2()                   \
  /* return { foo: foo }; */                       \
  CHECK_EXPR(ObjectLiteral, Bounds::Unbounded()) { \
    CHECK_VAR(foo, FUNC_V_TYPE);                   \
  }                                                \
  }                                                \
  }                                                \
  CHECK_TYPES_END


#define CHECK_FUNC_TYPES_END \
  CHECK_FUNC_TYPES_END_1();  \
  CHECK_FUNC_TYPES_END_2();


#define CHECK_FUNC_ERROR(func, message)        \
  HARNESS_PREAMBLE()                           \
  func "\n" HARNESS_POSTAMBLE();               \
                                               \
  v8::V8::Initialize();                        \
  HandleAndZoneScope handles;                  \
  Zone* zone = handles.main_zone();            \
  ZoneVector<ExpressionTypeEntry> types(zone); \
  CHECK_EQ(message, Validate(zone, test_function, &types));


TEST(BareHarness) {
  CHECK_FUNC_TYPES_BEGIN("function foo() {}") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {}
  }
  CHECK_FUNC_TYPES_END
}


TEST(ReturnVoid) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { return; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      // return undefined;
      CHECK_EXPR(Literal, Bounds(Type::Undefined()));
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(Type::Undefined())) {
        CHECK_VAR(bar, FUNC_V_TYPE);
      }
    }
  }
  CHECK_FUNC_TYPES_END
}


TEST(ReturnInt32Literal) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { return 1; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      // return 1;
      CHECK_EXPR(Literal, Bounds(cache.kInt32));
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kInt32)) { CHECK_VAR(bar, FUNC_I_TYPE); }
    }
  }
  CHECK_FUNC_TYPES_END
}


TEST(ReturnFloat64Literal) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { return 1.0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_D_TYPE) {
      // return 1.0;
      CHECK_EXPR(Literal, Bounds(cache.kFloat64));
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kFloat64)) { CHECK_VAR(bar, FUNC_D_TYPE); }
    }
  }
  CHECK_FUNC_TYPES_END
}


TEST(ReturnFloat32Literal) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { return fround(1.0); }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_F_TYPE) {
      // return fround(1.0);
      CHECK_EXPR(Call, Bounds(cache.kFloat32)) {
        CHECK_VAR(fround, FUNC_N2F_TYPE);
        CHECK_EXPR(Literal, Bounds(cache.kFloat64));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kFloat32)) { CHECK_VAR(bar, FUNC_F_TYPE); }
    }
  }
  CHECK_FUNC_TYPES_END
}


TEST(ReturnFloat64Var) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1.0; return +x; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_D_TYPE) {
      // return 1.0;
      CHECK_EXPR(Assignment, Bounds(cache.kFloat64)) {
        CHECK_VAR(x, Bounds(cache.kFloat64));
        CHECK_EXPR(Literal, Bounds(cache.kFloat64));
      }
      // return 1.0;
      CHECK_EXPR(BinaryOperation, Bounds(cache.kFloat64)) {
        CHECK_VAR(x, Bounds(cache.kFloat64));
        CHECK_EXPR(Literal, Bounds(cache.kFloat64));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kFloat64)) { CHECK_VAR(bar, FUNC_D_TYPE); }
    }
  }
  CHECK_FUNC_TYPES_END
}


TEST(Addition2) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 2; return (x+y)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(x, Bounds(cache.kInt32));
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(y, Bounds(cache.kInt32));
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_VAR(x, Bounds(cache.kInt32));
          CHECK_VAR(y, Bounds(cache.kInt32));
        }
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(Addition4) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 2; return (x+y+x+y)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(x, Bounds(cache.kInt32));
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(y, Bounds(cache.kInt32));
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
            CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
              CHECK_VAR(x, Bounds(cache.kInt32));
              CHECK_VAR(y, Bounds(cache.kInt32));
            }
            CHECK_VAR(x, Bounds(cache.kInt32));
          }
          CHECK_VAR(y, Bounds(cache.kInt32));
        }
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(Multiplication2) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 2; return (x*y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 39: direct integer multiply forbidden\n");
}


TEST(Division4) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 2; return (x/y/x/y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 39: too many consecutive multiplicative ops\n");
}


TEST(Load1) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = i8[x>>0]|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(x, Bounds(cache.kInt32));
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(y, Bounds(cache.kInt32));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_EXPR(Property, Bounds(cache.kInt8)) {
            CHECK_VAR(i8, Bounds(cache.kInt8Array));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
              CHECK_VAR(x, Bounds(cache.kInt32));
              CHECK_EXPR(Literal, Bounds(cache.kInt32));
            }
          }
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(FunctionTables) {
  CHECK_FUNC_TYPES_BEGIN(
      "function func1(x) { x = x | 0; return (x * 5) | 0; }\n"
      "function func2(x) { x = x | 0; return (x * 25) | 0; }\n"
      "var table1 = [func1, func2];\n"
      "function bar(x, y) { x = x | 0; y = y | 0;\n"
      "   return table1[x & 1](y)|0; }\n"
      "function foo() { bar(1, 2); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I2I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(x, Bounds(cache.kInt32));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_VAR(x, Bounds(cache.kInt32));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_VAR(x, Bounds(cache.kInt32));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_I2I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(x, Bounds(cache.kInt32));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_VAR(x, Bounds(cache.kInt32));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_VAR(x, Bounds(cache.kInt32));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_II2I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(x, Bounds(cache.kInt32));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_VAR(x, Bounds(cache.kInt32));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
      }
      CHECK_EXPR(Assignment, Bounds(cache.kInt32)) {
        CHECK_VAR(y, Bounds(cache.kInt32));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
          CHECK_VAR(y, Bounds(cache.kInt32));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
        CHECK_EXPR(Call, Bounds(cache.kInt32)) {
          CHECK_EXPR(Property, FUNC_I2I_TYPE) {
            CHECK_VAR(table1, FUNC_I2I_ARRAY_TYPE);
            CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
              CHECK_VAR(x, Bounds(cache.kInt32));
              CHECK_EXPR(Literal, Bounds(cache.kInt32));
            }
          }
          CHECK_VAR(y, Bounds(cache.kInt32));
        }
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END_1();
  CHECK_EXPR(Assignment, FUNC_I2I_ARRAY_TYPE) {
    CHECK_VAR(table1, FUNC_I2I_ARRAY_TYPE);
    CHECK_EXPR(ArrayLiteral, FUNC_I2I_ARRAY_TYPE) {
      CHECK_VAR(func1, FUNC_I2I_TYPE);
      CHECK_VAR(func2, FUNC_I2I_TYPE);
    }
  }
  CHECK_FUNC_TYPES_END_2();
}


TEST(BadFunctionTable) {
  CHECK_FUNC_ERROR(
      "function func1(x) { x = x | 0; return (x * 5) | 0; }\n"
      "var table1 = [func1, 1];\n"
      "function bar(x, y) { x = x | 0; y = y | 0;\n"
      "   return table1[x & 1](y)|0; }\n"
      "function foo() { bar(1, 2); }",
      "asm: line 40: array component expected to be a function\n");
}


TEST(MissingParameterTypes) {
  CHECK_FUNC_ERROR(
      "function bar(x) { var y = 1; }\n"
      "function foo() { bar(2); }",
      "asm: line 39: missing parameter type annotations\n");
}


TEST(InvalidTypeAnnotationBinaryOpDiv) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = x / 4; }\n"
      "function foo() { bar(2); }",
      "asm: line 39: invalid type annotation on binary op\n");
}


TEST(InvalidTypeAnnotationBinaryOpMul) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = x * 4.0; }\n"
      "function foo() { bar(2); }",
      "asm: line 39: invalid type annotation on binary op\n");
}


TEST(InvalidArgumentCount) {
  CHECK_FUNC_ERROR(
      "function bar(x) { return fround(4, 5); }\n"
      "function foo() { bar(); }",
      "asm: line 39: invalid argument count calling fround\n");
}


TEST(InvalidTypeAnnotationArity) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = max(x); }\n"
      "function foo() { bar(3); }",
      "asm: line 39: only fround allowed on expression annotations\n");
}


TEST(InvalidTypeAnnotationOnlyFround) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = sin(x); }\n"
      "function foo() { bar(3); }",
      "asm: line 39: only fround allowed on expression annotations\n");
}


TEST(InvalidTypeAnnotation) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = (x+x)(x); }\n"
      "function foo() { bar(3); }",
      "asm: line 39: invalid type annotation\n");
}


TEST(WithStatement) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 0; with (x) { x = x + 1; } }\n"
      "function foo() { bar(); }",
      "asm: line 39: bad with statement\n");
}


TEST(NestedFunction) {
  CHECK_FUNC_ERROR(
      "function bar() { function x() { return 1; } }\n"
      "function foo() { bar(); }",
      "asm: line 39: function declared inside another\n");
}


TEST(UnboundVariable) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = y; }\n"
      "function foo() { bar(); }",
      "asm: line 39: unbound variable\n");
}


TEST(ForeignFunction) {
  CHECK_FUNC_TYPES_BEGIN(
      "var baz = foreign.baz;\n"
      "function bar() { return baz(1, 2)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(BinaryOperation, Bounds(cache.kInt32)) {
        CHECK_EXPR(Call, Bounds(Type::Number(zone))) {
          CHECK_VAR(baz, Bounds(Type::Any()));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
          CHECK_EXPR(Literal, Bounds(cache.kInt32));
        }
        CHECK_EXPR(Literal, Bounds(cache.kInt32));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kInt32)) { CHECK_VAR(bar, FUNC_I_TYPE); }
    }
  }
  CHECK_FUNC_TYPES_END_1()
  CHECK_EXPR(Assignment, Bounds(Type::Any())) {
    CHECK_VAR(baz, Bounds(Type::Any()));
    CHECK_EXPR(Property, Bounds(Type::Any())) {
      CHECK_VAR(foreign, Bounds::Unbounded());
      CHECK_EXPR(Literal, Bounds::Unbounded());
    }
  }
  CHECK_FUNC_TYPES_END_2()
}


TEST(BadExports) {
  HARNESS_PREAMBLE()
  "function foo() {};\n"
  "return {foo: foo, bar: 1};"
  "}\n";

  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 40: non-function in function table\n",
           Validate(zone, test_function, &types));
}
