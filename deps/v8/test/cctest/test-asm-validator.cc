// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/ast/ast.h"
#include "src/ast/ast-expression-visitor.h"
#include "src/ast/scopes.h"
#include "src/parsing/parser.h"
#include "src/parsing/rewriter.h"
#include "src/type-cache.h"
#include "src/typing-asm.h"
#include "test/cctest/cctest.h"
#include "test/cctest/expression-type-collector.h"
#include "test/cctest/expression-type-collector-macros.h"

// Macros for function types.
#define FUNC_FOREIGN_TYPE Bounds(Type::Function(Type::Any(), zone))
#define FUNC_V_TYPE Bounds(Type::Function(Type::Undefined(), zone))
#define FUNC_I_TYPE Bounds(Type::Function(cache.kAsmSigned, zone))
#define FUNC_F_TYPE Bounds(Type::Function(cache.kAsmFloat, zone))
#define FUNC_D_TYPE Bounds(Type::Function(cache.kAsmDouble, zone))
#define FUNC_D2D_TYPE \
  Bounds(Type::Function(cache.kAsmDouble, cache.kAsmDouble, zone))
#define FUNC_N2F_TYPE \
  Bounds(Type::Function(cache.kAsmFloat, Type::Number(), zone))
#define FUNC_I2I_TYPE \
  Bounds(Type::Function(cache.kAsmSigned, cache.kAsmInt, zone))
#define FUNC_II2D_TYPE \
  Bounds(Type::Function(cache.kAsmDouble, cache.kAsmInt, cache.kAsmInt, zone))
#define FUNC_II2I_TYPE \
  Bounds(Type::Function(cache.kAsmSigned, cache.kAsmInt, cache.kAsmInt, zone))
#define FUNC_DD2D_TYPE                                                        \
  Bounds(Type::Function(cache.kAsmDouble, cache.kAsmDouble, cache.kAsmDouble, \
                        zone))
#define FUNC_NN2N_TYPE \
  Bounds(Type::Function(Type::Number(), Type::Number(), Type::Number(), zone))
#define FUNC_N2N_TYPE \
  Bounds(Type::Function(Type::Number(), Type::Number(), zone))

// Macros for array types.
#define FLOAT64_ARRAY_TYPE Bounds(Type::Array(cache.kAsmDouble, zone))
#define FUNC_I2I_ARRAY_TYPE                                                 \
  Bounds(Type::Array(Type::Function(cache.kAsmSigned, cache.kAsmInt, zone), \
                     zone))

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
  info.set_global();
  info.set_lazy(false);
  info.set_allow_lazy_parsing(false);
  info.set_toplevel(true);

  CHECK(i::Compiler::ParseAndAnalyze(&info));

  FunctionLiteral* root =
      info.scope()->declarations()->at(0)->AsFunctionDeclaration()->fun();
  AsmTyper typer(isolate, zone, *script, root);
  if (typer.Validate()) {
    ExpressionTypeCollector(isolate, root, typer.bounds(), types).Run();
    return "";
  } else {
    return typer.error_message();
  }
}

}  // namespace


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
  TypeCache cache;

  CHECK_TYPES_BEGIN {
    // Module.
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
      // function logSum
      CHECK_EXPR(FunctionLiteral, FUNC_II2D_TYPE) {
        CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
          CHECK_VAR(start, Bounds(cache.kAsmInt));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(start, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
          CHECK_VAR(end, Bounds(cache.kAsmInt));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(end, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
          CHECK_VAR(sum, Bounds(cache.kAsmDouble));
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
        CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
          CHECK_VAR(p, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
          CHECK_VAR(q, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        // for (p = start << 3, q = end << 3;
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
          CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
            CHECK_VAR(p, Bounds(cache.kAsmInt));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
              CHECK_VAR(start, Bounds(cache.kAsmInt));
              CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            }
          }
          CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
            CHECK_VAR(q, Bounds(cache.kAsmInt));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
              CHECK_VAR(end, Bounds(cache.kAsmInt));
              CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            }
          }
        }
        // (p|0) < (q|0);
        CHECK_EXPR(CompareOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(p, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(q, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        // p = (p + 8)|0) {
        CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
          CHECK_VAR(p, Bounds(cache.kAsmInt));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
              CHECK_VAR(p, Bounds(cache.kAsmInt));
              CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            }
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        // sum = sum + +log(values[p>>3]);
        CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
          CHECK_VAR(sum, Bounds(cache.kAsmDouble));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
            CHECK_VAR(sum, Bounds(cache.kAsmDouble));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
              CHECK_EXPR(Call, Bounds(cache.kAsmDouble)) {
                CHECK_VAR(log, FUNC_D2D_TYPE);
                CHECK_EXPR(Property, Bounds(cache.kAsmDouble)) {
                  CHECK_VAR(values, FLOAT64_ARRAY_TYPE);
                  CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
                    CHECK_VAR(p, Bounds(cache.kAsmSigned));
                    CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
                  }
                }
              }
              CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
            }
          }
        }
        // return +sum;
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
          CHECK_VAR(sum, Bounds(cache.kAsmDouble));
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
      // function geometricMean
      CHECK_EXPR(FunctionLiteral, FUNC_II2D_TYPE) {
        CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
          CHECK_VAR(start, Bounds(cache.kAsmInt));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(start, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
          CHECK_VAR(end, Bounds(cache.kAsmInt));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(end, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        // return +exp(+logSum(start, end) / +((end - start)|0));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
          CHECK_EXPR(Call, Bounds(cache.kAsmDouble)) {
            CHECK_VAR(exp, FUNC_D2D_TYPE);
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
              CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
                CHECK_EXPR(Call, Bounds(cache.kAsmDouble)) {
                  CHECK_VAR(logSum, FUNC_II2D_TYPE);
                  CHECK_VAR(start, Bounds(cache.kAsmInt));
                  CHECK_VAR(end, Bounds(cache.kAsmInt));
                }
                CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
              }
              CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
                CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
                  CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
                    CHECK_VAR(end, Bounds(cache.kAsmInt));
                    CHECK_VAR(start, Bounds(cache.kAsmInt));
                  }
                  CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
                }
                CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
              }
            }
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
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


