// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that return_call to a well-known import produces the same result as
// a regular call.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function TestReturnCallStringLength() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let kStringLength =
      builder.addImport('wasm:js-string', 'length', kSig_i_r);

  builder.addFunction("call_length", kSig_i_r)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, kStringLength,
    ]);

  builder.addFunction("return_call_length", kSig_i_r)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprReturnCall, kStringLength,
    ]);

  let instance = builder.instantiate({}, { builtins: ["js-string"] });
  let {call_length, return_call_length} = instance.exports;

  for (let s of ['', 'a', 'hello', '\u{1F600}']) {
    assertEquals(call_length(s), return_call_length(s));
  }
  assertEquals(call_length('hello'), 5);
  assertEquals(return_call_length('hello'), 5);
})();

(function TestReturnCallStringCharCodeAt() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let kSig_i_ri = makeSig([kWasmExternRef, kWasmI32], [kWasmI32]);
  let kStringCharCodeAt =
      builder.addImport('wasm:js-string', 'charCodeAt', kSig_i_ri);

  builder.addFunction("call_charCodeAt", kSig_i_ri)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, kStringCharCodeAt,
    ]);

  builder.addFunction("return_call_charCodeAt", kSig_i_ri)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprReturnCall, kStringCharCodeAt,
    ]);

  let instance = builder.instantiate({}, { builtins: ["js-string"] });
  let {call_charCodeAt, return_call_charCodeAt} = instance.exports;

  let str = 'hello';
  for (let i = 0; i < str.length; i++) {
    assertEquals(call_charCodeAt(str, i), return_call_charCodeAt(str, i));
  }
  assertEquals(return_call_charCodeAt('A', 0), 65);
})();

(function TestReturnCallStringEquals() {
  print(arguments.callee.name);
  let builder = new WasmModuleBuilder();
  let kSig_i_rr = makeSig([kWasmExternRef, kWasmExternRef], [kWasmI32]);
  let kStringEquals =
      builder.addImport('wasm:js-string', 'equals', kSig_i_rr);

  builder.addFunction("call_equals", kSig_i_rr)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, kStringEquals,
    ]);

  builder.addFunction("return_call_equals", kSig_i_rr)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprReturnCall, kStringEquals,
    ]);

  let instance = builder.instantiate({}, { builtins: ["js-string"] });
  let {call_equals, return_call_equals} = instance.exports;

  assertEquals(call_equals('abc', 'abc'), return_call_equals('abc', 'abc'));
  assertEquals(call_equals('abc', 'xyz'), return_call_equals('abc', 'xyz'));
  assertEquals(return_call_equals('a', 'a'), 1);
  assertEquals(return_call_equals('a', 'b'), 0);
})();
