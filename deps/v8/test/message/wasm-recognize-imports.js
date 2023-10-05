// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-stringref --allow-natives-syntax
// Flags: --experimental-wasm-gc --experimental-wasm-imported-strings
// Flags: --trace-wasm-inlining --liftoff --experimental-wasm-typed-funcref
// Also explicitly enable inlining and disable debug code to avoid differences
// between --future and --no-future or debug and release builds.
// Flags: --experimental-wasm-inlining --no-debug-code

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

// Tests the basic mechanism: imports can be recognized, optimized code will
// be discarded when the same module is instantiated with other imports.
(function TestBasics() {
  let builder = new WasmModuleBuilder();
  let sig_w_w = makeSig([kWasmStringRef], [kWasmStringRef]);
  let toLowerCase = builder.addImport("m", "toLowerCase", sig_w_w);

  builder.addFunction('call_tolower', sig_w_w).exportFunc().addBody([
    kExprLocalGet, 0,
    kExprCallFunction, toLowerCase,
  ]);

  let module = builder.toModule();

  let recognizable = Function.prototype.call.bind(String.prototype.toLowerCase);
  let recognizable_imports = { m: { toLowerCase: recognizable } };

  let instance1 = new WebAssembly.Instance(module, recognizable_imports);
  let call_tolower = instance1.exports.call_tolower;
  call_tolower("ABC");
  %WasmTierUpFunction(call_tolower);
  call_tolower("ABC");

  // Creating a second instance with identical imports should not cause
  // recompilation.
  console.log("Second instance.");
  let instance2 = new WebAssembly.Instance(module, recognizable_imports);
  let call_tolower2 = instance2.exports.call_tolower;
  call_tolower2("DEF");
  console.log("Still optimized: " + %IsTurboFanFunction(call_tolower2));

  // Creating a third instance with different imports must not reuse the
  // existing optimized code.
  console.log("Third instance.");
  let other_imports = { m: { toLowerCase: () => "foo" } };
  let instance3 = new WebAssembly.Instance(module, other_imports);
  let call_tolower3 = instance3.exports.call_tolower;
  call_tolower3("GHI");
  console.log("Still optimized: " + %IsTurboFanFunction(call_tolower3));
})();

// Tests that the builtins we aim to recognize actually are recognized.
(function TestRecognizedBuiltins() {
  let builder = new WasmModuleBuilder();
  let sig_d_w = makeSig([kWasmStringRef], [kWasmF64]);
  let sig_i_wwi =
      makeSig([kWasmStringRef, kWasmStringRef, kWasmI32], [kWasmI32]);
  let sig_w_d = makeSig([kWasmF64], [kWasmStringRef]);
  let sig_w_ii = makeSig([kWasmI32, kWasmI32], [kWasmStringRef]);
  let sig_w_ww = makeSig([kWasmStringRef, kWasmStringRef], [kWasmStringRef]);
  let nn_string = wasmRefType(kWasmStringRef);
  let sig_w_ww_nonnullable = makeSig([nn_string, nn_string], [nn_string]);
  builder.addImport("m", "intToString", sig_w_ii);
  builder.addImport("m", "doubleToString", sig_w_d);
  builder.addImport("m", "parseFloat", sig_d_w);
  builder.addImport("m", "indexOf", sig_i_wwi);
  builder.addImport("m", "toLocaleLowerCase", sig_w_ww);
  builder.addImport("m", "toLocaleLowerCase_nn", sig_w_ww_nonnullable);
  let indexOf = Function.prototype.call.bind(String.prototype.indexOf);
  let intToString = Function.prototype.call.bind(Number.prototype.toString);
  let doubleToString = intToString;
  let toLocaleLowerCase =
      Function.prototype.call.bind(String.prototype.toLocaleLowerCase);
  builder.instantiate({
    m: {
      intToString,
      doubleToString,
      parseFloat,
      indexOf,
      toLocaleLowerCase,
      toLocaleLowerCase_nn: toLocaleLowerCase,
    }
  });
})();

// Tests imported strings.
(function TestImportedStrings() {
  console.log("Testing imported strings");
  // We use "r" for nullable "externref", and "e" for non-nullable "ref extern".
  let kRefExtern = wasmRefType(kWasmExternRef);
  let kSig_e_ii = makeSig([kWasmI32, kWasmI32], [kRefExtern]);
  let kSig_e_d = makeSig([kWasmF64], [kRefExtern]);
  let kSig_i_ri = makeSig([kWasmExternRef, kWasmI32], [kWasmI32]);
  let kSig_i_rr = makeSig([kWasmExternRef, kWasmExternRef], [kWasmI32]);
  let kSig_e_i = makeSig([kWasmI32], [kRefExtern]);
  let kSig_e_rii = makeSig([kWasmExternRef, kWasmI32, kWasmI32],
                          [kRefExtern]);
  let kSig_e_rr = makeSig([kWasmExternRef, kWasmExternRef], [kRefExtern]);

  let builder = new WasmModuleBuilder();
  let kArrayI16 = builder.addArray(kWasmI16, true);
  let kArrayI8 = builder.addArray(kWasmI8, true);
  let a16ref = wasmRefNullType(kArrayI16);
  let a8ref = wasmRefNullType(kArrayI8);

  builder.addImport('String', 'fromWtf16Array',
                    makeSig([a16ref, kWasmI32, kWasmI32], [kRefExtern]));
  builder.addImport('String', 'fromWtf8Array',
                    makeSig([a8ref, kWasmI32, kWasmI32], [kRefExtern]));
  builder.addImport('String', 'toWtf16Array',
                    makeSig([kWasmExternRef, a16ref, kWasmI32], [kWasmI32]));
  builder.addImport('String', 'fromCharCode', kSig_e_i);
  builder.addImport('String', 'fromCodePoint', kSig_e_i);
  builder.addImport('String', 'charCodeAt', kSig_i_ri);
  builder.addImport('String', 'codePointAt', kSig_i_ri);
  builder.addImport('String', 'length', kSig_i_r);
  builder.addImport('String', 'concat', kSig_e_rr);
  builder.addImport('String', 'substring', kSig_e_rii);
  builder.addImport('String', 'equals', kSig_i_rr);
  builder.addImport('String', 'compare', kSig_i_rr);

  builder.addImport('related', 'intToString', kSig_e_ii);
  builder.addImport('related', 'doubleToString', kSig_e_d);
  // String-consuming imports like "toLowerCase" are not (yet?) supported for
  // special-casing with imported strings due to lack of static typing.
  let intToString = Function.prototype.call.bind(Number.prototype.toString);
  let doubleToString = intToString;

  builder.instantiate({
    String: WebAssembly.String,
    related: { intToString, doubleToString }
  });
})();
