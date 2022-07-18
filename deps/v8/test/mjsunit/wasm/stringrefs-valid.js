// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --experimental-wasm-stringref

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

function assertValid(fn) {
  let builder = new WasmModuleBuilder();
  fn(builder);
  // If an assertValid() ever fails unexpectedly, uncomment this line to
  // get a more precise error:
  // builder.toModule();
  assertTrue(WebAssembly.validate(builder.toBuffer()));
}
function assertInvalid(fn, message) {
  let builder = new WasmModuleBuilder();
  fn(builder);
  assertThrows(() => builder.toModule(), WebAssembly.CompileError,
               `WebAssembly.Module(): ${message}`);
}

assertValid(builder => builder.addLiteralStringRef("foo"));

for (let [name, code] of [['string', kStringRefCode],
                          ['stringview_wtf8', kStringViewWtf8Code],
                          ['stringview_wtf16', kStringViewWtf16Code],
                          ['stringview_iter', kStringViewIterCode]]) {
  let default_init = [kExprRefNull, code];

  assertValid(b => b.addType(makeSig([code], [])));
  assertValid(b => b.addStruct([makeField(code, true)]));
  assertValid(b => b.addArray(code, true));
  assertValid(b => b.addType(makeSig([], [code])));
  assertValid(b => b.addGlobal(code, true, default_init));
  assertValid(b => b.addTable(code, 0));
  assertValid(b => b.addPassiveElementSegment([default_init], code));
  assertValid(b => b.addTag(makeSig([code], [])));
  assertValid(
    b => b.addFunction(undefined, kSig_v_v).addLocals(code, 1).addBody([]));
}

let kSig_w_ii = makeSig([kWasmI32, kWasmI32], [kWasmStringRef]);
let kSig_w_v = makeSig([], [kWasmStringRef]);
let kSig_i_w = makeSig([kWasmStringRef], [kWasmI32]);
let kSig_i_wi = makeSig([kWasmStringRef, kWasmI32], [kWasmI32]);
let kSig_w_ww = makeSig([kWasmStringRef, kWasmStringRef], [kWasmStringRef]);
let kSig_i_ww = makeSig([kWasmStringRef, kWasmStringRef], [kWasmI32]);
let kSig_x_w = makeSig([kWasmStringRef], [kWasmStringViewWtf8]);
let kSig_i_xii = makeSig([kWasmStringViewWtf8, kWasmI32, kWasmI32],
                         [kWasmI32]);
let kSig_ii_xiii = makeSig([kWasmStringViewWtf8, kWasmI32, kWasmI32,
                            kWasmI32],
                           [kWasmI32, kWasmI32]);
let kSig_w_xii = makeSig([kWasmStringViewWtf8, kWasmI32, kWasmI32],
                         [kWasmStringRef]);
let kSig_y_w = makeSig([kWasmStringRef], [kWasmStringViewWtf16]);
let kSig_i_y = makeSig([kWasmStringViewWtf16], [kWasmI32]);
let kSig_i_yi = makeSig([kWasmStringViewWtf16, kWasmI32], [kWasmI32]);
let kSig_i_yiii = makeSig([kWasmStringViewWtf16, kWasmI32, kWasmI32,
                           kWasmI32], [kWasmI32]);
let kSig_w_yii = makeSig([kWasmStringViewWtf16, kWasmI32, kWasmI32],
                         [kWasmStringRef]);
let kSig_z_w = makeSig([kWasmStringRef], [kWasmStringViewIter]);
let kSig_i_z = makeSig([kWasmStringViewIter], [kWasmI32]);
let kSig_i_zi = makeSig([kWasmStringViewIter, kWasmI32], [kWasmI32]);
let kSig_w_zi = makeSig([kWasmStringViewIter, kWasmI32],
                        [kWasmStringRef]);

