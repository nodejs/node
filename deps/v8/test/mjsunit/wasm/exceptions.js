// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh

load("test/mjsunit/wasm/wasm-constants.js");
load("test/mjsunit/wasm/wasm-module-builder.js");

// The following method doesn't attempt to catch an raised exception.
var test_throw = (function () {
  var builder = new WasmModuleBuilder();

  builder.addException(kSig_v_v);

  builder.addFunction("throw_if_param_not_zero", kSig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprI32Const, 0,
        kExprI32Ne,
        kExprIf, kWasmStmt,
          kExprThrow, 0,
        kExprEnd,
        kExprI32Const, 1
      ]).exportFunc();

  return builder.instantiate();
})();

// Check the test_throw exists.
assertFalse(test_throw === undefined);
assertFalse(test_throw === null);
assertFalse(test_throw === 0);
assertEquals("object", typeof test_throw.exports);
assertEquals("function", typeof test_throw.exports.throw_if_param_not_zero);

// Test expected behavior of throws
assertEquals(1, test_throw.exports.throw_if_param_not_zero(0));
assertWasmThrows(0, [], function() { test_throw.exports.throw_if_param_not_zero(10) });
assertWasmThrows(0, [], function() { test_throw.exports.throw_if_param_not_zero(-1) });

// Now that we know throwing works, we test catching the exceptions we raise.
var test_catch = (function () {
  var builder = new WasmModuleBuilder();

  builder.addException(kSig_v_v);
  builder.addFunction("simple_throw_catch_to_0_1", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprGetLocal, 0,
          kExprI32Eqz,
          kExprIf, kWasmStmt,
            kExprThrow, 0,
          kExprEnd,
          kExprI32Const, 1,
        kExprCatch, 0,
          kExprI32Const, 0,
        kExprEnd
      ]).exportFunc();

  return builder.instantiate();
})();

// Check the test_catch exists.
assertFalse(test_catch === undefined);
assertFalse(test_catch === null);
assertFalse(test_catch === 0);
assertEquals("object", typeof test_catch.exports);
assertEquals("function", typeof test_catch.exports.simple_throw_catch_to_0_1);

// Test expected behavior of simple catch.
assertEquals(0, test_catch.exports.simple_throw_catch_to_0_1(0));
assertEquals(1, test_catch.exports.simple_throw_catch_to_0_1(1));

// Test that we can distinguish which exception was thrown.
var test_catch_2 = (function () {
  var builder = new WasmModuleBuilder();

  builder.addException(kSig_v_v);
  builder.addException(kSig_v_v);
  builder.addException(kSig_v_v);
  builder.addFunction("catch_different_exceptions", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprTry, kWasmI32,
            kExprGetLocal, 0,
            kExprI32Eqz,
            kExprIf, kWasmStmt,
              kExprThrow, 0,
            kExprElse,
              kExprGetLocal, 0,
              kExprI32Const, 1,
              kExprI32Eq,
              kExprIf, kWasmStmt,
                kExprThrow, 1,
              kExprElse,
                kExprThrow, 2,
              kExprEnd,
            kExprEnd,
            kExprI32Const, 2,
          kExprCatch, 0,
            kExprI32Const, 3,
          kExprEnd,
        kExprCatch, 1,
          kExprI32Const, 4,
        kExprEnd
      ]).exportFunc();
  return builder.instantiate();
})();

assertFalse(test_catch_2 === undefined);
assertFalse(test_catch_2 === null);
assertFalse(test_catch_2 === 0);
assertEquals("object", typeof test_catch_2.exports);
assertEquals("function", typeof test_catch_2.exports.catch_different_exceptions);

assertEquals(3, test_catch_2.exports.catch_different_exceptions(0));
assertEquals(4, test_catch_2.exports.catch_different_exceptions(1));
assertWasmThrows(2, [], function() { test_catch_2.exports.catch_different_exceptions(2) });

// Test throwing an exception with multiple values.
var test_throw_1_2 = (function() {
  var builder = new WasmModuleBuilder();
  builder.addException(kSig_v_ii);
  builder.addFunction("throw_1_2", kSig_v_v)
      .addBody([
        kExprI32Const, 1,
        kExprI32Const, 2,
        kExprThrow, 0,
      ]).exportFunc();

  return builder.instantiate();
})();

