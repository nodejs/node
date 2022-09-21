// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-inlining --no-liftoff --experimental-wasm-return-call
// Flags: --experimental-wasm-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// TODO(12166): Consider running tests with --trace-wasm and inspecting their
// output, or implementing testing infrastructure with --allow-natives-syntax.

(function SimpleInliningTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // f(x) = x - 1
  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub]);
  // g(x) = f(5) + x
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprI32Const, 5, kExprCallFunction, callee.index,
              kExprLocalGet, 0, kExprI32Add])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(14, instance.exports.main(10));
})();

(function MultiReturnTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // f(x) = (x - 1, x + 1)
  let callee = builder.addFunction("callee", kSig_ii_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub,
              kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add]);
  // g(x) = { let (a, b) = f(x); a * b}
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprCallFunction, callee.index, kExprI32Mul])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(9 * 11, instance.exports.main(10));
})();

(function NoReturnTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let global = builder.addGlobal(kWasmI32, true);

  let callee = builder.addFunction("callee", kSig_v_i)
    .addBody([kExprLocalGet, 0, kExprGlobalSet, global.index]);

  builder.addFunction("main", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprCallFunction, callee.index,
              kExprGlobalGet, global.index])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(10, instance.exports.main(10));
})();

(function LoopInLoopTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let fact = builder.addFunction("fact", kSig_i_i)
    .addLocals(kWasmI32, 1)
    .addBody([// result = 1;
              kExprI32Const, 1, kExprLocalSet, 1,
              kExprLoop, kWasmVoid,
                kExprLocalGet, 1,
                // if input == 1 return result;
                kExprLocalGet, 0, kExprI32Const, 1, kExprI32Eq, kExprBrIf, 1,
                // result *= input;
                kExprLocalGet, 0, kExprI32Mul, kExprLocalSet, 1,
                // input -= 1;
                kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub,
                kExprLocalSet, 0,
                kExprBr, 0,
              kExprEnd,
              kExprUnreachable]);

  builder.addFunction("main", kSig_i_i)
    .addLocals(kWasmI32, 1)
    .addBody([
      kExprLoop, kWasmVoid,
        kExprLocalGet, 1,
        // if input == 0 return sum;
        kExprLocalGet, 0, kExprI32Const, 0, kExprI32Eq, kExprBrIf, 1,
        // sum += fact(input);
        kExprLocalGet, 0, kExprCallFunction, fact.index,
        kExprI32Add, kExprLocalSet, 1,
        // input -= 1;
        kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub,
        kExprLocalSet, 0,
        kExprBr, 0,
      kExprEnd,
      kExprUnreachable])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(33, instance.exports.main(4));
})();

(function InfiniteLoopTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLoop, kWasmVoid,
                kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add,
                kExprLocalSet, 0, kExprBr, 0,
              kExprEnd,
              kExprLocalGet, 0]);

  builder.addFunction("main", kSig_i_i)
    .addBody([kExprI32Const, 5, kExprCallFunction, callee.index,
              kExprLocalGet, 0, kExprI32Add])
    .exportAs("main");

  builder.instantiate();
})();

(function TailCallInCalleeTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // f(x) = g(x - 1)
  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub,
              kExprReturnCall, 1]);
  // g(x) = x * 2
  builder.addFunction("inner_callee", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul]);
  // h(x) = f(x) + 5
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprCallFunction, callee.index,
              kExprI32Const, 5, kExprI32Add])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(23, instance.exports.main(10));
})();

(function MultipleCallAndReturnSitesTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // f(x) = x >= 0 ? x - 1 : x + 1
  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 0, kExprI32GeS,
              kExprIf, kWasmI32,
                kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub,
              kExprElse,
                kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add,
              kExprEnd]);
  // g(x) = f(x) * f(-x)
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprCallFunction, callee.index,
              kExprI32Const, 0, kExprLocalGet, 0, kExprI32Sub,
              kExprCallFunction, callee.index,
              kExprI32Mul])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(-81, instance.exports.main(10));
})();