TEST(MissingUseAsm) {
  const char test_function[] =
      "function foo() {\n"
      "  function bar() {}\n"
      "  return { bar: bar };\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 1: missing \"use asm\"\n",
           Validate(zone, test_function, &types));
}


TEST(WrongUseAsm) {
  const char test_function[] =
      "function foo() {\n"
      "  \"use wasm\"\n"
      "  function bar() {}\n"
      "  return { bar: bar };\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 1: missing \"use asm\"\n",
           Validate(zone, test_function, &types));
}


TEST(MissingReturnExports) {
  const char test_function[] =
      "function foo() {\n"
      "  \"use asm\"\n"
      "  function bar() {}\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 2: last statement in module is not a return\n",
           Validate(zone, test_function, &types));
}

#define HARNESS_STDLIB()                \
  "var Infinity = stdlib.Infinity; "    \
  "var NaN = stdlib.NaN; "              \
  "var acos = stdlib.Math.acos; "       \
  "var asin = stdlib.Math.asin; "       \
  "var atan = stdlib.Math.atan; "       \
  "var cos = stdlib.Math.cos; "         \
  "var sin = stdlib.Math.sin; "         \
  "var tan = stdlib.Math.tan; "         \
  "var exp = stdlib.Math.exp; "         \
  "var log = stdlib.Math.log; "         \
  "var ceil = stdlib.Math.ceil; "       \
  "var floor = stdlib.Math.floor; "     \
  "var sqrt = stdlib.Math.sqrt; "       \
  "var min = stdlib.Math.min; "         \
  "var max = stdlib.Math.max; "         \
  "var atan2 = stdlib.Math.atan2; "     \
  "var pow = stdlib.Math.pow; "         \
  "var abs = stdlib.Math.abs; "         \
  "var imul = stdlib.Math.imul; "       \
  "var fround = stdlib.Math.fround; "   \
  "var E = stdlib.Math.E; "             \
  "var LN10 = stdlib.Math.LN10; "       \
  "var LN2 = stdlib.Math.LN2; "         \
  "var LOG2E = stdlib.Math.LOG2E; "     \
  "var LOG10E = stdlib.Math.LOG10E; "   \
  "var PI = stdlib.Math.PI; "           \
  "var SQRT1_2 = stdlib.Math.SQRT1_2; " \
  "var SQRT2 = stdlib.Math.SQRT2; "

#define HARNESS_HEAP()                          \
  "var u8 = new stdlib.Uint8Array(buffer); "    \
  "var i8 = new stdlib.Int8Array(buffer); "     \
  "var u16 = new stdlib.Uint16Array(buffer); "  \
  "var i16 = new stdlib.Int16Array(buffer); "   \
  "var u32 = new stdlib.Uint32Array(buffer); "  \
  "var i32 = new stdlib.Int32Array(buffer); "   \
  "var f32 = new stdlib.Float32Array(buffer); " \
  "var f64 = new stdlib.Float64Array(buffer); "

#define HARNESS_PREAMBLE()                          \
  const char test_function[] =                      \
      "function Module(stdlib, foreign, buffer) { " \
      "\"use asm\"; " HARNESS_STDLIB() HARNESS_HEAP()

#define HARNESS_POSTAMBLE() \
  "return { foo: foo }; "   \
  "} ";

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