assertFalse(test_throw_1_2 === undefined);
assertFalse(test_throw_1_2 === null);
assertFalse(test_throw_1_2 === 0);
assertEquals("object", typeof test_throw_1_2.exports);
assertEquals("function", typeof test_throw_1_2.exports.throw_1_2);

assertWasmThrows(0, [0, 1, 0, 2], function() { test_throw_1_2.exports.throw_1_2(); });

// Test throwing/catching the i32 parameter value.
var test_throw_catch_param_i = (function () {
  var builder = new WasmModuleBuilder();
  builder.addException(kSig_v_i);
  builder.addFunction("throw_catch_param", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprGetLocal, 0,
          kExprThrow, 0,
          kExprI32Const, 2,
        kExprCatch, 0,
          kExprReturn,
        kExprEnd,
      ]).exportFunc();

  return builder.instantiate();
})();

assertFalse(test_throw_catch_param_i === undefined);
assertFalse(test_throw_catch_param_i === null);
assertFalse(test_throw_catch_param_i === 0);
assertEquals("object", typeof test_throw_catch_param_i.exports);
assertEquals("function",
             typeof test_throw_catch_param_i.exports.throw_catch_param);

assertEquals(0, test_throw_catch_param_i.exports.throw_catch_param(0));
assertEquals(1, test_throw_catch_param_i.exports.throw_catch_param(1));
assertEquals(10, test_throw_catch_param_i.exports.throw_catch_param(10));

// Test the encoding of a thrown exception with an integer exception.

var test_throw_param_i = (function () {
  var builder = new WasmModuleBuilder();
  builder.addException(kSig_v_i);
  builder.addFunction("throw_param", kSig_v_i)
      .addBody([
        kExprGetLocal, 0,
        kExprThrow, 0,
      ]).exportFunc();

  return builder.instantiate();
})();

assertFalse(test_throw_param_i === undefined);
assertFalse(test_throw_param_i === null);
assertFalse(test_throw_param_i === 0);
assertEquals("object", typeof test_throw_param_i.exports);
assertEquals("function",
             typeof test_throw_param_i.exports.throw_param);

assertWasmThrows(0, [0, 5], function() { test_throw_param_i.exports.throw_param(5); });
assertWasmThrows(0, [6, 31026],
                 function() { test_throw_param_i.exports.throw_param(424242); });

// Test throwing/catching the f32 parameter value.
var test_throw_catch_param_f = (function () {
  var builder = new WasmModuleBuilder();
  builder.addException(kSig_v_f);
  builder.addFunction("throw_catch_param", kSig_f_f)
      .addBody([
        kExprTry, kWasmF32,
          kExprGetLocal, 0,
          kExprThrow, 0,
          kExprF32Const, 0, 0, 0, 0,
        kExprCatch, 0,
          kExprReturn,
        kExprEnd,
      ]).exportFunc();

  return builder.instantiate();
})();

assertFalse(test_throw_catch_param_f === undefined);
assertFalse(test_throw_catch_param_f === null);
assertFalse(test_throw_catch_param_f === 0);
assertEquals("object", typeof test_throw_catch_param_f.exports);
assertEquals("function",
             typeof test_throw_catch_param_f.exports.throw_catch_param);

assertEquals(5.0, test_throw_catch_param_f.exports.throw_catch_param(5.0));
assertEquals(10.5, test_throw_catch_param_f.exports.throw_catch_param(10.5));

// Test the encoding of a thrown exception with a float value.

var test_throw_param_f = (function () {
  var builder = new WasmModuleBuilder();
  builder.addException(kSig_v_f);
  builder.addFunction("throw_param", kSig_v_f)
      .addBody([
        kExprGetLocal, 0,
        kExprThrow, 0,
      ]).exportFunc();

  return builder.instantiate();
})();

assertFalse(test_throw_param_f === undefined);
assertFalse(test_throw_param_f === null);
assertFalse(test_throw_param_f === 0);
assertEquals("object", typeof test_throw_param_f.exports);
assertEquals("function",
             typeof test_throw_param_f.exports.throw_param);

