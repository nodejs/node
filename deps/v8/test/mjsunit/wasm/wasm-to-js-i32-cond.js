// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for an invalid conversion from JS to Wasm for I32.
// Torque converted the JS number to 'Unsigned' during the conversion from
// JS to Wasm. This only affected RiscV which does conditions on the
// whole 64 bit value. Furthermore, it only happened if there was no other
// operation on the I32, since those would automatically sign-extend.

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

function GenerateAndRunTest() {
  const builder = new WasmModuleBuilder();
  const impSig = kSig_i_i;
  const impIndex = builder.addImport('m', 'f', impSig);
  let body = [];
  body.push(kExprLocalGet, 0);
  body.push(kExprCallFunction, impIndex);
  body.push(kExprI32Const, 11);
  body.push(kExprI32LtS);
  body.push(kExprIf, kWasmVoid);
  body.push(kExprI32Const, 1);
  body.push(kExprReturn);
  body.push(kExprEnd);
  body.push(kExprI32Const, 0);
  body.push(kExprReturn);

  function impFunction(x) {
    return x;
  };

  builder.addFunction('main', kSig_i_i).addBody(body).exportFunc();
  const instance = builder.instantiate({ m: { f: impFunction } });
  // 8 is less than 11, so the function should return 1.
  let result = instance.exports.main(8);
  assertEquals(1, result);
  // 15 is greater than 11, so the function should return 0.
  result = instance.exports.main(15);
  assertEquals(0, result);
  // -44 is less than 11, so the function should return 1.
  result = instance.exports.main(-44);
  assertEquals(1, result);
}

GenerateAndRunTest();
