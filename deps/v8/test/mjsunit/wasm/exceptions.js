// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-wasm --experimental-wasm-eh --allow-natives-syntax

load("test/mjsunit/wasm/wasm-module-builder.js");
load("test/mjsunit/wasm/exceptions-utils.js");

// First we just test that "exnref" local variables are allowed.
(function TestLocalExnRef() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("push_and_drop_exnref", kSig_v_v)
      .addLocals({except_count: 1})
      .addBody([
        kExprGetLocal, 0,
        kExprDrop,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertDoesNotThrow(instance.exports.push_and_drop_exnref);
})();

// The following method doesn't attempt to catch an raised exception.
(function TestThrowSimple() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_v);
  builder.addFunction("throw_if_param_not_zero", kSig_i_i)
      .addBody([
        kExprGetLocal, 0,
        kExprI32Const, 0,
        kExprI32Ne,
        kExprIf, kWasmStmt,
          kExprThrow, except,
        kExprEnd,
        kExprI32Const, 1
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(1, instance.exports.throw_if_param_not_zero(0));
  assertWasmThrows(instance, except, [], () => instance.exports.throw_if_param_not_zero(10));
  assertWasmThrows(instance, except, [], () => instance.exports.throw_if_param_not_zero(-1));
})();

// Test that empty try/catch blocks work.
(function TestCatchEmptyBlocks() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  builder.addFunction("catch_empty_try", kSig_v_v)
      .addBody([
        kExprTry, kWasmStmt,
        kExprCatch,
          kExprDrop,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertDoesNotThrow(instance.exports.catch_empty_try);
})();

// Now that we know throwing works, we test catching the exceptions we raise.
(function TestCatchSimple() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_v);
  builder.addFunction("simple_throw_catch_to_0_1", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprGetLocal, 0,
          kExprI32Eqz,
          kExprIf, kWasmStmt,
            kExprThrow, except,
          kExprEnd,
          kExprI32Const, 42,
        kExprCatch,
          kExprDrop,
          kExprI32Const, 23,
        kExprEnd
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(23, instance.exports.simple_throw_catch_to_0_1(0));
  assertEquals(42, instance.exports.simple_throw_catch_to_0_1(1));
})();

// Test that we can distinguish which exception was thrown by using a cascaded
// sequence of nested try blocks with a single handler in each catch block.
(function TestCatchComplex1() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  let except2 = builder.addException(kSig_v_v);
  let except3 = builder.addException(kSig_v_v);
  builder.addFunction("catch_complex", kSig_i_i)
      .addBody([
        kExprBlock, kWasmStmt,
          kExprBlock, kWasmStmt,
            kExprTry, kWasmStmt,
              kExprTry, kWasmStmt,
                kExprGetLocal, 0,
                kExprI32Eqz,
                kExprIf, kWasmStmt,
                  kExprThrow, except1,
                kExprElse,
                  kExprGetLocal, 0,
                  kExprI32Const, 1,
                  kExprI32Eq,
                  kExprIf, kWasmStmt,
                    kExprThrow, except2,
                  kExprElse,
                    kExprThrow, except3,
                  kExprEnd,
                kExprEnd,
                kExprI32Const, 2,
                kExprReturn,
              kExprCatch,
                kExprBrOnExn, 2, except1,
                kExprRethrow,
              kExprEnd,
            kExprCatch,
              kExprBrOnExn, 2, except2,
              kExprRethrow,
            kExprEnd,
          kExprEnd,
          kExprI32Const, 3,
          kExprReturn,
        kExprEnd,
        kExprI32Const, 4,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(3, instance.exports.catch_complex(0));
  assertEquals(4, instance.exports.catch_complex(1));
  assertWasmThrows(instance, except3, [], () => instance.exports.catch_complex(2));
})();

// Test that we can distinguish which exception was thrown by using a single
// try block with multiple handlers in the associated catch block.
(function TestCatchComplex2() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except1 = builder.addException(kSig_v_v);
  let except2 = builder.addException(kSig_v_v);
  let except3 = builder.addException(kSig_v_v);
  builder.addFunction("catch_complex", kSig_i_i)
      .addBody([
        kExprBlock, kWasmStmt,
          kExprBlock, kWasmStmt,
            kExprTry, kWasmStmt,
              kExprGetLocal, 0,
              kExprI32Eqz,
              kExprIf, kWasmStmt,
                kExprThrow, except1,
              kExprElse,
                kExprGetLocal, 0,
                kExprI32Const, 1,
                kExprI32Eq,
                kExprIf, kWasmStmt,
                  kExprThrow, except2,
                kExprElse,
                  kExprThrow, except3,
                kExprEnd,
              kExprEnd,
              kExprI32Const, 2,
              kExprReturn,
            kExprCatch,
              kExprBrOnExn, 1, except1,
              kExprBrOnExn, 2, except2,
              kExprRethrow,
            kExprEnd,
          kExprEnd,
          kExprI32Const, 3,
          kExprReturn,
        kExprEnd,
        kExprI32Const, 4,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(3, instance.exports.catch_complex(0));
  assertEquals(4, instance.exports.catch_complex(1));
  assertWasmThrows(instance, except3, [], () => instance.exports.catch_complex(2));
})();

// Test that br-on-exn also is allowed to consume values already present on the
// operand stack, instead of solely values being pushed by the branch itself.
(function TestCatchBranchWithValueOnStack() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_v);
  builder.addFunction("catch_complex", kSig_i_i)
      .addLocals({except_count: 1})
      .addBody([
        kExprBlock, kWasmI32,
          kExprTry, kWasmStmt,
            kExprGetLocal, 0,
            kExprI32Eqz,
            kExprIf, kWasmStmt,
              kExprThrow, except,
            kExprEnd,
          kExprCatch,
            kExprSetLocal, 1,
            kExprI32Const, 23,
            kExprGetLocal, 1,
            kExprBrOnExn, 1, except,
            kExprRethrow,
          kExprEnd,
          kExprI32Const, 42,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(23, instance.exports.catch_complex(0));
  assertEquals(42, instance.exports.catch_complex(1));
})();

// Test throwing an exception with multiple values.
(function TestThrowMultipleValues() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_ii);
  builder.addFunction("throw_1_2", kSig_v_v)
      .addBody([
        kExprI32Const, 1,
        kExprI32Const, 2,
        kExprThrow, except,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [0, 1, 0, 2], () => instance.exports.throw_1_2());
})();

// Test throwing/catching the i32 parameter value.
(function TestThrowCatchParamI() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_i);
  builder.addFunction("throw_catch_param", kSig_i_i)
      .addBody([
        kExprTry, kWasmI32,
          kExprGetLocal, 0,
          kExprThrow, except,
          kExprI32Const, 2,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(0, instance.exports.throw_catch_param(0));
  assertEquals(1, instance.exports.throw_catch_param(1));
  assertEquals(10, instance.exports.throw_catch_param(10));
})();

// Test the encoding of a thrown exception with an integer exception.
(function TestThrowParamI() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_i);
  builder.addFunction("throw_param", kSig_v_i)
      .addBody([
        kExprGetLocal, 0,
        kExprThrow, except,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [0, 5], () => instance.exports.throw_param(5));
  assertWasmThrows(instance, except, [6, 31026], () => instance.exports.throw_param(424242));
})();

// Test throwing/catching the f32 parameter value.
(function TestThrowCatchParamF() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_f);
  builder.addFunction("throw_catch_param", kSig_f_f)
      .addBody([
        kExprTry, kWasmF32,
          kExprGetLocal, 0,
          kExprThrow, except,
          kExprF32Const, 0, 0, 0, 0,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(5.0, instance.exports.throw_catch_param(5.0));
  assertEquals(10.5, instance.exports.throw_catch_param(10.5));
})();

// Test the encoding of a thrown exception with a float value.
(function TestThrowParamF() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_f);
  builder.addFunction("throw_param", kSig_v_f)
      .addBody([
        kExprGetLocal, 0,
        kExprThrow, except,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [16544, 0], () => instance.exports.throw_param(5.0));
  assertWasmThrows(instance, except, [16680, 0], () => instance.exports.throw_param(10.5));
})();

