// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --experimental-wasm-imported-strings-utf8

// Adapted from 'test/mjsunit/wasm/imported-strings-utf8.js'.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let kRefSharedExtern = wasmRefType(kWasmExternRef).shared();
let kRefNullSharedExtern = wasmRefNullType(kWasmExternRef).shared();

// We use "s" for nullable "shared externref", and "t" for non-nullable
// "ref shared extern".

let kSig_i_t = makeSig([kRefSharedExtern], [kWasmI32]);
let kSig_t_ii = makeSig([kWasmI32, kWasmI32], [kRefSharedExtern]);
let kSig_t_v = makeSig([], [kRefSharedExtern]);
let kSig_t_sii = makeSig([kRefNullSharedExtern, kWasmI32, kWasmI32],
                         [kRefSharedExtern]);
let kSig_t_s = makeSig([kRefNullSharedExtern], [kRefSharedExtern]);
// Here we cannot use i_s because it is used for Simd.
let kSig_i_shared = makeSig([kRefNullSharedExtern], [kWasmI32]);
let kSig_s_ii = makeSig([kWasmI32, kWasmI32], [kRefNullSharedExtern]);
let kSig_shared_v = makeSig([], [kRefNullSharedExtern]);

let interestingStrings = [
  '',
  'ascii',
  'latin\xa91',        // Latin-1.
  '2 \ucccc b',        // Two-byte.
  'a \ud800\udc00 b',  // Proper surrogate pair.
  'a \ud800 b',        // Lone lead surrogate.
  'a \udc00 b',        // Lone trail surrogate.
  '\ud800 bc',         // Lone lead surrogate at the start.
  '\udc00 bc',         // Lone trail surrogate at the start.
  'ab \ud800',         // Lone lead surrogate at the end.
  'ab \udc00',         // Lone trail surrogate at the end.
  'a \udc00\ud800 b',  // Swapped surrogate pair.
];

function IsSurrogate(codepoint) {
  return 0xD800 <= codepoint && codepoint <= 0xDFFF
}

function HasIsolatedSurrogate(str) {
  for (let codepoint of str) {
    let value = codepoint.codePointAt(0);
    if (IsSurrogate(value)) return true;
  }
  return false;
}
function ReplaceIsolatedSurrogates(str, replacement='\ufffd') {
  let replaced = '';
  for (let codepoint of str) {
    replaced +=
      IsSurrogate(codepoint.codePointAt(0)) ? replacement : codepoint;
  }
  return replaced;
}

let kArrayI8Shared;
let kStringFromUtf8ArrayShared;
let kStringIntoUtf8ArrayShared;
let kStringToUtf8ArrayShared;
let kStringMeasureUtf8Shared;

function MakeBuilder() {
  let builder = new WasmModuleBuilder();
  builder.startRecGroup();
  kArrayI8Shared = builder.addArray(kWasmI8, {final: true, shared: true});
  builder.endRecGroup();
  let array8ref = wasmRefNullType(kArrayI8Shared);

  kStringFromUtf8ArrayShared = builder.addImport(
      'wasm:text-decoder', 'decodeStringFromUTF8Array',
      makeSig([array8ref, kWasmI32, kWasmI32], [kRefSharedExtern]));
  kStringMeasureUtf8Shared =
      builder.addImport('wasm:text-encoder', 'measureStringAsUTF8',
                        kSig_i_shared);
  kStringIntoUtf8ArrayShared = builder.addImport(
      'wasm:text-encoder', 'encodeStringIntoUTF8Array',
      makeSig([kRefNullSharedExtern, array8ref, kWasmI32], [kWasmI32]));
  kStringToUtf8ArrayShared = builder.addImport(
      'wasm:text-encoder', 'encodeStringToUTF8Array',
      makeSig([kRefNullSharedExtern], [wasmRefType(kArrayI8Shared)]));

  return builder;
}

let kImports = {};
let kBuiltins = { builtins: ["text-decoder", "text-encoder"] };

