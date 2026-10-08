// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-stringref --expose-gc

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
const $js_call1 = builder.addImport("m", "js_call1", kSig_v_v);
const $js_call2 = builder.addImport("m", "js_call2", kSig_v_v);

let kRefExtern = wasmRefType(kWasmExternRef);
let kSig_e_r = makeSig([kWasmExternRef], [kRefExtern]);
let kSig_i_ri = makeSig([kWasmExternRef, kWasmI32], [kWasmI32]);

let kStringCast = builder.addImport('wasm:js-string', 'cast', kSig_e_r);
let kStringCharCodeAt =
    builder.addImport('wasm:js-string', 'charCodeAt', kSig_i_ri);

const kLength = 100;

builder.addFunction("stringref", makeSig([kWasmStringRef], [kWasmI32]))
  .addLocals(kWasmStringViewWtf16, 1)
  .addBody([
    kExprLocalGet, 0,
    ...GCInstr(kExprStringAsWtf16),
    kExprLocalTee, 1,
    kExprI32Const, 0,
    ...GCInstr(kExprStringViewWtf16GetCodeunit),
    kExprDrop,

    kExprCallFunction, $js_call1,

    kExprLocalGet, 1,
    ...wasmI32Const(kLength),
    ...GCInstr(kExprStringViewWtf16GetCodeunit),
  ])
  .exportFunc();

builder.addFunction("importedstrings", makeSig([kWasmExternRef], [kWasmI32]))
  .addLocals(kRefExtern, 1)
  .addBody([
    kExprLocalGet, 0,
    kExprCallFunction, kStringCast,
    kExprLocalTee, 1,
    kExprI32Const, 0,
    kExprCallFunction, kStringCharCodeAt,
    kExprDrop,

    kExprCallFunction, $js_call2,

    kExprLocalGet, 1,
    ...wasmI32Const(kLength),
    kExprCallFunction, kStringCharCodeAt,
  ])
  .exportFunc();

let internalized = (new Array(kLength + 1).fill('A')).join('');
Symbol.for(internalized);

const instance = builder.instantiate({
  m: {
    js_call1: () => {
      // Rewrite the string into a ThinString.
      Symbol.for(string1);
      gc();  // Free/zap its previous characters.
      gc();
    },
    js_call2: () => {
      Symbol.for(string2);
      gc();
      gc();
    },
  }
}, { builtins: ["js-string"] });

// Both of these are identical to {internalized}, so will be rewritten into
// ThinStrings when they're internalized.
let string1 = (new Array(kLength + 1).fill('A')).join('');
let string2 = (new Array(kLength + 1).fill('A')).join('');
// Move {string1} and {string2} to old space, so that after they get
// internalized they remain ThinStrings forever.
gc();

assertEquals(0x41, instance.exports.stringref(string1));
assertEquals(0x41, instance.exports.importedstrings(string2));