// Test throwing/catching an I64 value
(function TestThrowCatchParamL() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_l);
  builder.addFunction("throw_catch_param", kSig_i_i)
      .addLocals({i64_count: 1})
      .addBody([
        kExprGetLocal, 0,
        kExprI64UConvertI32,
        kExprSetLocal, 1,
        kExprTry, kWasmI64,
          kExprGetLocal, 1,
          kExprThrow, except,
          kExprI64Const, 23,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
        kExprGetLocal, 1,
        kExprI64Eq,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(1, instance.exports.throw_catch_param(5));
  assertEquals(1, instance.exports.throw_catch_param(0));
  assertEquals(1, instance.exports.throw_catch_param(-1));
})();

// Test the encoding of a thrown exception with an I64 value.
(function TestThrowParamL() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_l);
  builder.addFunction("throw_param", kSig_v_ii)
      .addBody([
        kExprGetLocal, 0,
        kExprI64UConvertI32,
        kExprI64Const, 32,
        kExprI64Shl,
        kExprGetLocal, 1,
        kExprI64UConvertI32,
        kExprI64Ior,
        kExprThrow, except,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [0, 10, 0, 5], () => instance.exports.throw_param(10, 5));
  assertWasmThrows(instance, except, [65535, 65535, 0, 13], () => instance.exports.throw_param(-1, 13));
})();