void CheckStdlibShortcuts1(Zone* zone, ZoneVector<ExpressionTypeEntry>& types,
                           size_t& index, int& depth, TypeCache& cache) {
  // var exp = stdlib.*;
  CHECK_VAR_SHORTCUT(Infinity, Bounds(cache.kAsmDouble));
  CHECK_VAR_SHORTCUT(NaN, Bounds(cache.kAsmDouble));
  // var x = stdlib.Math.x;
  CHECK_VAR_MATH_SHORTCUT(acos, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(asin, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(atan, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(cos, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(sin, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(tan, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(exp, FUNC_D2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(log, FUNC_D2D_TYPE);

  CHECK_VAR_MATH_SHORTCUT(ceil, FUNC_N2N_TYPE);
  CHECK_VAR_MATH_SHORTCUT(floor, FUNC_N2N_TYPE);
  CHECK_VAR_MATH_SHORTCUT(sqrt, FUNC_N2N_TYPE);

  CHECK_VAR_MATH_SHORTCUT(min, FUNC_NN2N_TYPE);
  CHECK_VAR_MATH_SHORTCUT(max, FUNC_NN2N_TYPE);

  CHECK_VAR_MATH_SHORTCUT(atan2, FUNC_DD2D_TYPE);
  CHECK_VAR_MATH_SHORTCUT(pow, FUNC_DD2D_TYPE);

  CHECK_VAR_MATH_SHORTCUT(abs, FUNC_N2N_TYPE);
  CHECK_VAR_MATH_SHORTCUT(imul, FUNC_II2I_TYPE);
  CHECK_VAR_MATH_SHORTCUT(fround, FUNC_N2F_TYPE);
}


void CheckStdlibShortcuts2(Zone* zone, ZoneVector<ExpressionTypeEntry>& types,
                           size_t& index, int& depth, TypeCache& cache) {
  // var exp = stdlib.Math.*; (D * 12)
  CHECK_VAR_MATH_SHORTCUT(E, Bounds(cache.kAsmDouble));
  CHECK_VAR_MATH_SHORTCUT(LN10, Bounds(cache.kAsmDouble));
  CHECK_VAR_MATH_SHORTCUT(LN2, Bounds(cache.kAsmDouble));
  CHECK_VAR_MATH_SHORTCUT(LOG2E, Bounds(cache.kAsmDouble));
  CHECK_VAR_MATH_SHORTCUT(LOG10E, Bounds(cache.kAsmDouble));
  CHECK_VAR_MATH_SHORTCUT(PI, Bounds(cache.kAsmDouble));
  CHECK_VAR_MATH_SHORTCUT(SQRT1_2, Bounds(cache.kAsmDouble));
  CHECK_VAR_MATH_SHORTCUT(SQRT2, Bounds(cache.kAsmDouble));
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

}  // namespace


#define CHECK_FUNC_TYPES_BEGIN(func)                   \
  HARNESS_PREAMBLE()                                   \
  func "\n" HARNESS_POSTAMBLE();                       \
                                                       \
  v8::V8::Initialize();                                \
  HandleAndZoneScope handles;                          \
  Zone* zone = handles.main_zone();                    \
  ZoneVector<ExpressionTypeEntry> types(zone);         \
  CHECK_EQ("", Validate(zone, test_function, &types)); \
  TypeCache cache;                                     \
                                                       \
  CHECK_TYPES_BEGIN {                                  \
    /* Module. */                                      \
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
#define CHECK_FUNC_TYPES_END_1()                           \
  /* "use asm"; */                                         \
  CHECK_EXPR(Literal, Bounds(Type::String()));             \
  /* stdlib shortcuts. */                                  \
  CheckStdlibShortcuts1(zone, types, index, depth, cache); \
  CheckStdlibShortcuts2(zone, types, index, depth, cache);

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


TEST(EmptyBody) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE);
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(Type::Undefined())) {
        CHECK_VAR(bar, FUNC_V_TYPE);
      }
    }
  }
  CHECK_FUNC_TYPES_END
}


TEST(DoesNothing) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1.0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
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
      CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kAsmSigned)) {
        CHECK_VAR(bar, FUNC_I_TYPE);
      }
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
      CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(bar, FUNC_D_TYPE);
      }
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
      CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(fround, FUNC_N2F_TYPE);
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) { CHECK_VAR(bar, FUNC_F_TYPE); }
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
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
      // return 1.0;
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(bar, FUNC_D_TYPE);
      }
    }
  }
  CHECK_FUNC_TYPES_END
}


TEST(Addition2) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 2; return (x+y)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_VAR(y, Bounds(cache.kAsmInt));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


#define TEST_COMPARE_OP(name, op)                                  \
  TEST(name) {                                                     \
    CHECK_FUNC_TYPES_BEGIN("function bar() { return (0 " op        \
                           " 0)|0; }\n"                            \
                           "function foo() { bar(); }") {          \
      CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {                   \
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {    \
          CHECK_EXPR(CompareOperation, Bounds(cache.kAsmSigned)) { \
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));         \
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));         \
          }                                                        \
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));           \
        }                                                          \
      }                                                            \
      CHECK_SKIP();                                                \
    }                                                              \
    CHECK_FUNC_TYPES_END                                           \
  }


TEST_COMPARE_OP(EqOperator, "==")
TEST_COMPARE_OP(LtOperator, "<")
TEST_COMPARE_OP(LteOperator, "<=")
TEST_COMPARE_OP(GtOperator, ">")
TEST_COMPARE_OP(GteOperator, ">=")


TEST(NeqOperator) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { return (0 != 0)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(UnaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(CompareOperation, Bounds(cache.kAsmSigned)) {
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(NotOperator) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 0; return (!x)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(UnaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(InvertOperator) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 0; return (~x)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(InvertConversion) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 0.0; return (~~x)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(x, Bounds(cache.kAsmDouble));
            CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(Ternary) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 1; return (x?y:5)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(Conditional, Bounds(cache.kAsmInt)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_VAR(y, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


#define TEST_INT_BIN_OP(name, op)                                      \
  TEST(name) {                                                         \
    CHECK_FUNC_TYPES_BEGIN("function bar() { var x = 0; return (x " op \
                           " 123)|0; }\n"                              \
                           "function foo() { bar(); }") {              \
      CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {                       \
        CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {                \
          CHECK_VAR(x, Bounds(cache.kAsmInt));                         \
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));               \
        }                                                              \
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {        \
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {      \
            CHECK_VAR(x, Bounds(cache.kAsmInt));                       \
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));             \
          }                                                            \
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));               \
        }                                                              \
      }                                                                \
      CHECK_SKIP();                                                    \
    }                                                                  \
    CHECK_FUNC_TYPES_END                                               \
  }


TEST_INT_BIN_OP(AndOperator, "&")
TEST_INT_BIN_OP(OrOperator, "|")
TEST_INT_BIN_OP(XorOperator, "^")