(function TestInstructions() {
  let builder = new WasmModuleBuilder();

  builder.addMemory(0, undefined, false, false);

  builder.addFunction("string.new_wtf8/reject", kSig_w_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringNewWtf8, 0, kWtf8PolicyReject
    ]);

  builder.addFunction("string.new_wtf8/accept", kSig_w_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringNewWtf8, 0, kWtf8PolicyAccept
    ]);

  builder.addFunction("string.new_wtf8/replace", kSig_w_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringNewWtf8, 0, kWtf8PolicyReplace
    ]);

  builder.addFunction("string.new_wtf16", kSig_w_ii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringNewWtf16, 0
    ]);

  builder.addLiteralStringRef("foo");
  builder.addFunction("string.const", kSig_w_v)
    .addBody([
      kGCPrefix, kExprStringConst, 0
    ]);

  builder.addFunction("string.measure_wtf8/utf-8", kSig_i_w)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStringMeasureWtf8, kWtf8PolicyReject
    ]);

  builder.addFunction("string.measure_wtf8/wtf-8", kSig_i_w)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStringMeasureWtf8, kWtf8PolicyAccept
    ]);

  builder.addFunction("string.measure_wtf8/replace", kSig_i_w)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStringMeasureWtf8, kWtf8PolicyReplace
    ]);

  builder.addFunction("string.measure_wtf16", kSig_i_w)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStringMeasureWtf16
    ]);

  builder.addFunction("string.encode_wtf8/utf-8", kSig_i_wi)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringEncodeWtf8, 0, kWtf8PolicyAccept
    ]);
  builder.addFunction("string.encode_wtf8/wtf-8", kSig_i_wi)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringEncodeWtf8, 0, kWtf8PolicyReject
    ]);
  builder.addFunction("string.encode_wtf8/replace", kSig_i_wi)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringEncodeWtf8, 0, kWtf8PolicyReplace
    ]);

  builder.addFunction("string.encode_wtf16", kSig_i_wi)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringEncodeWtf16, 0
    ]);

  builder.addFunction("string.concat", kSig_w_ww)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringConcat
    ]);

  builder.addFunction("string.eq", kSig_i_ww)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringEq
    ]);

  builder.addFunction("string.as_wtf8", kSig_x_w)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStringAsWtf8
    ]);

  builder.addFunction("stringview_wtf8.advance", kSig_i_xii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
      kGCPrefix, kExprStringViewWtf8Advance
    ]);

  builder.addFunction("stringview_wtf8.encode/utf-8", kSig_ii_xiii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2, kExprLocalGet, 3,
      kGCPrefix, kExprStringViewWtf8Encode, 0, 0
    ]);

  builder.addFunction("stringview_wtf8.encode/wtf-8", kSig_ii_xiii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2, kExprLocalGet, 3,
      kGCPrefix, kExprStringViewWtf8Encode, 0, 1
    ]);

  builder.addFunction("stringview_wtf8.encode/replace", kSig_ii_xiii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2, kExprLocalGet, 3,
      kGCPrefix, kExprStringViewWtf8Encode, 0, 2
    ]);

  builder.addFunction("stringview_wtf8.slice", kSig_w_xii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
      kGCPrefix, kExprStringViewWtf8Slice
    ]);

  builder.addFunction("string.as_wtf16", kSig_y_w)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStringAsWtf16
    ]);

  builder.addFunction("stringview_wtf16.length", kSig_i_y)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStringViewWtf16Length
    ]);

  builder.addFunction("stringview_wtf16.get_codeunit", kSig_i_yi)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringViewWtf16GetCodeunit
    ]);

  builder.addFunction("stringview_wtf16.encode", kSig_i_yiii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2, kExprLocalGet, 3,
      kGCPrefix, kExprStringViewWtf16Encode, 0
    ]);

  builder.addFunction("stringview_wtf16.slice", kSig_w_yii)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1, kExprLocalGet, 2,
      kGCPrefix, kExprStringViewWtf16Slice
    ]);

  builder.addFunction("string.as_iter", kSig_z_w)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStringAsIter
    ]);

  builder.addFunction("stringview_iter.next", kSig_i_z)
    .addBody([
      kExprLocalGet, 0,
      kGCPrefix, kExprStringViewIterNext
    ]);

  builder.addFunction("stringview_iter.advance", kSig_i_zi)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringViewIterAdvance
    ]);

  builder.addFunction("stringview_iter.rewind", kSig_i_zi)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringViewIterRewind
    ]);

  builder.addFunction("stringview_iter.slice", kSig_w_zi)
    .addBody([
      kExprLocalGet, 0, kExprLocalGet, 1,
      kGCPrefix, kExprStringViewIterSlice
    ]);

  let i8_array = builder.addArray(kWasmI8, true);
  let i16_array = builder.addArray(kWasmI16, true);

  builder.addFunction("string.new_wtf8_array/accept", kSig_w_v)
    .addBody([
      kExprRefNull, i8_array,
      kExprI32Const, 0,
      kExprI32Const, 0,
      kGCPrefix, kExprStringNewWtf8Array, kWtf8PolicyAccept
    ]);

  builder.addFunction("string.new_wtf8_array/reject", kSig_w_v)
    .addBody([
      kExprRefNull, i8_array,
      kExprI32Const, 0,
      kExprI32Const, 0,
      kGCPrefix, kExprStringNewWtf8Array, kWtf8PolicyReject
    ]);

  builder.addFunction("string.new_wtf8_array/replace", kSig_w_v)
    .addBody([
      kExprRefNull, i8_array,
      kExprI32Const, 0,
      kExprI32Const, 0,
      kGCPrefix, kExprStringNewWtf8Array, kWtf8PolicyReplace
    ]);

  builder.addFunction("string.new_wtf16_array", kSig_w_v)
    .addBody([
      kExprRefNull, i16_array,
      kExprI32Const, 0,
      kExprI32Const, 0,
      kGCPrefix, kExprStringNewWtf16Array
    ]);

  builder.addFunction("string.encode_wtf8_array/accept", kSig_i_v)
    .addBody([
      kExprRefNull, kStringRefCode,
      kExprRefNull, i8_array,
      kExprI32Const, 0,
      kGCPrefix, kExprStringEncodeWtf8Array, kWtf8PolicyAccept
    ]);

  builder.addFunction("string.encode_wtf8_array/reject", kSig_i_v)
    .addBody([
      kExprRefNull, kStringRefCode,
      kExprRefNull, i8_array,
      kExprI32Const, 0,
      kGCPrefix, kExprStringEncodeWtf8Array, kWtf8PolicyReject
    ]);

  builder.addFunction("string.encode_wtf8_array/replace", kSig_i_v)
    .addBody([
      kExprRefNull, kStringRefCode,
      kExprRefNull, i8_array,
      kExprI32Const, 0,
      kGCPrefix, kExprStringEncodeWtf8Array, kWtf8PolicyReplace
    ]);

  builder.addFunction("string.encode_wtf16_array", kSig_i_v)
    .addBody([
      kExprRefNull, kStringRefCode,
      kExprRefNull, i16_array,
      kExprI32Const, 0,
      kGCPrefix, kExprStringEncodeWtf16Array
    ]);

  assertTrue(WebAssembly.validate(builder.toBuffer()));
})();