// Test throwing/catching the F64 parameter value
(function TestThrowCatchParamD() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_d);
  builder.addFunction("throw_catch_param", kSig_d_d)
      .addBody([
        kExprTry, kWasmF64,
          kExprGetLocal, 0,
          kExprThrow, except,
          kExprF64Const, 0, 0, 0, 0, 0, 0, 0, 0,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertEquals(5.0, instance.exports.throw_catch_param(5.0));
  assertEquals(10.5, instance.exports.throw_catch_param(10.5));
})();

// Test the encoding of a thrown exception with an f64 value.
(function TestThrowParamD() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_d);
  builder.addFunction("throw_param", kSig_v_f)
      .addBody([
        kExprGetLocal, 0,
        kExprF64ConvertF32,
        kExprThrow, except,
      ]).exportFunc();
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [16404, 0, 0, 0], () => instance.exports.throw_param(5.0));
  assertWasmThrows(instance, except, [16739, 4816, 0, 0], () => instance.exports.throw_param(10000000.5));
})();

// Test the encoding of a computed parameter value.
(function TestThrowParamComputed() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_i);
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
        kExprThrow, except,
      ]).exportFunc()
  let instance = builder.instantiate();

  assertWasmThrows(instance, except, [65535, 65536-8], () => instance.exports.throw_expr_with_params(1.5, 2.5, 4));
  assertWasmThrows(instance, except, [0, 12], () => instance.exports.throw_expr_with_params(5.7, 2.5, 4));
})();