assertWasmThrows(0, [16544, 0],
                 function() { test_throw_param_f.exports.throw_param(5.0); });
assertWasmThrows(0, [16680, 0],
                 function() { test_throw_param_f.exports.throw_param(10.5); });

// Test throwing/catching an I64 value
var test_throw_catch_param_l = (function () {
  var builder = new WasmModuleBuilder();
  builder.addException(kSig_v_l);
  builder.addFunction("throw_catch_param", kSig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprI64UConvertI32,
        kExprSetLocal, 1,
        kExprTry, kWasmI32,
          kExprGetLocal, 1,
          kExprThrow, 0,
          kExprI32Const, 2,
        kExprCatch, 0,
          kExprGetLocal, 1,
          kExprI64Eq,
          kExprIf, kWasmI32,
            kExprI32Const, 1,
          kExprElse,
            kExprI32Const, 0,
          kExprEnd,
          // TODO(kschimpf): Why is this return necessary?
          kExprReturn,
        kExprEnd,
      ]).addLocals({i64_count: 1}).exportFunc();

  return builder.instantiate();
})();

assertFalse(test_throw_catch_param_l === undefined);
assertFalse(test_throw_catch_param_l === null);
assertFalse(test_throw_catch_param_l === 0);
assertEquals("object", typeof test_throw_catch_param_l.exports);
assertEquals("function",
             typeof test_throw_catch_param_l.exports.throw_catch_param);

assertEquals(1, test_throw_catch_param_l.exports.throw_catch_param(5));
assertEquals(1, test_throw_catch_param_l.exports.throw_catch_param(0));
assertEquals(1, test_throw_catch_param_l.exports.throw_catch_param(-1));

// Test the encoding of a thrown exception with an I64 value.

var test_throw_param_l = (function () {
  var builder = new WasmModuleBuilder();
  builder.addException(kSig_v_l);
  builder.addFunction("throw_param", kSig_v_ii)
      .addBody([
        kExprGetLocal, 0,
        kExprI64UConvertI32,
        kExprI64Const, 32,
        kExprI64Shl,
        kExprGetLocal, 1,
        kExprI64UConvertI32,
        kExprI64Ior,
        kExprThrow, 0
      ]).exportFunc();

  return builder.instantiate();
})();

assertFalse(test_throw_param_l === undefined);
assertFalse(test_throw_param_l === null);
assertFalse(test_throw_param_l === 0);
assertEquals("object", typeof test_throw_param_l.exports);
assertEquals("function",
             typeof test_throw_param_l.exports.throw_param);

assertWasmThrows(0, [0, 10, 0, 5],
                 function() { test_throw_param_l.exports.throw_param(10, 5); });
assertWasmThrows(0, [65535, 65535, 0, 13],
                 function() { test_throw_param_l.exports.throw_param(-1, 13); });

// Test throwing/catching the F64 parameter value
var test_throw_catch_param_d = (function () {
  var builder = new WasmModuleBuilder();
  builder.addException(kSig_v_d);
  builder.addFunction("throw_catch_param", kSig_d_d)
      .addBody([
        kExprTry, kWasmF64,
          kExprGetLocal, 0,
          kExprThrow, 0,
          kExprF64Const, 0, 0, 0, 0, 0, 0, 0, 0,
        kExprCatch, 0,
          kExprReturn,
        kExprEnd,
      ]).exportFunc();

  return builder.instantiate();
})();

assertFalse(test_throw_catch_param_d === undefined);
assertFalse(test_throw_catch_param_d === null);
assertFalse(test_throw_catch_param_d === 0);
assertEquals("object", typeof test_throw_catch_param_d.exports);
assertEquals("function",
             typeof test_throw_catch_param_d.exports.throw_catch_param);

assertEquals(5.0, test_throw_catch_param_d.exports.throw_catch_param(5.0));
assertEquals(10.5, test_throw_catch_param_d.exports.throw_catch_param(10.5));

// Test the encoding of a thrown exception with an f64 value.

var test_throw_param_d = (function () {
  var builder = new WasmModuleBuilder();
  builder.addException(kSig_v_d);
  builder.addFunction("throw_param", kSig_v_f)
      .addBody([
        kExprGetLocal, 0,
        kExprF64ConvertF32,
        kExprThrow, 0
      ]).exportFunc();

  return builder.instantiate();
})();