function encodeWtf8(str) {
  // String iterator coalesces surrogate pairs.
  let out = [];
  for (let codepoint of str) {
    codepoint = codepoint.codePointAt(0);
    if (codepoint <= 0x7f) {
      out.push(codepoint);
    } else if (codepoint <= 0x7ff) {
      out.push(0xc0 | (codepoint >> 6));
      out.push(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0xffff) {
      out.push(0xe0 | (codepoint >> 12));
      out.push(0x80 | ((codepoint >> 6) & 0x3f));
      out.push(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0x10ffff) {
      out.push(0xf0 | (codepoint >> 18));
      out.push(0x80 | ((codepoint >> 12) & 0x3f));
      out.push(0x80 | ((codepoint >> 6) & 0x3f));
      out.push(0x80 | (codepoint & 0x3f));
    } else {
      throw new Error("bad codepoint " + codepoint);
    }
  }
  return out;
}

function makeWtf8TestDataSegment() {
  let data = []
  let valid = {};
  let invalid = {};

  for (let str of interestingStrings) {
    let bytes = encodeWtf8(str);
    valid[str] = { offset: data.length, length: bytes.length };
    for (let byte of bytes) {
      data.push(byte);
    }
  }
  let invalid_inputs = [
    'trailing high byte \xa9',
    'interstitial high \xa9 byte',
    'invalid \xc0 byte',
    'invalid three-byte \xed\xd0\x80',
    'surrogate \xed\xa0\x80\xed\xb0\x80 pair'
  ];
  let invalid_replaced = [
    'trailing high byte \uFFFD',
    'interstitial high \uFFFD byte',
    'invalid \uFFFD byte',
    'invalid three-byte \uFFFD\u0400',
    'surrogate \uFFFD\uFFFD\uFFFD\uFFFD\uFFFD\uFFFD pair'
  ];
  for (let i = 0; i < invalid_inputs.length; i++) {
    let bytes = invalid_inputs[i];
    invalid[bytes] = {
      offset: data.length,
      length: bytes.length,
      replaced: invalid_replaced[i]
    };
    for (let i = 0; i < bytes.length; i++) {
      data.push(bytes.charCodeAt(i));
    }
  }
  return { valid, invalid, data: Uint8Array.from(data) };
};

(function TestStringNewUtf8Array() {
  print(arguments.callee.name);
  let builder = MakeBuilder();
  let data = makeWtf8TestDataSegment();
  let data_index = builder.addPassiveDataSegment(data.data);

  let ascii_data_index =
      builder.addPassiveDataSegment(Uint8Array.from(encodeWtf8("ascii")));

  let make_i8_array = builder.addFunction(
      "make_i8_array", makeSig([], [wasmRefType(kArrayI8Shared)]))
    .addBody([
      ...wasmI32Const(0),
      ...wasmI32Const(data.data.length),
      kGCPrefix, kExprArrayNewData, kArrayI8Shared, data_index
    ]).index;

  builder.addFunction("new_utf8", kSig_s_ii)
    .exportFunc()
    .addBody([
      kExprCallFunction, make_i8_array,
      kExprLocalGet, 0, kExprLocalGet, 1,
      kExprCallFunction, kStringFromUtf8ArrayShared,
    ]);

  builder.addFunction("bounds_check", kSig_s_ii)
    .exportFunc()
    .addBody([
      ...wasmI32Const(0),
      ...wasmI32Const("ascii".length),
      kGCPrefix, kExprArrayNewData, kArrayI8Shared, ascii_data_index,
      kExprLocalGet, 0, kExprLocalGet, 1,
      kExprCallFunction, kStringFromUtf8ArrayShared,
    ]);

  builder.addFunction("null_array", kSig_shared_v).exportFunc()
    .addBody([
      kExprRefNull, kArrayI8Shared,
      kExprI32Const, 0, kExprI32Const, 0,
      kExprCallFunction, kStringFromUtf8ArrayShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  for (let [str, {offset, length}] of Object.entries(data.valid)) {
    let start = offset;
    let end = offset + length;
    if (HasIsolatedSurrogate(str)) {
      // Isolated surrogates have the three-byte pattern ED [A0,BF] [80,BF].
      // When the sloppy decoder gets to the second byte, it will reject
      // the sequence, and then retry parsing at the second byte.
      // Seeing the second byte can't start a sequence, it replaces the
      // second byte and continues with the next, which also can't start
      // a sequence.  The result is that one isolated surrogate is replaced
      // by three U+FFFD codepoints.
      assertEquals(ReplaceIsolatedSurrogates(str, '\ufffd\ufffd\ufffd'),
                   instance.exports.new_utf8(start, end));
    } else {
      assertEquals(str, instance.exports.new_utf8(start, end));
    }
  }
  for (let [str, {offset, length, replaced}] of Object.entries(data.invalid)) {
    let start = offset;
    let end = offset + length;
    assertEquals(replaced, instance.exports.new_utf8(start, end));
  }

  assertEquals("ascii", instance.exports.bounds_check(0, "ascii".length));
  assertEquals("", instance.exports.bounds_check("ascii".length,
                                                 "ascii".length));
  assertEquals("i", instance.exports.bounds_check("ascii".length - 1,
                                                  "ascii".length));
  assertThrows(() => instance.exports.bounds_check(0, 100),
               WebAssembly.RuntimeError, "array element access out of bounds");
  assertThrows(() => instance.exports.bounds_check(0, -1),
               WebAssembly.RuntimeError, "array element access out of bounds");
  assertThrows(() => instance.exports.bounds_check(-1, 0),
               WebAssembly.RuntimeError, "array element access out of bounds");
  assertThrows(() => instance.exports.bounds_check("ascii".length,
                                                   "ascii".length + 1),
               WebAssembly.RuntimeError, "array element access out of bounds");
  assertThrows(() => instance.exports.null_array(),
               WebAssembly.RuntimeError, "dereferencing a null pointer");
})();

(function TestStringMeasureUtf8() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  builder.addFunction("string_measure_utf8", kSig_i_t)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, kStringMeasureUtf8Shared,
    ]);

  builder.addFunction("string_measure_utf8_null", kSig_i_v)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprCallFunction, kStringMeasureUtf8Shared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  for (let str of interestingStrings) {
    let wtf8 = encodeWtf8(str);
    assertEquals(wtf8.length, instance.exports.string_measure_utf8(str));
  }

  assertThrows(() => instance.exports.string_measure_utf8_null(),
               WebAssembly.RuntimeError, "illegal cast");
})();

(function TestStringEncodeUtf8Array() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  // Allocate an array that's exactly the expected size, and encode
  // into it.  Then decode it.
  // (str, length, offset=0) -> str
  builder.addFunction("encode_utf8", kSig_t_sii)
    .exportFunc()
    .addLocals(wasmRefNullType(kArrayI8Shared), 1)
    .addLocals(kWasmI32, 1)
    .addBody([
      // Allocate buffer.
      kExprLocalGet, 1,
      kGCPrefix, kExprArrayNewDefault, kArrayI8Shared,
      kExprLocalSet, 3,

      // Write buffer, store number of bytes written.
      kExprLocalGet, 0,
      kExprLocalGet, 3,
      kExprLocalGet, 2,
      kExprCallFunction, kStringIntoUtf8ArrayShared,
      kExprLocalSet, 4,

      // Read buffer.
      kExprLocalGet, 3,
      kExprLocalGet, 2,
      kExprLocalGet, 2, kExprLocalGet, 4, kExprI32Add,
      kExprCallFunction, kStringFromUtf8ArrayShared,
    ]);


  builder.addFunction("encode_null_string", kSig_i_v)
    .exportFunc()
    .addBody([
        kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
        kExprI32Const, 0, kGCPrefix, kExprArrayNewDefault, kArrayI8Shared,
        kExprI32Const, 0,
        kExprCallFunction, kStringIntoUtf8ArrayShared,
      ]);
  builder.addFunction("encode_null_array", kSig_i_v)
    .exportFunc()
    .addBody([
        kExprI32Const, 0, kGCPrefix, kExprArrayNewDefault, kArrayI8Shared,
        kExprI32Const, 0, kExprI32Const, 0,
        kExprCallFunction, kStringFromUtf8ArrayShared,
        kExprRefNull, kArrayI8Shared,
        kExprI32Const, 0,
        kExprCallFunction, kStringIntoUtf8ArrayShared,
      ]);

  let instance = builder.instantiate(kImports, kBuiltins);

  for (let str of interestingStrings) {
    let replaced = ReplaceIsolatedSurrogates(str);
    if (!HasIsolatedSurrogate(str)) assertEquals(str, replaced);
    let wtf8 = encodeWtf8(replaced);
    assertEquals(replaced,
                 instance.exports.encode_utf8(str, wtf8.length, 0));
    assertEquals(replaced,
                 instance.exports.encode_utf8(str, wtf8.length + 20, 10));
  }

  assertThrows(() => instance.exports.encode_null_array(),
               WebAssembly.RuntimeError, "dereferencing a null pointer");
  assertThrows(() => instance.exports.encode_null_string(),
               WebAssembly.RuntimeError, "illegal cast");

  for (let str of interestingStrings) {
    let wtf8 = encodeWtf8(str);
    let message = "array element access out of bounds";
    assertThrows(() => instance.exports.encode_utf8(str, wtf8.length, 1),
                 WebAssembly.RuntimeError, message);
  }
})();