assertInvalid(
  builder => {
    builder.addFunction("string.const/bad-index", kSig_w_v)
      .addBody([
        kGCPrefix, kExprStringConst, 0
      ]);
  },
  "Compiling function #0:\"string.const/bad-index\" failed: " +
    "Invalid string literal index: 0 @+26");

assertInvalid(
  builder => {
    builder.addFunction("string.new_wtf8/no-mem", kSig_w_ii)
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1,
        kGCPrefix, kExprStringNewWtf8, 0, kWtf8PolicyAccept
      ]);
  },
  "Compiling function #0:\"string.new_wtf8/no-mem\" failed: " +
    "memory instruction with no memory @+32");

assertInvalid(
  builder => {
    builder.addMemory(0, undefined, false, false);
    builder.addFunction("string.new_wtf8/bad-mem", kSig_w_ii)
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1,
        kGCPrefix, kExprStringNewWtf8, 1, kWtf8PolicyAccept
      ]);
  },
  "Compiling function #0:\"string.new_wtf8/bad-mem\" failed: " +
    "expected memory index 0, found 1 @+37");

assertInvalid(
  builder => {
    builder.addFunction("string.encode_wtf8/no-mem", kSig_i_wi)
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1,
        kGCPrefix, kExprStringEncodeWtf8, 0, kWtf8PolicyAccept
      ]);
  },
  "Compiling function #0:\"string.encode_wtf8/no-mem\" failed: " +
    "memory instruction with no memory @+32");

assertInvalid(
  builder => {
    builder.addMemory(0, undefined, false, false);
    builder.addFunction("string.encode_wtf8/bad-mem", kSig_i_wi)
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1,
        kGCPrefix, kExprStringEncodeWtf8, 1, kWtf8PolicyAccept
      ]);
  },
  "Compiling function #0:\"string.encode_wtf8/bad-mem\" failed: " +
    "expected memory index 0, found 1 @+37");