TEST(SignedCompare) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 1; return ((x|0) < (y|0))|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(CompareOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(x, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(y, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(SignedCompareConst) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 1; return ((x|0) < (1<<31))|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(CompareOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(x, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(UnsignedCompare) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 1; return ((x>>>0) < (y>>>0))|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(CompareOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmUnsigned)) {
            CHECK_VAR(x, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmUnsigned)) {
            CHECK_VAR(y, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(UnsignedCompareConst0) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 1; return ((x>>>0) < (0>>>0))|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(CompareOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmUnsigned)) {
            CHECK_VAR(x, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(UnsignedCompareConst1) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 1; return ((x>>>0) < "
      "(0xffffffff>>>0))|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(CompareOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmUnsigned)) {
            CHECK_VAR(x, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmUnsigned));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(UnsignedDivide) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 1; return ((x>>>0) / (y>>>0))|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(BinaryOperation, Bounds(Type::None(), Type::Any())) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmUnsigned)) {
            CHECK_VAR(x, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmUnsigned)) {
            CHECK_VAR(y, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(UnsignedFromFloat64) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1.0; return (x>>>0)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: left bitwise operand expected to be an integer\n");
}


TEST(AndFloat64) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1.0; return (x&0)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: left bitwise operand expected to be an integer\n");
}


TEST(TypeMismatchAddInt32Float64) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1.0; var y = 0; return (x + y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: ill-typed arithmetic operation\n");
}


TEST(TypeMismatchSubInt32Float64) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1.0; var y = 0; return (x - y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: ill-typed arithmetic operation\n");
}


TEST(TypeMismatchDivInt32Float64) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1.0; var y = 0; return (x / y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: ill-typed arithmetic operation\n");
}


TEST(TypeMismatchModInt32Float64) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1.0; var y = 0; return (x % y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: ill-typed arithmetic operation\n");
}


TEST(ModFloat32) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = fround(1.0); return (x % x)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: ill-typed arithmetic operation\n");
}


TEST(TernaryMismatchInt32Float64) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 0.0; return (1 ? x : y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: then and else expressions in ? must have the same type\n");
}


TEST(TernaryMismatchIntish) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 0; return (1 ? x + x : y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: invalid type in ? then expression\n");
}


TEST(TernaryMismatchInt32Float32) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 2.0; return (x?fround(y):x)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: then and else expressions in ? must have the same type\n");
}


TEST(TernaryBadCondition) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 2.0; return (y?x:1)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: condition must be of type int\n");
}

TEST(BadIntishMultiply) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; return ((x + x) * 4) | 0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: intish not allowed in multiply\n");
}

TEST(IntToFloat32) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; return fround(x); }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal function argument type\n");
}

TEST(Int32ToFloat32) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; return fround(x|0); }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_F_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(fround, FUNC_N2F_TYPE);
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}

TEST(Uint32ToFloat32) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; return fround(x>>>0); }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_F_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(fround, FUNC_N2F_TYPE);
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmUnsigned)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}

TEST(Float64ToFloat32) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1.0; return fround(x); }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_F_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
      CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(fround, FUNC_N2F_TYPE);
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}

TEST(Int32ToFloat32ToInt32) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; return ~~fround(x|0) | 0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
              CHECK_VAR(fround, FUNC_N2F_TYPE);
              CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
                CHECK_VAR(x, Bounds(cache.kAsmInt));
                CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
              }
            }
            CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
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
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
              CHECK_VAR(x, Bounds(cache.kAsmInt));
              CHECK_VAR(y, Bounds(cache.kAsmInt));
            }
            CHECK_VAR(x, Bounds(cache.kAsmInt));
          }
          CHECK_VAR(y, Bounds(cache.kAsmInt));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
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
      "asm: line 1: multiply must be by an integer literal\n");
}


TEST(Division4) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 2; return (x/y/x/y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: too many consecutive multiplicative ops\n");
}


TEST(CompareToStringLeft) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; return ('hi' > x)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: bad type on left side of comparison\n");
}


TEST(CompareToStringRight) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; return (x < 'hi')|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: bad type on right side of comparison\n");
}


TEST(CompareMismatchInt32Float64) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 2.0; return (x < y)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: left and right side of comparison must match\n");
}


TEST(CompareMismatchInt32Uint32) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 2; return ((x|0) < (y>>>0))|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: left and right side of comparison must match\n");
}


TEST(CompareMismatchInt32Float32) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; var y = 2.0; return (x < fround(y))|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: left and right side of comparison must match\n");
}


TEST(Float64ToInt32) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 0.0; x = ~~y; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(y, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(y, Bounds(cache.kAsmDouble));
            CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(Load1) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = i8[x>>0]|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(Property, Bounds(cache.kAsmInt)) {
            CHECK_VAR(i8, Bounds(cache.kInt8Array));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
              CHECK_VAR(x, Bounds(cache.kAsmInt));
              CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            }
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(LoadDouble) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 0.0; y = +f64[x>>3]; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(y, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(y, Bounds(cache.kAsmDouble));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
          CHECK_EXPR(Property, Bounds(cache.kAsmDouble)) {
            CHECK_VAR(f64, Bounds(cache.kFloat64Array));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
              CHECK_VAR(x, Bounds(cache.kAsmSigned));
              CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            }
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(Store1) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; i8[x>>0] = 0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_EXPR(Property, Bounds::Unbounded()) {
          CHECK_VAR(i8, Bounds(cache.kInt8Array));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(x, Bounds(cache.kAsmInt));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(StoreFloat) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = fround(1.0); "
      "f32[0] = fround(x + fround(1.0)); }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(x, Bounds(cache.kAsmFloat));
        CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
          CHECK_VAR(fround, FUNC_N2F_TYPE);
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmFloat)) {
        CHECK_EXPR(Property, Bounds::Unbounded()) {
          CHECK_VAR(f32, Bounds(cache.kFloat32Array));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
          CHECK_VAR(fround, FUNC_N2F_TYPE);
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmFloat)) {
            CHECK_VAR(x, Bounds(cache.kAsmFloat));
            CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
              CHECK_VAR(fround, FUNC_N2F_TYPE);
              CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
            }
          }
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}

