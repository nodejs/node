// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

(function testStringToLowerCaseIntl() {
  if (typeof Intl === 'undefined') return;

  const builder = new WasmModuleBuilder();
  const sig_w_w = makeSig([kWasmStringRef], [kWasmStringRef]);
  const toLowerCase = builder.addImport("m", "toLowerCase", sig_w_w);
  builder.addFunction('main', makeSig([kWasmStringRef], []))
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, toLowerCase,
      kExprDrop,
    ])
    .exportFunc();

  const func = Function.prototype.call.bind(String.prototype.toLowerCase);
  const instance = builder.instantiate({ m: { toLowerCase: func } });

  // In lower limits mode, %StringMaxLength() is 1<<20 = 1048576.
  // U+0130 lowercases to two characters ('i\u0307'), doubling the length and
  // exceeding String::kMaxLength, throwing RangeError.
  const len = Math.floor(%StringMaxLength() / 2) + 1;
  const huge_string = "\u0130".repeat(len);

  assertThrows(() => instance.exports.main(huge_string), RangeError,
               /Invalid string length/);
})();

(function testIntToString() {
  const builder = new WasmModuleBuilder();
  const sig_w_ii = makeSig([kWasmI32, kWasmI32], [kWasmStringRef]);
  const intToString = builder.addImport("m", "intToString", sig_w_ii);
  builder.addFunction('main', makeSig([kWasmI32, kWasmI32], []))
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, intToString,
      kExprDrop,
    ])
    .exportFunc();

  const func = Function.prototype.call.bind(Number.prototype.toString);
  const instance = builder.instantiate({ m: { intToString: func } });

  // Radix must be between 2 and 36; 100 throws RangeError.
  assertThrows(() => instance.exports.main(42, 100), RangeError,
               /toString\(\) radix argument must be between 2 and 36/);
})();