(function TailCallInCallerTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // f(x) = x > 0 ? g(x) + 1: g(x - 1);
  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 0, kExprI32GeS,
              kExprIf, kWasmI32,
                kExprLocalGet, 0, kExprCallFunction, 1, kExprI32Const, 1,
                kExprI32Add,
              kExprElse,
                kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub,
                kExprReturnCall, 1,
              kExprEnd]);
  // g(x) = x * 2
  builder.addFunction("inner_callee", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 2, kExprI32Mul]);
  // h(x) = f(x + 5)
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 5, kExprI32Add,
              kExprReturnCall, callee.index])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(31, instance.exports.main(10));
  assertEquals(-12, instance.exports.main(-10));
})();

(function HandledInHandledTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let tag = builder.addTag(kSig_v_i);

  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprTry, kWasmI32,
                kExprI32Const, 42,
                kExprThrow, tag,
              kExprCatchAll,
                kExprLocalGet, 0,
              kExprEnd]);

  builder.addFunction("main", kSig_i_ii)
    .addBody([kExprTry, kWasmI32,
                kExprLocalGet, 0,
                kExprCallFunction, callee.index,
              kExprCatchAll,
                kExprLocalGet, 1,
              kExprEnd])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(10, instance.exports.main(10, 20));
})();

(function HandledInUnhandledTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let tag = builder.addTag(kSig_v_i);

  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprTry, kWasmI32,
                kExprI32Const, 42,
                kExprThrow, tag,
              kExprCatchAll,
                kExprLocalGet, 0,
              kExprEnd]);

  builder.addFunction("main", kSig_i_ii)
    .addBody([kExprLocalGet, 0,
              kExprCallFunction, callee.index,])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(10, instance.exports.main(10, 20));
})();

(function UnhandledInUnhandledTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let tag = builder.addTag(kSig_v_i);

  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([kExprI32Const, 42, kExprThrow, tag]);

  builder.addFunction("main", kSig_i_ii)
    .addBody([kExprLocalGet, 0,
              kExprCallFunction, callee.index])
    .exportAs("main");

  let instance = builder.instantiate();
  assertThrows(() => instance.exports.main(10, 20), WebAssembly.Exception);
})();

// This is the most interesting of the exception tests, as it requires rewiring
// the unhandled calls in the callee (including the 'throw' builtin) to the
// handler in the caller.
(function UnhandledInHandledTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let tag = builder.addTag(kSig_v_i);

  let callee = builder.addFunction("callee", kSig_i_i)
    .addBody([
      kExprLocalGet, 0,
      kExprIf, kWasmI32,
        kExprLocalGet, 0, kExprThrow, tag,
      kExprElse,
        kExprCallFunction, 1,
      kExprEnd]);

  builder.addFunction("unreachable", kSig_i_v)
    .addBody([kExprUnreachable]);

  builder.addFunction("main", kSig_i_ii)
    .addBody([kExprTry, kWasmI32,
                kExprLocalGet, 0,
                kExprCallFunction, callee.index,
              kExprCatchAll,
                kExprLocalGet, 1,
              kExprEnd])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(20, instance.exports.main(10, 20));
})();

(function LoopUnrollingTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();

  // f(x, y) = { do { y += 1; x -= 1; } while (x > 0); return y; }
  let callee = builder.addFunction("callee", kSig_i_ii)
    .addBody([
      kExprLoop, kWasmVoid,
        kExprLocalGet, 1, kExprI32Const, 1, kExprI32Add, kExprLocalSet, 1,
        kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub, kExprLocalSet, 0,
        kExprLocalGet, 0, kExprI32Const, 0, kExprI32GtS, kExprBrIf, 0,
      kExprEnd,
      kExprLocalGet, 1
    ]);
  // g(x) = f(5, x) + x
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprI32Const, 5, kExprLocalGet, 0,
              kExprCallFunction, callee.index,
              kExprLocalGet, 0, kExprI32Add])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(25, instance.exports.main(10));
})();