TEST(StoreIntish) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = 1; i32[0] = x + y; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_EXPR(Property, Bounds::Unbounded()) {
          CHECK_VAR(i32, Bounds(cache.kInt32Array));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_VAR(y, Bounds(cache.kAsmInt));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}

TEST(StoreFloatish) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { "
      "var x = fround(1.0); "
      "var y = fround(1.0); f32[0] = x + y; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(x, Bounds(cache.kAsmFloat));
        CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
          CHECK_VAR(fround, FUNC_N2F_TYPE);
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(y, Bounds(cache.kAsmFloat));
        CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
          CHECK_VAR(fround, FUNC_N2F_TYPE);
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmFloat)) {
        CHECK_EXPR(Property, Bounds::Unbounded()) {
          CHECK_VAR(f32, Bounds(cache.kFloat32Array));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmFloat)) {
          CHECK_VAR(x, Bounds(cache.kAsmFloat));
          CHECK_VAR(y, Bounds(cache.kAsmFloat));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}

TEST(Load1Constant) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 1; var y = i8[5]|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(Property, Bounds(cache.kAsmInt)) {
            CHECK_VAR(i8, Bounds(cache.kInt8Array));
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
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
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_I2I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmInt)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_II2I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(y, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(y, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(Call, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(Property, FUNC_I2I_TYPE) {
            CHECK_VAR(table1, FUNC_I2I_ARRAY_TYPE);
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
              // TODO(bradnelson): revert this
              // CHECK_VAR(x, Bounds(cache.kAsmSigned));
              CHECK_VAR(x, Bounds(cache.kAsmInt));
              CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            }
          }
          CHECK_VAR(y, Bounds(cache.kAsmInt));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
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
      "asm: line 2: array component expected to be a function\n");
}


TEST(MissingParameterTypes) {
  CHECK_FUNC_ERROR(
      "function bar(x) { var y = 1; }\n"
      "function foo() { bar(2); }",
      "asm: line 1: missing parameter type annotations\n");
}


TEST(InvalidTypeAnnotationBinaryOpDiv) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = x / 4; }\n"
      "function foo() { bar(2); }",
      "asm: line 1: invalid type annotation on binary op\n");
}


TEST(InvalidTypeAnnotationBinaryOpMul) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = x * 4.0; }\n"
      "function foo() { bar(2); }",
      "asm: line 1: invalid type annotation on binary op\n");
}


TEST(InvalidArgumentCount) {
  CHECK_FUNC_ERROR(
      "function bar(x) { return fround(4, 5); }\n"
      "function foo() { bar(); }",
      "asm: line 1: invalid argument count calling function\n");
}


TEST(InvalidTypeAnnotationArity) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = max(x); }\n"
      "function foo() { bar(3); }",
      "asm: line 1: only fround allowed on expression annotations\n");
}


TEST(InvalidTypeAnnotationOnlyFround) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = sin(x); }\n"
      "function foo() { bar(3); }",
      "asm: line 1: only fround allowed on expression annotations\n");
}


TEST(InvalidTypeAnnotation) {
  CHECK_FUNC_ERROR(
      "function bar(x) { x = (x+x)(x); }\n"
      "function foo() { bar(3); }",
      "asm: line 1: invalid type annotation\n");
}


TEST(WithStatement) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 0; with (x) { x = x + 1; } }\n"
      "function foo() { bar(); }",
      "asm: line 1: bad with statement\n");
}


TEST(NestedFunction) {
  CHECK_FUNC_ERROR(
      "function bar() { function x() { return 1; } }\n"
      "function foo() { bar(); }",
      "asm: line 1: function declared inside another\n");
}


TEST(UnboundVariable) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = y; }\n"
      "function foo() { bar(); }",
      "asm: line 1: unbound variable\n");
}


TEST(EqStrict) {
  CHECK_FUNC_ERROR(
      "function bar() { return (0 === 0)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal comparison operator\n");
}


TEST(NeStrict) {
  CHECK_FUNC_ERROR(
      "function bar() { return (0 !== 0)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal comparison operator\n");
}


TEST(InstanceOf) {
  CHECK_FUNC_ERROR(
      "function bar() { return (0 instanceof 0)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal comparison operator\n");
}


TEST(InOperator) {
  CHECK_FUNC_ERROR(
      "function bar() { return (0 in 0)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal comparison operator\n");
}


TEST(LogicalAndOperator) {
  CHECK_FUNC_ERROR(
      "function bar() { return (0 && 0)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal logical operator\n");
}


TEST(LogicalOrOperator) {
  CHECK_FUNC_ERROR(
      "function bar() { return (0 || 0)|0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal logical operator\n");
}

TEST(BitOrDouble) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1.0; return x | 0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: intish required\n");
}

TEST(BadLiteral) {
  CHECK_FUNC_ERROR(
      "function bar() { return true | 0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal literal\n");
}


TEST(MismatchedReturnTypeLiteral) {
  CHECK_FUNC_ERROR(
      "function bar() { if(1) { return 1; } return 1.0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: return type does not match function signature\n");
}


TEST(MismatchedReturnTypeExpression) {
  CHECK_FUNC_ERROR(
      "function bar() {\n"
      "  var x = 1; var y = 1.0; if(1) { return x; } return +y; }\n"
      "function foo() { bar(); }",
      "asm: line 2: return type does not match function signature\n");
}


