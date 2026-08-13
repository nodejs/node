// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-externalize-string

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

const builder = new WasmModuleBuilder();

const sig_i_ri = makeSig([kWasmExternRef, kWasmI32], [kWasmI32]);
const $sig_v_r = builder.addType(kSig_v_r);

const $ext = builder.addImport("m", "ext", $sig_v_r);
const $thin = builder.addImport("m", "thin", $sig_v_r);
assertEquals(1, $thin);  // Reused as index in table below.

const $charCodeAt = builder.addImport("wasm:js-string", "charCodeAt", sig_i_ri);
const $table0 = builder.addTable(wasmRefNullType($sig_v_r), 2, 2);
builder.addActiveElementSegment(
    $table0.index, wasmI32Const(0),
    [[kExprRefFunc, $ext], [kExprRefFunc, $thin]],
    wasmRefType($sig_v_r));

builder.addFunction("test", sig_i_ri).exportFunc().addBody([
    kExprLocalGet, 0,
    kExprI32Const, 0,
    kExprCallFunction, $charCodeAt,
    kExprDrop,

    kExprLocalGet, 0,  // string
    kExprLocalGet, 1,  // function index
    kExprCallIndirect, $sig_v_r, $table0.index,

    kExprLocalGet, 0,
    kExprI32Const, 0,
    kExprCallFunction, $charCodeAt,

    kExprLocalGet, 0,
    kExprI32Const, 1,
    kExprCallFunction, $charCodeAt,
    kExprI32Const, 8,
    kExprI32Shl,
    kExprI32Ior,

    kExprLocalGet, 0,
    kExprI32Const, 2,
    kExprCallFunction, $charCodeAt,
    kExprI32Const, 16,
    kExprI32Shl,
    kExprI32Ior,

    kExprLocalGet, 0,
    kExprI32Const, 3,
    kExprCallFunction, $charCodeAt,
    kExprI32Const, 24,
    kExprI32Shl,
    kExprI32Ior,
  ]);

function thin(s) { const o = {}; o[s]; }
function ext(s) { externalizeString(s); }

let instance =
    builder.instantiate({m: { ext, thin }}, {builtins: ["js-string"]});

function f(s, func) {
  let clone = String.fromCharCode(...s.split("").map(c => c.charCodeAt(0)));
  clone = createExternalizableString(clone);
  return instance.exports.test(clone, func).toString(16);
}
const string = "abcdefghijklmnopqrstuvwxyz";
const expected = "64636261";  // Hex ASCII for "abcd", little endian order.
assertEquals(expected, f(string, $thin));
assertEquals(expected, f(string, $ext));
%WasmTierUpFunction(instance.exports.test);
assertEquals(expected, f(string, $thin));
assertEquals(expected, f(string, $ext));