(function ThrowInLoopTest() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let tag = builder.addTag(kSig_v_i);

  // f(x, y) {
  //   do {
  //     if (x < 0) throw x;
  //     y++; x--;
  //   } while (x > 0);
  //   return y;
  // }
  let callee = builder.addFunction("callee", kSig_i_ii)
    .addBody([
      kExprLoop, kWasmVoid,
        kExprLocalGet, 0, kExprI32Const, 0, kExprI32LtS,
        kExprIf, kWasmVoid,
          kExprLocalGet, 0, kExprThrow, tag,
        kExprEnd,
        kExprLocalGet, 1, kExprI32Const, 1, kExprI32Add, kExprLocalSet, 1,
        kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub, kExprLocalSet, 0,
        kExprLocalGet, 0, kExprI32Const, 0, kExprI32GtS, kExprBrIf, 0,
      kExprEnd,
      kExprLocalGet, 1
    ]);
  // g(x) = (try { f(x, 5) } catch(x) { x }) + x
  builder.addFunction("main", kSig_i_i)
    .addBody([kExprTry, kWasmI32,
              kExprLocalGet, 0, kExprI32Const, 5,
                kExprCallFunction, callee.index,
              kExprCatch, tag,
              kExprEnd,
              kExprLocalGet, 0, kExprI32Add])
    .exportAs("main");

  let instance = builder.instantiate();
  assertEquals(25, instance.exports.main(10));
  assertEquals(-20, instance.exports.main(-10));
})();

(function InlineSubtypeSignatureTest() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);

  let callee = builder
    .addFunction("callee", makeSig([wasmRefNullType(struct)], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0]);

  // When inlining "callee", TF should pass the real parameter type (ref 0) and
  // thus eliminate the null check for struct.get.
  builder.addFunction("main", makeSig([wasmRefType(struct)], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kExprCallFunction, callee.index])
    .exportFunc();

  builder.instantiate({});
})();

(function InliningAndEscapeAnalysisTest() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();
  let struct = builder.addStruct([makeField(kWasmI32, true)]);

  let callee = builder
    .addFunction("callee", makeSig([wasmRefNullType(struct)], [kWasmI32]))
    .addBody([kExprLocalGet, 0, kGCPrefix, kExprStructGet, struct, 0]);

  // The allocation should be removed.
  builder.addFunction("main", kSig_i_i)
    .addBody([
      kExprLocalGet, 0, kExprI32Const, 1, kExprI32Add,
      kGCPrefix, kExprStructNew, struct,
      kExprCallFunction, callee.index])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(11, instance.exports.main(10));
})();

(function Int64Lowering() {
  print(arguments.callee.name);

  let kSig_l_li = makeSig([kWasmI64, kWasmI32], [kWasmI64]);

  let builder = new WasmModuleBuilder();

  let callee = builder.addFunction("callee", kSig_l_li)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprI64SConvertI32, kExprI64Add]);

  builder.addFunction("main", kSig_l_li)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprCallFunction, callee.index])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(BigInt(21), instance.exports.main(BigInt(10), 11));
})();

(function InliningRecursiveTest() {
  print(arguments.callee.name);

  let builder = new WasmModuleBuilder();

  let factorial = builder
    .addFunction("factorial", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprI32Const, 1, kExprI32LeS,
              kExprIf, kWasmVoid, kExprI32Const, 1, kExprReturn, kExprEnd,
              kExprLocalGet, 0, kExprI32Const, 1, kExprI32Sub,
              kExprCallFunction, 0,
              kExprLocalGet, 0, kExprI32Mul]);

  builder.addFunction("main", kSig_i_i)
    .addBody([kExprLocalGet, 0, kExprCallFunction, factorial.index])
    .exportFunc();

  let instance = builder.instantiate({});
  assertEquals(1, instance.exports.main(1));
  // {factorial} should not be fully inlined in the trace.
  assertEquals(120, instance.exports.main(5));
})();