TEST(AssignToFloatishToF64) {
  CHECK_FUNC_ERROR(
      "function bar() { var v = fround(1.0); f64[0] = v + fround(1.0); }\n"
      "function foo() { bar(); }",
      "asm: line 1: floatish assignment to double array\n");
}


TEST(ForeignFunction) {
  CHECK_FUNC_TYPES_BEGIN(
      "var baz = foreign.baz;\n"
      "function bar() { return baz(1, 2)|0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
        CHECK_EXPR(Call, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(baz, FUNC_FOREIGN_TYPE);
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Call, Bounds(cache.kAsmSigned)) {
        CHECK_VAR(bar, FUNC_I_TYPE);
      }
    }
  }
  CHECK_FUNC_TYPES_END_1()
  CHECK_EXPR(Assignment, Bounds(FUNC_FOREIGN_TYPE)) {
    CHECK_VAR(baz, Bounds(FUNC_FOREIGN_TYPE));
    CHECK_EXPR(Property, Bounds(FUNC_FOREIGN_TYPE)) {
      CHECK_VAR(foreign, Bounds::Unbounded());
      CHECK_EXPR(Literal, Bounds::Unbounded());
    }
  }
  CHECK_FUNC_TYPES_END_2()
}

TEST(ByteArray) {
  // Forbidden by asm.js spec, present in embenchen.
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 0; i8[x] = 2; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_EXPR(Property, Bounds::Unbounded()) {
          CHECK_VAR(i8, Bounds(cache.kInt8Array));
          CHECK_VAR(x, Bounds(cache.kAsmSigned));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
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
  CHECK_EQ("asm: line 2: non-function in function table\n",
           Validate(zone, test_function, &types));
}


TEST(NestedHeapAssignment) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 0; i16[x = 1] = 2; }\n"
      "function foo() { bar(); }",
      "asm: line 1: expected >> in heap access\n");
}

TEST(BadOperatorHeapAssignment) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 0; i16[x & 1] = 2; }\n"
      "function foo() { bar(); }",
      "asm: line 1: expected >> in heap access\n");
}


TEST(BadArrayAssignment) {
  CHECK_FUNC_ERROR(
      "function bar() { i8[0] = 0.0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal type in assignment\n");
}


TEST(BadStandardFunctionCallOutside) {
  CHECK_FUNC_ERROR(
      "var s0 = sin(0);\n"
      "function bar() { }\n"
      "function foo() { bar(); }",
      "asm: line 1: illegal variable reference in module body\n");
}


TEST(BadFunctionCallOutside) {
  CHECK_FUNC_ERROR(
      "function bar() { return 0.0; }\n"
      "var s0 = bar(0);\n"
      "function foo() { bar(); }",
      "asm: line 2: illegal variable reference in module body\n");
}

TEST(UnaryPlusOnIntForbidden) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 1; return +x; }\n"
      "function foo() { bar(); }",
      "asm: line 1: "
      "unary + only allowed on signed, unsigned, float?, or double?\n");
}

TEST(MultiplyNon1ConvertForbidden) {
  CHECK_FUNC_ERROR(
      "function bar() { var x = 0.0; return x * 2.0; }\n"
      "function foo() { bar(); }",
      "asm: line 1: invalid type annotation on binary op\n");
}

TEST(NestedVariableAssignment) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 0; x = x = 4; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(NestedAssignmentInHeap) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 0; i8[(x = 1) >> 0] = 2; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_EXPR(Property, Bounds::Unbounded()) {
          CHECK_VAR(i8, Bounds(cache.kInt8Array));
          CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
            CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
              CHECK_VAR(x, Bounds(cache.kAsmInt));
              CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            }
            CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
          }
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(NegativeDouble) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = -123.2; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(NegativeInteger) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = -123; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(AbsFunction) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = -123.0; x = abs(x); }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Call, Bounds(cache.kAsmDouble)) {
          CHECK_VAR(abs, FUNC_N2N_TYPE);
          CHECK_VAR(x, Bounds(cache.kAsmDouble));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(CeilFloat) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = fround(3.1); x = ceil(x); }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(x, Bounds(cache.kAsmFloat));
        CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
          CHECK_VAR(fround, FUNC_N2F_TYPE);
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(x, Bounds(cache.kAsmFloat));
        CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
          CHECK_VAR(ceil, FUNC_N2N_TYPE);
          CHECK_VAR(x, Bounds(cache.kAsmFloat));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}

