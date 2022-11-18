// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-stringref --allow-natives-syntax
// We just want speculative inlining, but the "stress" variant doesn't like
// that flag for some reason, so use the GC flag which implies it.
// Flags: --experimental-wasm-gc

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let kSig_w_v = makeSig([], [kWasmStringRef]);
let kSig_w_i = makeSig([kWasmI32], [kWasmStringRef]);
let kSig_v_w = makeSig([kWasmStringRef], []);

(function () {
  let huge_builder = new WasmModuleBuilder();
  huge_builder.addMemory(65001, undefined, false, false);

  huge_builder.addFunction("huge", kSig_w_v).exportFunc().addBody([
    kExprI32Const, 0,                  // address
    ...wasmI32Const(65000 * 65536),    // bytes
    ...GCInstr(kExprStringNewUtf8), 0  // memory index
  ]);

  let callee = huge_builder.addFunction("callee", kSig_w_i).addBody([
    kExprI32Const, 0,                  // address
    kExprLocalGet, 0,                  // bytes
    ...GCInstr(kExprStringNewUtf8), 0  // memory index
  ]);

  let caller = huge_builder.addFunction("caller", kSig_i_i).exportFunc()
    .addBody([
      kExprTry, kWasmI32,
      kExprLocalGet, 0,
      kExprCallFunction, callee.index,
      kExprDrop,
      kExprI32Const, 1,
      kExprCatchAll,
      kExprI32Const, 0,
      kExprEnd
    ]);

  let instance;
  try {
    instance = huge_builder.instantiate();
    // On 64-bit platforms, expect to see this message.
    console.log("Instantiation successful, proceeding.");
  } catch (e) {
    // 32-bit builds don't have enough virtual memory, that's OK.
    assertInstanceof(e, RangeError);
    assertMatches(/Cannot allocate Wasm memory for new instance/, e.message,
      'Error message');
    return;
  }

  // Bug 1: The Utf8Decoder can't handle more than kMaxInt bytes as input.
  assertThrows(() => instance.exports.huge(), RangeError);

  // Bug 2: Exceptions created by the JS-focused strings infrastructure must
  // be marked as uncatchable by Wasm.
  let f1 = instance.exports.caller;
  assertThrows(() => f1(2147483647), RangeError);

  // Bug 3: Builtin calls that have neither a kNoThrow annotation nor exception-
  // handling support make the Wasm inliner sad.
  for (let i = 0; i < 20; i++) f1(10);
  %WasmTierUpFunction(instance, caller.index);
  f1(10);
})();

let builder = new WasmModuleBuilder();

let concat_body = [];
// This doubles the string 26 times, i.e. multiplies its length with a factor
// of ~65 million.
for (let i = 0; i < 26; i++) {
  concat_body.push(...[
    kExprLocalGet, 0, kExprLocalGet, 0,
    ...GCInstr(kExprStringConcat),
    kExprLocalSet, 0
  ]);
}

let concat =
    builder.addFunction('concat', kSig_v_w).exportFunc().addBody(concat_body);

let instance = builder.instantiate();

// Bug 4: Throwing in StringAdd must clear the "thread in wasm" bit.
let f2 = instance.exports.concat;
assertThrows(() => f2("1234567890"));  // 650M characters is too much.

// Bug 5: Operations that can trap must not be marked as kEliminatable,
// otherwise the trap may be eliminated.
for (let i = 0; i < 3; i++) f2("a");   // 65M characters is okay.
%WasmTierUpFunction(instance, concat.index);
assertThrows(() => f2("1234567890"));  // Optimized code still traps.