assertFalse(test_throw_param_d === undefined);
assertFalse(test_throw_param_d === null);
assertFalse(test_throw_param_d === 0);
assertEquals("object", typeof test_throw_param_d.exports);
assertEquals("function",
             typeof test_throw_param_d.exports.throw_param);

assertWasmThrows(0, [16404, 0, 0, 0],
                 function() { test_throw_param_d.exports.throw_param(5.0); });
assertWasmThrows(0, [16739, 4816, 0, 0],
                 function() { test_throw_param_d.exports.throw_param(10000000.5); });

/* TODO(kschimpf) Convert these tests to work for the proposed exceptions.

// The following methods do not attempt to catch the exception they raise.
var test_throw = (function () {
  var builder = new WasmModuleBuilder();

  builder.addFunction("throw_expr_with_params", kSig_v_ddi)
    .addBody([
      // p2 * (p0 + min(p0, p1))|0 - 20
      kExprGetLocal, 2,
      kExprGetLocal, 0,
      kExprGetLocal, 0,
      kExprGetLocal, 1,
      kExprF64Min,
      kExprF64Add,
      kExprI32SConvertF64,
      kExprI32Mul,
      kExprI32Const, 20,
      kExprI32Sub,
      kExprThrow,
    ])
    .exportFunc()

  return builder.instantiate();
})();

// Check the test_throw exists.
assertFalse(test_throw === undefined);
assertFalse(test_throw === null);
assertFalse(test_throw === 0);
assertEquals("object", typeof test_throw.exports);
assertEquals("function", typeof test_throw.exports.throw_expr_with_params);

assertEquals(1, test_throw.exports.throw_param_if_not_zero(0));
assertWasmThrows(
    -8, function() { test_throw.exports.throw_expr_with_params(1.5, 2.5, 4); });
assertWasmThrows(
    12, function() { test_throw.exports.throw_expr_with_params(5.7, 2.5, 4); });

// Now that we know throwing works, we test catching the exceptions we raise.
var test_catch = (function () {
  var builder = new WasmModuleBuilder();

    // Helper function for throwing from js. It is imported by the Wasm module
    // as throw_i.
    function throw_value(value) {
      throw value;
    }
    var sig_index = builder.addType(kSig_v_i);
    var kJSThrowI = builder.addImport("", "throw_i", sig_index);

    // Helper function that throws a string. Wasm should not catch it.
    function throw_string() {
      throw "use wasm;";
    }
    sig_index = builder.addType(kSig_v_v);
    var kJSThrowString = builder.addImport("", "throw_string", sig_index);

    // Helper function that throws undefined. Wasm should not catch it.
    function throw_undefined() {
      throw undefined;
    }
    var kJSThrowUndefined = builder.addImport("", "throw_undefined", sig_index);

    // Helper function that throws an fp. Wasm should not catch it.
    function throw_fp() {
      throw 10.5;
    }
    var kJSThrowFP = builder.addImport("", "throw_fp", sig_index);

    // Helper function that throws a large number. Wasm should not catch it.
    function throw_large() {
      throw 1e+28;
    }
    var kJSThrowLarge = builder.addImport("", "throw_large", sig_index);

    // Helper function for throwing from WebAssembly.
    var kWasmThrowFunction =
      builder.addFunction("throw", kSig_v_i)
        .addBody([
          kExprGetLocal, 0,
          kExprThrow
        ])
        .index;

    // Scenario 1: Throw and catch appear on the same function. This should
    // happen in case of inlining, for example.
    builder.addFunction("same_scope", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprGetLocal, 0,
          kExprI32Const, 0,
          kExprI32Ne,
          kExprIf, kWasmStmt,
            kExprGetLocal, 0,
            kExprThrow,
            kExprUnreachable,
          kExprEnd,
          kExprI32Const, 63,
        kExprCatch, 1,
          kExprGetLocal, 1,
        kExprEnd
      ])
      .addLocals({i32_count: 1})
      .exportFunc()
      .index;

    builder.addFunction("same_scope_ignore", kSig_i_i)
      .addBody([
          kExprTry, kWasmI32,
            kExprGetLocal, 0,
            kExprThrow,
            kExprUnreachable,
          kExprCatch, 1,
            kExprGetLocal, 0,
          kExprEnd,
      ])
      .addLocals({i32_count: 1})
      .exportFunc();

    builder.addFunction("same_scope_multiple", kSig_i_i)
      // path = 0;
      //
      // try {
      //   try {
      //     try {
      //       if (p == 1)
      //         throw 1;
      //       path |= 2
      //     } catch (v) {
      //       path |= v | 4;
      //       throw path;
      //     }
      //     if (p == 2)
      //       throw path|8;
      //     path |= 16;
      //   } catch (v) {
      //     path |= v | 32;
      //     throw path;
      //   }
      //   if (p == 3)
      //     throw path|64;
      //   path |= 128
      // } catch (v) {
      //   path |= v | 256;
      // }
      //
      // return path;
      //
      // p == 1 -> path == 293
      // p == 2 -> path == 298
      // p == 3 -> path == 338
      // else   -> path == 146
      .addBody([
          kExprTry, kWasmI32,
            kExprTry, kWasmI32,
              kExprTry, kWasmI32,
                kExprGetLocal, 0,
                kExprI32Const, 1,
                kExprI32Eq,
                kExprIf, kWasmStmt,
                  kExprI32Const, 1,
                  kExprThrow,
                  kExprUnreachable,
                kExprEnd,
                kExprI32Const, 2,
              kExprCatch, 1,
                kExprGetLocal, 1,
                kExprI32Const, 4,
                kExprI32Ior,
                kExprThrow,
                kExprUnreachable,
              kExprEnd,
              kExprTeeLocal, 2,
              kExprGetLocal, 0,
              kExprI32Const, 2,
              kExprI32Eq,
              kExprIf, kWasmStmt,
                kExprGetLocal, 2,
                kExprI32Const, 8,
                kExprI32Ior,
                kExprThrow,
                kExprUnreachable,
              kExprEnd,
              kExprI32Const, 16,
              kExprI32Ior,
            kExprCatch, 1,
              kExprGetLocal, 1,
              kExprI32Const, 32,
              kExprI32Ior,
              kExprThrow,
              kExprUnreachable,
            kExprEnd,
            kExprTeeLocal, 2,
            kExprGetLocal, 0,
            kExprI32Const, 3,
            kExprI32Eq,
            kExprIf, kWasmStmt,
              kExprGetLocal, 2,
              kExprI32Const, / *64=* / 192, 0,
              kExprI32Ior,
              kExprThrow,
              kExprUnreachable,
            kExprEnd,
            kExprI32Const, / *128=* / 128, 1,
            kExprI32Ior,
          kExprCatch, 1,
            kExprGetLocal, 1,
            kExprI32Const, / *256=* / 128, 2,
            kExprI32Ior,
          kExprEnd,
      ])
      .addLocals({i32_count: 2})
      .exportFunc();

    // Scenario 2: Catches an exception raised from the direct callee.
    var kFromDirectCallee =
      builder.addFunction("from_direct_callee", kSig_i_i)
        .addBody([
          kExprTry, kWasmI32,
            kExprGetLocal, 0,
            kExprCallFunction, kWasmThrowFunction,
            kExprI32Const, / *-1=* / 127,
          kExprCatch, 1,
            kExprGetLocal, 1,
          kExprEnd
        ])
        .addLocals({i32_count: 1})
        .exportFunc()
        .index;

    // Scenario 3: Catches an exception raised from an indirect callee.
    var kFromIndirectCalleeHelper = kFromDirectCallee + 1;
    builder.addFunction("from_indirect_callee_helper", kSig_v_ii)
      .addBody([
        kExprGetLocal, 0,
        kExprI32Const, 0,
        kExprI32GtS,
        kExprIf, kWasmStmt,
          kExprGetLocal, 0,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprGetLocal, 1,
          kExprI32Const, 1,
          kExprI32Sub,
          kExprCallFunction, kFromIndirectCalleeHelper,
        kExprEnd,
        kExprGetLocal, 1,
        kExprCallFunction, kWasmThrowFunction,
      ]);

    builder.addFunction("from_indirect_callee", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprGetLocal, 0,
          kExprI32Const, 0,
          kExprCallFunction, kFromIndirectCalleeHelper,
          kExprI32Const, / *-1=* / 127,
        kExprCatch, 1,
          kExprGetLocal, 1,
        kExprEnd
      ])
      .addLocals({i32_count: 1})
      .exportFunc();

    // Scenario 4: Catches an exception raised in JS.
    builder.addFunction("from_js", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprGetLocal, 0,
          kExprCallFunction, kJSThrowI,
          kExprI32Const, / *-1=* / 127,
        kExprCatch, 1,
          kExprGetLocal, 1,
        kExprEnd,
      ])
      .addLocals({i32_count: 1})
      .exportFunc();

    // Scenario 5: Does not catch an exception raised in JS if it is not a
    // number.
    builder.addFunction("string_from_js", kSig_v_v)
      .addBody([
          kExprCallFunction, kJSThrowString
      ])
      .exportFunc();

    builder.addFunction("fp_from_js", kSig_v_v)
      .addBody([
          kExprCallFunction, kJSThrowFP
      ])
      .exportFunc();

    builder.addFunction("large_from_js", kSig_v_v)
      .addBody([
          kExprCallFunction, kJSThrowLarge
      ])
      .exportFunc();

    builder.addFunction("undefined_from_js", kSig_v_v)
      .addBody([
          kExprCallFunction, kJSThrowUndefined
      ])
      .exportFunc();

  return builder.instantiate({"": {
      throw_i: throw_value,
      throw_string: throw_string,
      throw_fp: throw_fp,
      throw_large, throw_large,
      throw_undefined: throw_undefined
  }});
})();

// Check the test_catch exists.
assertFalse(test_catch === undefined);
assertFalse(test_catch === null);
assertFalse(test_catch === 0);
assertEquals("object", typeof test_catch.exports);
assertEquals("function", typeof test_catch.exports.same_scope);
assertEquals("function", typeof test_catch.exports.same_scope_ignore);
assertEquals("function", typeof test_catch.exports.same_scope_multiple);
assertEquals("function", typeof test_catch.exports.from_direct_callee);
assertEquals("function", typeof test_catch.exports.from_indirect_callee);
assertEquals("function", typeof test_catch.exports.from_js);
assertEquals("function", typeof test_catch.exports.string_from_js);

assertEquals(63, test_catch.exports.same_scope(0));
assertEquals(1024, test_catch.exports.same_scope(1024));
assertEquals(-3, test_catch.exports.same_scope(-3));
assertEquals(-1, test_catch.exports.same_scope_ignore(-1));
assertEquals(1, test_catch.exports.same_scope_ignore(1));
assertEquals(0x7FFFFFFF, test_catch.exports.same_scope_ignore(0x7FFFFFFF));
assertEquals(1024, test_catch.exports.same_scope_ignore(1024));
assertEquals(-1, test_catch.exports.same_scope_ignore(-1));
assertEquals(293, test_catch.exports.same_scope_multiple(1));
assertEquals(298, test_catch.exports.same_scope_multiple(2));
assertEquals(338, test_catch.exports.same_scope_multiple(3));
assertEquals(146, test_catch.exports.same_scope_multiple(0));
assertEquals(-10024, test_catch.exports.from_direct_callee(-10024));
assertEquals(3334333, test_catch.exports.from_direct_callee(3334333));
assertEquals(-1, test_catch.exports.from_direct_callee(0xFFFFFFFF));
assertEquals(0x7FFFFFFF, test_catch.exports.from_direct_callee(0x7FFFFFFF));
assertEquals(-10, test_catch.exports.from_indirect_callee(10));
assertEquals(-77, test_catch.exports.from_indirect_callee(77));
assertEquals(10, test_catch.exports.from_js(10));
assertEquals(-10, test_catch.exports.from_js(-10));

assertThrowsEquals(test_catch.exports.string_from_js, "use wasm;");
assertThrowsEquals(test_catch.exports.large_from_js, 1e+28);
assertThrowsEquals(test_catch.exports.undefined_from_js, undefined);
*/
