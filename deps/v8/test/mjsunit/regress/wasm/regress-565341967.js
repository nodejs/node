// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-stringref --turboshaft-verify-load-elimination

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let builder = new WasmModuleBuilder();
let internalize = builder.addImport("env", "internalize", kSig_v_r);
builder.addFunction("test", makeSig([kWasmExternRef, kWasmI32], [kWasmI32]))
  .addBody([
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCastNull, kStringRefCode,
    ...GCInstr(kExprStringAsWtf16),
    kExprDrop,
    kExprLocalGet, 0,
    kExprCallFunction, internalize,
    kExprLocalGet, 0,
    kGCPrefix, kExprAnyConvertExtern,
    kGCPrefix, kExprRefCastNull, kStringRefCode,
    ...GCInstr(kExprStringAsWtf16),
    kExprLocalGet, 1,
    ...GCInstr(kExprStringViewWtf16GetCodeunit),
  ]).exportFunc();

let map = new Map();
function do_internalize(s) {
  map.set(s); // internalizes s in-place -> ThinString!
}
let instance = builder.instantiate({ env: { internalize: do_internalize } });
let arr = [,  ,  ];
let s1 = String.fromCharCode(...arr); // SeqOneByteString
let r = instance.exports.test(s1);
console.log(r);
