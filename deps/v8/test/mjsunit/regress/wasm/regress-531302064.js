// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Save some time:
// Flags: --stack-size=80

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

// The SIMD-enablde scenario is interesting but not essential, so include
// it if possible and quietly skip it otherwise.
// Meaning of these validation bytes: "(module (type (array (field v128))))".
const kSimdEnabled = WebAssembly.validate(
    new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 4, 1, 94, 123, 0]));
console.log(`Including SIMD test scenario: ${kSimdEnabled}`);

let builder = new WasmModuleBuilder();
const kCount = 1000;
// Empirically, the most difficult edge case to get right is when both the
// fixed frame size and the space for stack parameters are just under 4KB,
// so create that situation on 64-bit platforms with i64 values:
const kCount64 = 510;

let $sig_i32 = builder.addType(makeSig(Array(kCount).fill(kWasmI32), []));
let $target_i32 = builder.addFunction("target32", $sig_i32).addBody([]);

let $sig_i64 = builder.addType(makeSig(Array(kCount64).fill(kWasmI64), []));
let $target_i64 = builder.addFunction("target64", $sig_i64).addBody([]);

let body_i32 = [];
let body_i64 = [];
let body_s128 = [];
for (let i = 0; i < kCount; i++) {
  body_i32.push(kExprI32Const, 0);
  body_s128.push(kExprI32Const, 0, kSimdPrefix, kExprI8x16Splat);
}
for (let i = 0; i < kCount64; i++) {
  body_i64.push(kExprI64Const, 0);
}
builder.addFunction("i32", kSig_v_v).exportFunc().addBody(
  body_i32.concat([kExprCallFunction, $target_i32.index]));
builder.addFunction("i64", kSig_v_v).exportFunc().addBody(
  body_i64.concat([kExprCallFunction, $target_i64.index]));

if (kSimdEnabled) {
  let $sig_s128 = builder.addType(makeSig(Array(kCount).fill(kWasmS128), []));
  let $target_s128 = builder.addFunction("target128", $sig_s128).addBody([]);
  builder.addFunction("s128", kSig_v_v).exportFunc().addBody(
    body_s128.concat([kExprCallFunction, $target_s128.index]));
}

let instance = builder.instantiate();
// For easier debugging: trigger lazy compilation while we have enough stack
// space for --print-wasm-code to work.
instance.exports.i32();
instance.exports.i64();

if (kSimdEnabled) {
  instance.exports.s128();
  // Passing many s128 values maximizes stack space consumption when pushing
  // parameters before the call.
  function recurse_s128(depth) {
    try {
      recurse_s128(depth + 1);
    } catch (e) {
      if (e instanceof RangeError) {
        instance.exports.s128();
      }
    }
  }
  recurse_s128(0);
}

// Passing many i32 values doesn't need quite as much stack space for the call
// parameters, but the callee decrements the stack pointer before checking
// for stack space because its frame size is just under 4 KB.
function recurse_i32(depth) {
  try {
    recurse_i32(depth + 1);
  } catch (e) {
    if (e instanceof RangeError) {
      instance.exports.i32();
    }
  }
}
recurse_i32(0);

function recurse_i64(depth) {
  try {
    recurse_i64(depth + 1);
  } catch (e) {
    if (e instanceof RangeError) {
      instance.exports.i64();
    }
  }
}
recurse_i64(0);