// Now that we know catching works locally, we test catching exceptions that
// cross function boundaries and/or raised by JavaScript.
(function TestCatchCrossFunctions() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let except = builder.addException(kSig_v_i);

  // Helper function for throwing from JS. It is imported by the Wasm module
  // as throw_i.
  function throw_value(value) {
    throw value;
  }
  let sig_index = builder.addType(kSig_v_i);
  let kJSThrowI = builder.addImport("", "throw_i", sig_index);

  // Helper function that throws a string. Wasm should not catch it.
  function throw_string() {
    throw "use wasm";
  }
  sig_index = builder.addType(kSig_v_v);
  let kJSThrowString = builder.addImport("", "throw_string", sig_index);

  // Helper function that throws undefined. Wasm should not catch it.
  function throw_undefined() {
    throw undefined;
  }
  let kJSThrowUndefined = builder.addImport("", "throw_undefined", sig_index);

  // Helper function that throws an fp. Wasm should not catch it.
  function throw_fp() {
    throw 10.5;
  }
  let kJSThrowFP = builder.addImport("", "throw_fp", sig_index);

  // Helper function that throws a large number. Wasm should not catch it.
  function throw_large() {
    throw 1e+28;
  }
  let kJSThrowLarge = builder.addImport("", "throw_large", sig_index);

  // Helper function for throwing from WebAssembly.
  let kWasmThrowFunction =
    builder.addFunction("throw", kSig_v_i)
      .addBody([
        kExprGetLocal, 0,
        kExprThrow, except,
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
          kExprThrow, except,
          kExprUnreachable,
        kExprEnd,
        kExprI32Const, 63,
      kExprCatch,
        kExprBrOnExn, 0, except,
        kExprRethrow,
      kExprEnd
    ])
    .exportFunc();

  builder.addFunction("same_scope_ignore", kSig_i_i)
    .addBody([
        kExprTry, kWasmI32,
          kExprGetLocal, 0,
          kExprThrow, except,
          kExprUnreachable,
        kExprCatch,
          kExprBrOnExn, 0, except,
          kExprRethrow,
        kExprEnd,
    ])
    .exportFunc();

  builder.addFunction("same_scope_multiple", kSig_i_i)
    .addLocals({i32_count: 1, except_count: 1})
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
                kExprThrow, except,
                kExprUnreachable,
              kExprEnd,
              kExprI32Const, 2,
            kExprCatch,
              kExprSetLocal, 2,
              kExprBlock, kWasmI32,
                kExprGetLocal, 2,
                kExprBrOnExn, 0, except,
                kExprRethrow,
              kExprEnd,
              kExprI32Const, 4,
              kExprI32Ior,
              kExprThrow, except,
              kExprUnreachable,
            kExprEnd,
            kExprTeeLocal, 1,
            kExprGetLocal, 0,
            kExprI32Const, 2,
            kExprI32Eq,
            kExprIf, kWasmStmt,
              kExprGetLocal, 1,
              kExprI32Const, 8,
              kExprI32Ior,
              kExprThrow, except,
              kExprUnreachable,
            kExprEnd,
            kExprI32Const, 16,
            kExprI32Ior,
          kExprCatch,
            kExprSetLocal, 2,
            kExprBlock, kWasmI32,
              kExprGetLocal, 2,
              kExprBrOnExn, 0, except,
              kExprRethrow,
            kExprEnd,
            kExprI32Const, 32,
            kExprI32Ior,
            kExprThrow, except,
            kExprUnreachable,
          kExprEnd,
          kExprTeeLocal, 1,
          kExprGetLocal, 0,
          kExprI32Const, 3,
          kExprI32Eq,
          kExprIf, kWasmStmt,
            kExprGetLocal, 1,
            kExprI32Const, /*64=*/ 192, 0,
            kExprI32Ior,
            kExprThrow, except,
            kExprUnreachable,
          kExprEnd,
          kExprI32Const, /*128=*/ 128, 1,
          kExprI32Ior,
        kExprCatch,
          kExprSetLocal, 2,
          kExprBlock, kWasmI32,
            kExprGetLocal, 2,
            kExprBrOnExn, 0, except,
            kExprRethrow,
          kExprEnd,
          kExprI32Const, /*256=*/ 128, 2,
          kExprI32Ior,
        kExprEnd,
    ])
    .exportFunc();

  // Scenario 2: Catches an exception raised from the direct callee.
  builder.addFunction("from_direct_callee", kSig_i_i)
    .addBody([
      kExprTry, kWasmI32,
        kExprGetLocal, 0,
        kExprCallFunction, kWasmThrowFunction,
        kExprUnreachable,
      kExprCatch,
        kExprBrOnExn, 0, except,
        kExprRethrow,
      kExprEnd,
    ])
    .exportFunc();

  // Scenario 3: Catches an exception raised from an indirect callee.
  let sig_v_i = builder.addType(kSig_v_i);
  builder.appendToTable([kWasmThrowFunction, kWasmThrowFunction]);
  builder.addFunction("from_indirect_callee", kSig_i_ii)
    .addBody([
      kExprTry, kWasmI32,
        kExprGetLocal, 0,
        kExprGetLocal, 1,
        kExprCallIndirect, sig_v_i, kTableZero,
        kExprUnreachable,
      kExprCatch,
        kExprBrOnExn, 0, except,
        kExprRethrow,
      kExprEnd
    ])
    .exportFunc();

  // Scenario 4: Does not catch an exception raised in JS, even if primitive
  // values are being used as exceptions.
  builder.addFunction("i_from_js", kSig_i_i)
    .addBody([
      kExprTry, kWasmI32,
        kExprGetLocal, 0,
        kExprCallFunction, kJSThrowI,
        kExprUnreachable,
      kExprCatch,
        kExprBrOnExn, 0, except,
        kExprRethrow,
      kExprEnd,
      kExprUnreachable,
    ])
    .exportFunc();

  builder.addFunction("string_from_js", kSig_v_v)
    .addBody([
      kExprTry, kWasmStmt,
        kExprCallFunction, kJSThrowString,
      kExprCatch,
        kExprBrOnExn, 0, except,
        kExprRethrow,
      kExprEnd,
      kExprUnreachable,
    ])
    .exportFunc();

  builder.addFunction("fp_from_js", kSig_v_v)
    .addBody([
      kExprTry, kWasmStmt,
        kExprCallFunction, kJSThrowFP,
      kExprCatch,
        kExprBrOnExn, 0, except,
        kExprRethrow,
      kExprEnd,
      kExprUnreachable,
    ])
    .exportFunc();

  builder.addFunction("large_from_js", kSig_v_v)
    .addBody([
      kExprTry, kWasmStmt,
        kExprCallFunction, kJSThrowLarge,
      kExprCatch,
        kExprBrOnExn, 0, except,
        kExprRethrow,
      kExprEnd,
      kExprUnreachable,
    ])
    .exportFunc();

  builder.addFunction("undefined_from_js", kSig_v_v)
    .addBody([
      kExprTry, kWasmStmt,
        kExprCallFunction, kJSThrowUndefined,
      kExprCatch,
        kExprBrOnExn, 0, except,
        kExprRethrow,
      kExprEnd,
      kExprUnreachable,
    ])
    .exportFunc();

  let instance = builder.instantiate({"": {
      throw_i: throw_value,
      throw_string: throw_string,
      throw_fp: throw_fp,
      throw_large, throw_large,
      throw_undefined: throw_undefined
  }});

  assertEquals(63, instance.exports.same_scope(0));
  assertEquals(1024, instance.exports.same_scope(1024));
  assertEquals(-3, instance.exports.same_scope(-3));
  assertEquals(-1, instance.exports.same_scope_ignore(-1));
  assertEquals(1, instance.exports.same_scope_ignore(1));
  assertEquals(0x7FFFFFFF, instance.exports.same_scope_ignore(0x7FFFFFFF));
  assertEquals(1024, instance.exports.same_scope_ignore(1024));
  assertEquals(-1, instance.exports.same_scope_ignore(-1));
  assertEquals(293, instance.exports.same_scope_multiple(1));
  assertEquals(298, instance.exports.same_scope_multiple(2));
  assertEquals(338, instance.exports.same_scope_multiple(3));
  assertEquals(146, instance.exports.same_scope_multiple(0));
  assertEquals(-10024, instance.exports.from_direct_callee(-10024));
  assertEquals(3334333, instance.exports.from_direct_callee(3334333));
  assertEquals(-1, instance.exports.from_direct_callee(0xFFFFFFFF));
  assertEquals(0x7FFFFFFF, instance.exports.from_direct_callee(0x7FFFFFFF));
  assertEquals(10, instance.exports.from_indirect_callee(10, 0));
  assertEquals(77, instance.exports.from_indirect_callee(77, 1));

  assertThrowsEquals(() => instance.exports.i_from_js(10), 10);
  assertThrowsEquals(() => instance.exports.i_from_js(-10), -10);
  assertThrowsEquals(instance.exports.string_from_js, "use wasm");
  assertThrowsEquals(instance.exports.fp_from_js, 10.5);
  assertThrowsEquals(instance.exports.large_from_js, 1e+28);
  assertThrowsEquals(instance.exports.undefined_from_js, undefined);
})();