TEST(FloatReturnAsDouble) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = fround(3.1); return +fround(x); }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_D_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmFloat)) {
        CHECK_VAR(x, Bounds(cache.kAsmFloat));
        CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
          CHECK_VAR(fround, FUNC_N2F_TYPE);
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
      CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
        CHECK_EXPR(Call, Bounds(cache.kAsmFloat)) {
          CHECK_VAR(fround, FUNC_N2F_TYPE);
          CHECK_VAR(x, Bounds(cache.kAsmFloat));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}

TEST(TypeConsistency) {
  v8::V8::Initialize();
  TypeCache cache;
  // Check the consistency of each of the main Asm.js types.
  CHECK(cache.kAsmFixnum->Is(cache.kAsmFixnum));
  CHECK(cache.kAsmFixnum->Is(cache.kAsmSigned));
  CHECK(cache.kAsmFixnum->Is(cache.kAsmUnsigned));
  CHECK(cache.kAsmFixnum->Is(cache.kAsmInt));
  CHECK(!cache.kAsmFixnum->Is(cache.kAsmFloat));
  CHECK(!cache.kAsmFixnum->Is(cache.kAsmDouble));

  CHECK(cache.kAsmSigned->Is(cache.kAsmSigned));
  CHECK(cache.kAsmSigned->Is(cache.kAsmInt));
  CHECK(!cache.kAsmSigned->Is(cache.kAsmFixnum));
  CHECK(!cache.kAsmSigned->Is(cache.kAsmUnsigned));
  CHECK(!cache.kAsmSigned->Is(cache.kAsmFloat));
  CHECK(!cache.kAsmSigned->Is(cache.kAsmDouble));

  CHECK(cache.kAsmUnsigned->Is(cache.kAsmUnsigned));
  CHECK(cache.kAsmUnsigned->Is(cache.kAsmInt));
  CHECK(!cache.kAsmUnsigned->Is(cache.kAsmSigned));
  CHECK(!cache.kAsmUnsigned->Is(cache.kAsmFixnum));
  CHECK(!cache.kAsmUnsigned->Is(cache.kAsmFloat));
  CHECK(!cache.kAsmUnsigned->Is(cache.kAsmDouble));

  CHECK(cache.kAsmInt->Is(cache.kAsmInt));
  CHECK(!cache.kAsmInt->Is(cache.kAsmUnsigned));
  CHECK(!cache.kAsmInt->Is(cache.kAsmSigned));
  CHECK(!cache.kAsmInt->Is(cache.kAsmFixnum));
  CHECK(!cache.kAsmInt->Is(cache.kAsmFloat));
  CHECK(!cache.kAsmInt->Is(cache.kAsmDouble));

  CHECK(cache.kAsmFloat->Is(cache.kAsmFloat));
  CHECK(!cache.kAsmFloat->Is(cache.kAsmInt));
  CHECK(!cache.kAsmFloat->Is(cache.kAsmUnsigned));
  CHECK(!cache.kAsmFloat->Is(cache.kAsmSigned));
  CHECK(!cache.kAsmFloat->Is(cache.kAsmFixnum));
  CHECK(!cache.kAsmFloat->Is(cache.kAsmDouble));

  CHECK(cache.kAsmDouble->Is(cache.kAsmDouble));
  CHECK(!cache.kAsmDouble->Is(cache.kAsmInt));
  CHECK(!cache.kAsmDouble->Is(cache.kAsmUnsigned));
  CHECK(!cache.kAsmDouble->Is(cache.kAsmSigned));
  CHECK(!cache.kAsmDouble->Is(cache.kAsmFixnum));
  CHECK(!cache.kAsmDouble->Is(cache.kAsmFloat));
}


TEST(SwitchTest) {
  CHECK_FUNC_TYPES_BEGIN(
      "function switcher(x) {\n"
      "  x = x|0;\n"
      "  switch (x|0) {\n"
      "    case 1: return 23;\n"
      "    case 2: return 43;\n"
      "    default: return 66;\n"
      "  }\n"
      "  return 0;\n"
      "}\n"
      "function foo() { switcher(1); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_I2I_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(x, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(.switch_tag, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_VAR(x, Bounds(cache.kAsmInt));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
      CHECK_EXPR(Literal, Bounds(Type::Undefined()));
      CHECK_VAR(.switch_tag, Bounds(cache.kAsmSigned));
      // case 1: return 23;
      CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
      // case 2: return 43;
      CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
      CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
      // default: return 66;
      CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
      // return 0;
      CHECK_EXPR(Literal, Bounds(cache.kAsmSigned));
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}


TEST(BadSwitchRange) {
  CHECK_FUNC_ERROR(
      "function bar() { switch (1) { case -1: case 0x7fffffff: } }\n"
      "function foo() { bar(); }",
      "asm: line 1: case range too large\n");
}


TEST(DuplicateSwitchCase) {
  CHECK_FUNC_ERROR(
      "function bar() { switch (1) { case 0: case 0: } }\n"
      "function foo() { bar(); }",
      "asm: line 1: duplicate case value\n");
}


TEST(BadSwitchOrder) {
  CHECK_FUNC_ERROR(
      "function bar() { switch (1) { default: case 0: } }\n"
      "function foo() { bar(); }",
      "asm: line 1: default case out of order\n");
}

TEST(BadForeignCall) {
  const char test_function[] =
      "function TestModule(stdlib, foreign, buffer) {\n"
      "  \"use asm\";\n"
      "  var ffunc = foreign.foo;\n"
      "  function test1() { var x = 0; ffunc(x); }\n"
      "  return { testFunc1: test1 };\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ(
      "asm: line 4: foreign call argument expected to be int, double, or "
      "fixnum\n",
      Validate(zone, test_function, &types));
}

TEST(BadImports) {
  const char test_function[] =
      "function TestModule(stdlib, foreign, buffer) {\n"
      "  \"use asm\";\n"
      "  var fint = (foreign.bar | 0) | 0;\n"
      "  function test1() {}\n"
      "  return { testFunc1: test1 };\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 3: illegal computation inside module body\n",
           Validate(zone, test_function, &types));
}

TEST(BadVariableReference) {
  const char test_function[] =
      "function TestModule(stdlib, foreign, buffer) {\n"
      "  \"use asm\";\n"
      "  var x = 0;\n"
      "  var y = x;\n"
      "  function test1() {}\n"
      "  return { testFunc1: test1 };\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 4: illegal variable reference in module body\n",
           Validate(zone, test_function, &types));
}

TEST(BadForeignVariableReferenceValueOr) {
  const char test_function[] =
      "function TestModule(stdlib, foreign, buffer) {\n"
      "  \"use asm\";\n"
      "  var fint = foreign.bar | 1;\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 3: illegal integer annotation value\n",
           Validate(zone, test_function, &types));
}