assertInvalid(
  builder => {
    builder.addMemory(0, undefined, false, false);
    builder.addFunction("string.encode_wtf8/bad-policy", kSig_i_wi)
      .addBody([
        kExprLocalGet, 0, kExprLocalGet, 1,
        kGCPrefix, kExprStringEncodeWtf8, 0, 3
      ]);
  },
  "Compiling function #0:\"string.encode_wtf8/bad-policy\" failed: " +
    "expected wtf8 policy 0, 1, or 2, but found 3 @+38");

assertInvalid(
  builder => {
    builder.addFunction("string.measure_wtf8/bad-policy", kSig_i_w)
      .addBody([
        kExprLocalGet, 0,
        kGCPrefix, kExprStringMeasureWtf8, 3
      ]);
  },
  "Compiling function #0:\"string.measure_wtf8/bad-policy\" failed: " +
    "expected wtf8 policy 0, 1, or 2, but found 3 @+29");

assertInvalid(
  builder => {
    let i8_array = builder.addArray(kWasmI8, true);
    builder.addFunction("string.new_wtf8_array/bad-policy", kSig_w_v)
      .addBody([
        kExprRefNull, i8_array,
        kExprI32Const, 0,
        kExprI32Const, 0,
        kGCPrefix, kExprStringNewWtf8Array, 3
      ]);
  },
  "Compiling function #0:\"string.new_wtf8_array/bad-policy\" failed: " +
    "expected wtf8 policy 0, 1, or 2, but found 3 @+35");

assertInvalid(
  builder => {
    let i16_array = builder.addArray(kWasmI16, true);
    builder.addFunction("string.new_wtf8_array/bad-type", kSig_w_v)
      .addBody([
        kExprRefNull, i16_array,
        kExprI32Const, 0,
        kExprI32Const, 0,
        kGCPrefix, kExprStringNewWtf8Array, kWtf8PolicyAccept
      ]);
  },
  "Compiling function #0:\"string.new_wtf8_array/bad-type\" failed: " +
    "string.new_wtf8_array[0] expected array of i8, " +
    "found ref.null of type (ref null 0) @+27");

assertInvalid(
  builder => {
    let i8_array = builder.addArray(kWasmI8, true);
    builder.addFunction("string.new_wtf16_array/bad-type", kSig_w_v)
      .addBody([
        kExprRefNull, i8_array,
        kExprI32Const, 0,
        kExprI32Const, 0,
        kGCPrefix, kExprStringNewWtf16Array
      ]);
  },
  "Compiling function #0:\"string.new_wtf16_array/bad-type\" failed: " +
    "string.new_wtf16_array[0] expected array of i16, " +
    "found ref.null of type (ref null 0) @+27");

assertInvalid(
  builder => {
    let immutable_i8_array = builder.addArray(kWasmI8, false);
    let sig = makeSig([kWasmStringRef,
                       wasmRefType(immutable_i8_array),
                       kWasmI32],
                      [kWasmI32]);
    builder.addFunction("string.encode_wtf8_array/bad-type", sig)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kGCPrefix, kExprStringEncodeWtf8Array, kWtf8PolicyAccept,
      ]);
  },
  "Compiling function #0:\"string.encode_wtf8_array/bad-type\" failed: " +
    "string.encode_wtf8_array[1] expected array of mutable i8, " +
    "found local.get of type (ref 0) @+33");

assertInvalid(
  builder => {
    let immutable_i16_array = builder.addArray(kWasmI16, false);
    let sig = makeSig([kWasmStringRef,
                       wasmRefType(immutable_i16_array),
                       kWasmI32],
                      [kWasmI32]);
    builder.addFunction("string.encode_wtf16_array/bad-type", sig)
      .addBody([
        kExprLocalGet, 0,
        kExprLocalGet, 1,
        kExprLocalGet, 2,
        kGCPrefix, kExprStringEncodeWtf16Array,
      ]);
  },
  "Compiling function #0:\"string.encode_wtf16_array/bad-type\" failed: " +
    "string.encode_wtf16_array[1] expected array of mutable i16, " +
    "found local.get of type (ref 0) @+33");