(function TestStringToUtf8Array() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  // Convert the string to an array, then decode it back.
  builder.addFunction("encode_utf8", kSig_t_s)
    .exportFunc()
    .addLocals(wasmRefNullType(kArrayI8Shared), 1)
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, kStringToUtf8ArrayShared,
      kExprLocalTee, 1,

      kExprI32Const, 0,  // start
      kExprLocalGet, 1, kGCPrefix, kExprArrayLen,  // end
      kExprCallFunction, kStringFromUtf8ArrayShared,
    ]);

  let sig_as8_v = makeSig([], [wasmRefType(kArrayI8Shared)]);
  builder.addFunction("encode_null_string", sig_as8_v)
    .exportFunc()
    .addBody([
        kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
        kExprCallFunction, kStringToUtf8ArrayShared,
      ]);

  let instance = builder.instantiate(kImports, kBuiltins);

  for (let str of interestingStrings) {
    let replaced = ReplaceIsolatedSurrogates(str);
    if (!HasIsolatedSurrogate(str)) assertEquals(str, replaced);
    let wtf8 = encodeWtf8(replaced);
    assertEquals(replaced,
                 instance.exports.encode_utf8(str, wtf8.length, 0));
    assertEquals(replaced,
                 instance.exports.encode_utf8(str, wtf8.length + 20, 10));
  }

  assertThrows(() => instance.exports.encode_null_string(),
               WebAssembly.RuntimeError, "illegal cast");
})();