TEST(BadForeignVariableReferenceValueOrDot) {
  const char test_function[] =
      "function TestModule(stdlib, foreign, buffer) {\n"
      "  \"use asm\";\n"
      "  var fint = foreign.bar | 1.0;\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 3: illegal integer annotation value\n",
           Validate(zone, test_function, &types));
}

TEST(BadForeignVariableReferenceValueMul) {
  const char test_function[] =
      "function TestModule(stdlib, foreign, buffer) {\n"
      "  \"use asm\";\n"
      "  var fint = foreign.bar * 2.0;\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 3: illegal double annotation value\n",
           Validate(zone, test_function, &types));
}

TEST(BadForeignVariableReferenceValueMulNoDot) {
  const char test_function[] =
      "function TestModule(stdlib, foreign, buffer) {\n"
      "  \"use asm\";\n"
      "  var fint = foreign.bar * 1;\n"
      "}\n";
  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("asm: line 3: ill-typed arithmetic operation\n",
           Validate(zone, test_function, &types));
}

TEST(Imports) {
  const char test_function[] =
      "function TestModule(stdlib, foreign, buffer) {\n"
      "  \"use asm\";\n"
      "  var ffunc = foreign.foo;\n"
      "  var fint = foreign.bar | 0;\n"
      "  var fdouble = +foreign.baz;\n"
      "  function test1() { return ffunc(fint|0, fdouble) | 0; }\n"
      "  function test2() { return +ffunc(fdouble, fint|0); }\n"
      "  return { testFunc1: test1, testFunc2: test2 };\n"
      "}\n";

  v8::V8::Initialize();
  HandleAndZoneScope handles;
  Zone* zone = handles.main_zone();
  ZoneVector<ExpressionTypeEntry> types(zone);
  CHECK_EQ("", Validate(zone, test_function, &types));
  TypeCache cache;

  CHECK_TYPES_BEGIN {
    // Module.
    CHECK_EXPR(FunctionLiteral, Bounds::Unbounded()) {
      // function test1
      CHECK_EXPR(FunctionLiteral, FUNC_I_TYPE) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(Call, Bounds(cache.kAsmSigned)) {
            CHECK_VAR(ffunc, FUNC_FOREIGN_TYPE);
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
              CHECK_VAR(fint, Bounds(cache.kAsmInt));
              CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            }
            CHECK_VAR(fdouble, Bounds(cache.kAsmDouble));
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
      // function test2
      CHECK_EXPR(FunctionLiteral, FUNC_D_TYPE) {
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
          CHECK_EXPR(Call, Bounds(cache.kAsmDouble)) {
            CHECK_VAR(ffunc, FUNC_FOREIGN_TYPE);
            CHECK_VAR(fdouble, Bounds(cache.kAsmDouble));
            CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
              CHECK_VAR(fint, Bounds(cache.kAsmInt));
              CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
            }
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
      // "use asm";
      CHECK_EXPR(Literal, Bounds(Type::String()));
      // var func = foreign.foo;
      CHECK_EXPR(Assignment, Bounds(FUNC_FOREIGN_TYPE)) {
        CHECK_VAR(ffunc, Bounds(FUNC_FOREIGN_TYPE));
        CHECK_EXPR(Property, Bounds(FUNC_FOREIGN_TYPE)) {
          CHECK_VAR(foreign, Bounds::Unbounded());
          CHECK_EXPR(Literal, Bounds::Unbounded());
        }
      }
      // var fint = foreign.bar | 0;
      CHECK_EXPR(Assignment, Bounds(cache.kAsmInt)) {
        CHECK_VAR(fint, Bounds(cache.kAsmInt));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmSigned)) {
          CHECK_EXPR(Property, Bounds(Type::Number())) {
            CHECK_VAR(foreign, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
      }
      // var fdouble = +foreign.baz;
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(fdouble, Bounds(cache.kAsmDouble));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
          CHECK_EXPR(Property, Bounds(Type::Number())) {
            CHECK_VAR(foreign, Bounds::Unbounded());
            CHECK_EXPR(Literal, Bounds::Unbounded());
          }
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
      // return { testFunc1: test1, testFunc2: test2 };
      CHECK_EXPR(ObjectLiteral, Bounds::Unbounded()) {
        CHECK_VAR(test1, FUNC_I_TYPE);
        CHECK_VAR(test2, FUNC_D_TYPE);
      }
    }
  }
  CHECK_TYPES_END
}

TEST(StoreFloatFromDouble) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { f32[0] = 0.0; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_EXPR(Property, Bounds::Unbounded()) {
          CHECK_VAR(f32, Bounds(cache.kFloat32Array));
          CHECK_EXPR(Literal, Bounds(cache.kAsmFixnum));
        }
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}

TEST(NegateDouble) {
  CHECK_FUNC_TYPES_BEGIN(
      "function bar() { var x = 0.0; x = -x; }\n"
      "function foo() { bar(); }") {
    CHECK_EXPR(FunctionLiteral, FUNC_V_TYPE) {
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
      }
      CHECK_EXPR(Assignment, Bounds(cache.kAsmDouble)) {
        CHECK_VAR(x, Bounds(cache.kAsmDouble));
        CHECK_EXPR(BinaryOperation, Bounds(cache.kAsmDouble)) {
          CHECK_VAR(x, Bounds(cache.kAsmDouble));
          CHECK_EXPR(Literal, Bounds(cache.kAsmDouble));
        }
      }
    }
    CHECK_SKIP();
  }
  CHECK_FUNC_TYPES_END
}
