// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-shared --allow-natives-syntax
// Flags: --expose-externalize-string

// Adapted from 'test/mjsunit/wasm/imported-strings.js'.

d8.file.execute("test/mjsunit/wasm/wasm-module-builder.js");

let kRefSharedExtern = wasmRefType(kWasmExternRef).shared();
let kRefNullSharedExtern = wasmRefNullType(kWasmExternRef).shared();

// We use 's' for shared nullable externref, and 't' for shared non-nullable
// externref.
let kSig_t_ii = makeSig([kWasmI32, kWasmI32], [kRefSharedExtern]);
let kSig_t_v = makeSig([], [kRefSharedExtern]);
// Here we cannot use plain 's' because it is used for simd.
let kSig_i_shared = makeSig([kRefNullSharedExtern], [kWasmI32]);
let kSig_i_si = makeSig([kRefNullSharedExtern, kWasmI32], [kWasmI32]);
let kSig_i_sii = makeSig([kRefNullSharedExtern, kWasmI32, kWasmI32],
                          [kWasmI32]);
let kSig_i_ss = makeSig([kRefNullSharedExtern, kRefNullSharedExtern],
                        [kWasmI32]);
let kSig_i_ssi = makeSig(
    [kRefNullSharedExtern, kRefNullSharedExtern, kWasmI32], [kWasmI32]);
let kSig_i_siii = makeSig(
    [kRefNullSharedExtern, kWasmI32, kWasmI32, kWasmI32], [kWasmI32]);
let kSig_ii_siii = makeSig(
    [kRefNullSharedExtern, kWasmI32, kWasmI32, kWasmI32],
    [kWasmI32, kWasmI32]);
let kSig_t_i = makeSig([kWasmI32], [kRefSharedExtern]);
let kSig_t_sii = makeSig([kRefNullSharedExtern, kWasmI32, kWasmI32],
                          [kRefSharedExtern]);
let kSig_t_ss = makeSig([kRefNullSharedExtern, kRefNullSharedExtern],
                        [kRefSharedExtern]);
let kSig_t_s = makeSig([kRefNullSharedExtern], [kRefSharedExtern]);
let kSig_s_s = makeSig([kRefNullSharedExtern], [kRefNullSharedExtern]);

let kArrayI16Shared;
let kStringCastShared;
let kStringTestShared;
let kStringFromWtf16ArrayShared;
let kStringToWtf16ArrayShared;
let kStringFromCharCodeShared;
let kStringFromCodePointShared;
let kStringCharCodeAtShared;
let kStringCodePointAtShared;
let kStringLengthShared;
let kStringConcatShared;
let kStringSubstringShared;
let kStringEqualsShared;
let kStringCompareShared;

let kStringIndexOfImported;
let kStringToLowerCaseImported;

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

  %ConstructConsString('ascii left and', 'ascii right'),
  %ConstructConsString('2 \ucccc b', 'must be long enough \ucccc a'),

  %ConstructSlicedString('abcedhfenvejfwqfqw', 5),
  %ConstructSlicedString('febwvnewkvjabc\uccccdfenvefqw', 8),

  %ConstructThinString('this is thin and must be long enough'),
  %ConstructThinString('this is two-byte \ucccc thin string'),
];

function MakeBuilder() {
  let builder = new WasmModuleBuilder();
  builder.startRecGroup();
  kArrayI16Shared =
      builder.addArray(kWasmI16, {is_final: true, is_shared: true});
  builder.endRecGroup();
  let arrayref = wasmRefNullType(kArrayI16Shared);

  kStringCastShared = builder.addImport('wasm:js-string', 'cast', kSig_t_s);
  kStringTestShared = builder.addImport('wasm:js-string', 'test',
                                        kSig_i_shared);
  kStringFromWtf16ArrayShared = builder.addImport(
      'wasm:js-string', 'fromCharCodeArray',
      makeSig([arrayref, kWasmI32, kWasmI32], [kRefSharedExtern]));
  kStringToWtf16ArrayShared = builder.addImport(
      'wasm:js-string', 'intoCharCodeArray',
      makeSig([kRefNullSharedExtern, arrayref, kWasmI32], [kWasmI32]));
  kStringFromCharCodeShared =
      builder.addImport('wasm:js-string', 'fromCharCode', kSig_t_i);
  kStringFromCodePointShared =
      builder.addImport('wasm:js-string', 'fromCodePoint', kSig_t_i);
  kStringCharCodeAtShared =
      builder.addImport('wasm:js-string', 'charCodeAt', kSig_i_si);
  kStringCodePointAtShared =
      builder.addImport('wasm:js-string', 'codePointAt', kSig_i_si);
  kStringLengthShared =
      builder.addImport('wasm:js-string', 'length', kSig_i_shared);
  kStringConcatShared =
      builder.addImport('wasm:js-string', 'concat', kSig_t_ss);
  kStringSubstringShared =
      builder.addImport('wasm:js-string', 'substring', kSig_t_sii);
  kStringEqualsShared =
      builder.addImport('wasm:js-string', 'equals', kSig_i_ss);
  kStringCompareShared =
      builder.addImport('wasm:js-string', 'compare', kSig_i_ss);
  kStringIndexOfImported = builder.addImport('m', 'indexOf', kSig_i_ssi);
  kStringToLowerCaseImported =
      builder.addImport('m', 'toLowerCase', kSig_s_s);

  return builder;
}

let kImports = {
  strings: interestingStrings,
  m: {
    indexOf: Function.prototype.call.bind(String.prototype.indexOf),
    toLowerCase: Function.prototype.call.bind(String.prototype.toLowerCase),
  },
};
let kBuiltins = { builtins: ["js-string"] };

(function TestStringCast() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  builder.addFunction("cast", kSig_t_s)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, kStringCastShared,
    ]);

  builder.addFunction("cast_null", makeSig([], [kRefNullSharedExtern]))
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprCallFunction, kStringCastShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);

  assertEquals('foo', instance.exports.cast('foo'));
  assertThrows(
      () => instance.exports.cast(123), WebAssembly.RuntimeError,
      'illegal cast');
  assertThrows(
      () => instance.exports.cast(undefined), WebAssembly.RuntimeError,
      'illegal cast');
  assertThrows(
      () => instance.exports.cast(true), WebAssembly.RuntimeError,
      'illegal cast');
  assertThrows(
      () => instance.exports.cast(null), WebAssembly.RuntimeError,
      'illegal cast');
  assertThrows(
      () => instance.exports.cast_null(), WebAssembly.RuntimeError,
      'illegal cast');
})();

(function TestStringTest() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  builder.addFunction("test", kSig_i_shared)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, kStringTestShared,
    ]);

  builder.addFunction("test_null", kSig_i_v)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprCallFunction, kStringTestShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);

  assertEquals(1, instance.exports.test("foo"));
  assertEquals(0, instance.exports.test(123));
  assertEquals(0, instance.exports.test(undefined));
  assertEquals(0, instance.exports.test(true));
  assertEquals(0, instance.exports.test(null));
  assertEquals(0, instance.exports.test_null());
})();

(function TestIndexOfImportedStrings() {
  print(arguments.callee.name);
  let builder = new MakeBuilder();

  builder.addFunction('indexOf', kSig_i_ssi).exportFunc().addBody([
    kExprLocalGet, 0,
    kExprCallFunction, kStringCastShared,
    kExprLocalGet, 1,
    kExprCallFunction, kStringCastShared,
    kExprLocalGet, 2,
    kExprCallFunction, kStringIndexOfImported,
  ]);
  let instance = builder.instantiate(kImports, kBuiltins);

  assertEquals(2, instance.exports.indexOf('xxfooxx', 'foo', 0));
  assertEquals(2, instance.exports.indexOf('xxfooxx', 'foo', -2));
  assertEquals(-1, instance.exports.indexOf('xxfooxx', 'foo', 100));
  // Make sure we don't lose bits when Smi-tagging of the start position.
  assertEquals(-1, instance.exports.indexOf('xxfooxx', 'foo', 0x4000_0000));
  assertEquals(-1, instance.exports.indexOf('xxfooxx', 'foo', 0x2000_0000));
  assertEquals(
      2,
      instance.exports.indexOf(
          'xxfooxx', 'foo', 0x8000_0000));  // Negative i32.

  // Both first and second args should be non-null strings.
  assertThrows(
      () => instance.exports.indexOf('xxnullxx', null, 0),
      WebAssembly.RuntimeError, 'illegal cast');
  assertThrows(
      () => instance.exports.indexOf(12345, 234, 0), WebAssembly.RuntimeError,
      'illegal cast');
  assertThrows(
      () => instance.exports.indexOf(null, 'foo', 0), WebAssembly.RuntimeError,
      'illegal cast');
  assertThrows(
      () => instance.exports.indexOf(null, 'null', 0), WebAssembly.RuntimeError,
      'illegal cast');
})();

(function TestStringToLowerCaseImported() {
  print(arguments.callee.name);
  let builder = new MakeBuilder();

  builder.addFunction('toLowerCase', kSig_s_s).exportFunc().addBody([
    kExprLocalGet, 0,
    kExprCallFunction, kStringCastShared,
    kExprCallFunction, kStringToLowerCaseImported,
  ]);
  let instance = builder.instantiate(kImports, kBuiltins);

  assertEquals(
      'make this lowercase!',
      instance.exports.toLowerCase('MAKE THIS LOWERCASE!'));

  // The argument should be a non-null string.
  assertThrows(
      () => instance.exports.toLowerCase(null), WebAssembly.RuntimeError,
      'illegal cast');
  assertThrows(
      () => instance.exports.toLowerCase(123), WebAssembly.RuntimeError,
      'illegal cast');
})();

(function TestStringConst() {
  print(arguments.callee.name);
  let builder = MakeBuilder();
  for (let i = 0; i < interestingStrings.length; i++) {
    builder.addImportedGlobal('strings', i.toString(), kRefSharedExtern, false);
    builder.addFunction("string_const" + i, kSig_t_v).exportFunc()
      .addBody([kExprGlobalGet, i]);
  }
  for (let i = 0; i < interestingStrings.length; i++) {
    builder.addGlobal(kRefSharedExtern, false, false, [kExprGlobalGet, i])
      .exportAs("global" + i);
  }
  let instance = builder.instantiate(kImports, kBuiltins);
  for (let [i, str] of interestingStrings.entries()) {
    assertEquals(str, instance.exports["string_const" + i]());
    assertEquals(str, instance.exports["global" + i].value);
  }
})();

(function TestStringLength() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  builder.addFunction("string_length", kSig_i_shared)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, kStringLengthShared,
    ]);

  builder.addFunction("string_length_null", kSig_i_v)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprCallFunction, kStringLengthShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  for (let str of interestingStrings) {
    assertEquals(str.length, instance.exports.string_length(str));
  }

  assertThrows(() => instance.exports.string_length(null),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.string_length_null(),
               WebAssembly.RuntimeError, "illegal cast");
})();

(function TestStringConcat() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  builder.addFunction("concat", kSig_t_ss)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, kStringConcatShared,
    ]);

  builder.addFunction("concat_null_head", kSig_t_s)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprLocalGet, 0,
      kExprCallFunction, kStringConcatShared,
    ]);

  builder.addFunction("concat_null_tail", kSig_t_s)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprCallFunction, kStringConcatShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);

  for (let head of interestingStrings) {
    for (let tail of interestingStrings) {
      assertEquals(head + tail, instance.exports.concat(head, tail));
    }
  }

  assertThrows(() => instance.exports.concat(null, "hey"),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.concat('hey', null),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.concat_null_head("hey"),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.concat_null_tail("hey"),
               WebAssembly.RuntimeError, "illegal cast");
})();

(function TestStringEq() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  builder.addFunction("eq", kSig_i_ss)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, kStringEqualsShared,
    ]);

  builder.addFunction("eq_null_a", kSig_i_shared)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprLocalGet, 0,
      kExprCallFunction, kStringEqualsShared,
    ]);

  builder.addFunction("eq_null_b", kSig_i_shared)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprCallFunction, kStringEqualsShared,
    ]);

  builder.addFunction("eq_both_null", kSig_i_v)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprCallFunction, kStringEqualsShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);

  for (let head of interestingStrings) {
    for (let tail of interestingStrings) {
      let result = (head == tail) | 0;
      assertEquals(result, instance.exports.eq(head, tail));
      assertEquals(result, instance.exports.eq(head + head, tail + tail));
    }
    assertEquals(0, instance.exports.eq_null_a(head))
    assertEquals(0, instance.exports.eq(null, head))
    assertEquals(0, instance.exports.eq(head, null))
  }

  assertEquals(1, instance.exports.eq_both_null());
  assertEquals(1, instance.exports.eq(null, null));
})();

(function TestStringViewWtf16() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  builder.addFunction("get_codeunit", kSig_i_si)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, kStringCharCodeAtShared,
    ]);

  builder.addFunction("get_codeunit_null", kSig_i_v)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprI32Const, 0,
      kExprCallFunction, kStringCharCodeAtShared,
    ]);

  builder.addFunction("get_codepoint", kSig_i_si)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, kStringCodePointAtShared,
    ]);

  builder.addFunction("get_codepoint_null", kSig_i_v)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprI32Const, 0,
      kExprCallFunction, kStringCodePointAtShared,
    ]);

  builder.addFunction("slice", kSig_t_sii)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprLocalGet, 2,
      kExprCallFunction, kStringSubstringShared,
    ]);

  builder.addFunction("slice_null", kSig_t_v)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprI32Const, 0,
      kExprI32Const, 0,
      kExprCallFunction, kStringSubstringShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  for (let str of interestingStrings) {
    for (let i = 0; i < str.length; i++) {
      assertEquals(str.charCodeAt(i),
                   instance.exports.get_codeunit(str, i));
      assertEquals(str.codePointAt(i),
                   instance.exports.get_codepoint(str, i));
    }
    assertEquals(str, instance.exports.slice(str, 0, -1));
  }

  assertEquals("", instance.exports.slice("foo", 0, 0));
  assertEquals("f", instance.exports.slice("foo", 0, 1));
  assertEquals("fo", instance.exports.slice("foo", 0, 2));
  assertEquals("foo", instance.exports.slice("foo", 0, 3));
  assertEquals("foo", instance.exports.slice("foo", 0, 4));
  assertEquals("o", instance.exports.slice("foo", 1, 2));
  assertEquals("oo", instance.exports.slice("foo", 1, 3));
  assertEquals("oo", instance.exports.slice("foo", 1, 100));
  assertEquals("", instance.exports.slice("foo", 1, 0));
  assertEquals("", instance.exports.slice("foo", 3, 4));
  assertEquals("foo", instance.exports.slice("foo", 0, -1));
  assertEquals("", instance.exports.slice("foo", -1, 1));

  assertThrows(() => instance.exports.get_codeunit(null, 0),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.get_codeunit_null(),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.get_codeunit("", 0),
               WebAssembly.RuntimeError, "string offset out of bounds");
  assertThrows(() => instance.exports.get_codepoint(null, 0),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.get_codepoint_null(),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.get_codepoint("", 0),
               WebAssembly.RuntimeError, "string offset out of bounds");
  assertThrows(() => instance.exports.slice(null, 0, 0),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.slice_null(),
               WebAssembly.RuntimeError, "illegal cast");

  // Cover runtime code path for long slices.
  const prefix = "a".repeat(10);
  const slice = "x".repeat(40);
  const suffix = "b".repeat(40);
  const input = prefix + slice + suffix;
  const start = prefix.length;
  const end = start + slice.length;
  assertEquals(slice, instance.exports.slice(input, start, end));

  // Check that we create one-byte substrings when possible.
  let onebyte = instance.exports.slice("\u1234abcABCDE", 1, 4);
  assertEquals("abc", onebyte);
  assertTrue(isOneByteString(onebyte));

  // Check that the CodeStubAssembler implementation also creates one-byte
  // substrings.
  onebyte = instance.exports.slice("\u1234abcA", 1, 4);
  assertEquals("abc", onebyte);
  assertTrue(isOneByteString(onebyte));
  // Cover the code path that checks 8 characters at a time.
  onebyte = instance.exports.slice("\u1234abcdefgh\u1234", 1, 9);
  assertEquals("abcdefgh", onebyte);  // Exactly 8 characters.
  assertTrue(isOneByteString(onebyte));
  onebyte = instance.exports.slice("\u1234abcdefghij\u1234", 1, 11);
  assertEquals("abcdefghij", onebyte);  // Longer than 8.
  assertTrue(isOneByteString(onebyte));

  // Check that the runtime code path also creates one-byte substrings.
  assertTrue(isOneByteString(
      instance.exports.slice(input + "\u1234", start, end)));
})();

(function TestStringCompare() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  builder.addFunction("compare", kSig_i_ss)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprCallFunction, kStringCompareShared,
    ]);

  builder.addFunction("compare_null_a", kSig_i_shared)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprLocalGet, 0,
      kExprCallFunction, kStringCompareShared,
    ]);

  builder.addFunction("compare_null_b", kSig_i_shared)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprCallFunction, kStringCompareShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  for (let lhs of interestingStrings) {
    for (let rhs of interestingStrings) {
      const expected = lhs < rhs ? -1 : lhs > rhs ? 1 : 0;
      assertEquals(expected, instance.exports.compare(lhs, rhs));
    }
  }

  assertThrows(() => instance.exports.compare(null, "abc"),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.compare("abc", null),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.compare_null_a("abc"),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.compare_null_b("abc"),
               WebAssembly.RuntimeError, "illegal cast");
})();

(function TestStringCompareNullCheckStaticType() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  // Use a mix of nullable and non-nullable input types to the compare.
  builder.addFunction("compareLhsNullable",
                      makeSig([kRefNullSharedExtern, kRefNullSharedExtern],
                              [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprRefAsNonNull,
      kExprLocalGet, 1,
      kExprCallFunction, kStringCompareShared,
    ]);

  builder.addFunction("compareRhsNullable",
                      makeSig([kRefNullSharedExtern, kRefNullSharedExtern],
                              [kWasmI32]))
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprLocalGet, 1,
      kExprRefAsNonNull,
      kExprCallFunction, kStringCompareShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  assertThrows(() => instance.exports.compareLhsNullable(null, "abc"),
               WebAssembly.RuntimeError, "dereferencing a null pointer");
  assertThrows(() => instance.exports.compareLhsNullable("abc", null),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.compareRhsNullable(null, "abc"),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.compareRhsNullable("abc", null),
               WebAssembly.RuntimeError, "dereferencing a null pointer");
})();

(function TestStringFromCodePoint() {
  print(arguments.callee.name);
  let builder = MakeBuilder();
  builder.addFunction("asString", kSig_t_i)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, kStringFromCodePointShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  for (let char of "Az1#\n\ucccc\ud800\udc00") {
    assertEquals(char, instance.exports.asString(char.codePointAt(0)));
  }
  for (let codePoint of [0x110000, 0xFFFFFFFF, -1]) {
    assertThrows(() => instance.exports.asString(codePoint),
                 WebAssembly.RuntimeError, /Invalid code point -?[0-9]+/);
  }
})();

(function TestStringFromCharCode() {
  print(arguments.callee.name);
  let builder = MakeBuilder();
  builder.addFunction("asString", kSig_t_i)
    .exportFunc()
    .addBody([
      kExprLocalGet, 0,
      kExprCallFunction, kStringFromCharCodeShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  let inputs = "Az1#\n\ucccc\ud800\udc00";
  for (let i = 0; i < inputs.length; i++) {
    assertEquals(inputs.charAt(i),
                 instance.exports.asString(inputs.charCodeAt(i)));
  }
})();

function encodeWtf16LE(str) {
  // String iterator coalesces surrogate pairs.
  let out = [];
  for (let i = 0; i < str.length; i++) {
    codeunit = str.charCodeAt(i);
    out.push(codeunit & 0xff)
    out.push(codeunit >> 8);
  }
  return out;
}

function makeWtf16TestDataSegment(strings) {
  let data = []
  let valid = {};

  for (let str of strings) {
    valid[str] = { offset: data.length, length: str.length };
    for (let byte of encodeWtf16LE(str)) {
      data.push(byte);
    }
  }

  return { valid, data: Uint8Array.from(data) };
};

(function TestStringNewWtf16Array() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  // string.new_wtf16_array switches to a different implementation (runtime
  // instead of Torque) for more than 32 characters, so provide some coverage
  // for that case.
  let strings = interestingStrings.concat([
    "String with more than 32 characters, all of which are ASCII",
    "Two-byte string with more than 32 characters \ucccc \ud800\udc00 \xa9?"
  ]);

  let data = makeWtf16TestDataSegment(strings);
  let data_index = builder.addPassiveDataSegment(data.data);
  let ascii_data_index =
      builder.addPassiveDataSegment(Uint8Array.from(encodeWtf16LE("ascii")));

  let make_i16_array = builder.addFunction(
      "make_i16_array", makeSig([], [wasmRefType(kArrayI16Shared)]))
    .addBody([
      ...wasmI32Const(0),
      ...wasmI32Const(data.data.length / 2),
      kGCPrefix, kExprArrayNewData, kArrayI16Shared, data_index
    ]).index;

  builder.addFunction("new_wtf16", kSig_t_ii)
    .exportFunc()
    .addBody([
      kExprCallFunction, make_i16_array,
      kExprLocalGet, 0, kExprLocalGet, 1,
      kExprCallFunction, kStringFromWtf16ArrayShared,
    ]);

  builder.addFunction("bounds_check", kSig_t_ii)
    .exportFunc()
    .addBody([
      ...wasmI32Const(0),
      ...wasmI32Const("ascii".length),
      kGCPrefix, kExprArrayNewData, kArrayI16Shared, ascii_data_index,
      kExprLocalGet, 0, kExprLocalGet, 1,
      kExprCallFunction, kStringFromWtf16ArrayShared,
    ]);

  builder.addFunction("null_array", kSig_t_v).exportFunc()
    .addBody([
      kExprRefNull, kArrayI16Shared,
      kExprI32Const, 0, kExprI32Const, 0,
      kExprCallFunction, kStringFromWtf16ArrayShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  for (let [str, {offset, length}] of Object.entries(data.valid)) {
    let start = offset / 2;
    let end = start + length;
    assertEquals(str, instance.exports.new_wtf16(start, end));
  }

  assertEquals("ascii", instance.exports.bounds_check(0, "ascii".length));
  assertEquals("", instance.exports.bounds_check("ascii".length,
                                                 "ascii".length));
  assertEquals("i", instance.exports.bounds_check("ascii".length - 1,
                                                  "ascii".length));
  assertThrows(() => instance.exports.bounds_check(0, 100),
               WebAssembly.RuntimeError, "array element access out of bounds");
  assertThrows(() => instance.exports.bounds_check("ascii".length,
                                                   "ascii".length + 1),
               WebAssembly.RuntimeError, "array element access out of bounds");
  assertThrows(() => instance.exports.null_array(),
               WebAssembly.RuntimeError, "dereferencing a null pointer");
})();

(function TestStringEncodeWtf16Array() {
  print(arguments.callee.name);
  let builder = MakeBuilder();

  // Allocate an array and encode into it.  Then decode it.
  // (str, length, offset) -> str
  builder.addFunction("encode", kSig_t_sii)
    .exportFunc()
    .addLocals(wasmRefNullType(kArrayI16Shared), 1)
    .addLocals(kWasmI32, 1)
    .addBody([
      // Allocate buffer.
      kExprLocalGet, 1,
      kGCPrefix, kExprArrayNewDefault, kArrayI16Shared,
      kExprLocalSet, 3,

      // Write buffer, store number of code units written.
      kExprLocalGet, 0,
      kExprLocalGet, 3,
      kExprLocalGet, 2,
      kExprCallFunction, kStringToWtf16ArrayShared,
      kExprLocalSet, 4,

      // Read buffer.
      kExprLocalGet, 3,
      kExprLocalGet, 2,
      kExprLocalGet, 2, kExprLocalGet, 4, kExprI32Add,
      kExprCallFunction, kStringFromWtf16ArrayShared,
    ]);

  builder.addFunction("encode_null_string", kSig_i_v)
    .exportFunc()
    .addBody([
      kExprRefNull, kWasmSharedTypeForm, kExternRefCode,
      kExprI32Const, 0, kGCPrefix, kExprArrayNewDefault, kArrayI16Shared,
      kExprI32Const, 0,
      kExprCallFunction, kStringToWtf16ArrayShared,
    ]);

  builder.addFunction("encode_null_array", kSig_i_v)
    .exportFunc()
    .addBody([
      kExprI32Const, 0, kGCPrefix, kExprArrayNewDefault, kArrayI16Shared,
      kExprI32Const, 0, kExprI32Const, 0,
      kExprCallFunction, kStringFromWtf16ArrayShared,
      kExprRefNull, kArrayI16Shared,
      kExprI32Const, 0,
      kExprCallFunction, kStringToWtf16ArrayShared,
    ]);

  let instance = builder.instantiate(kImports, kBuiltins);
  for (let str of interestingStrings) {
    assertEquals(str, instance.exports.encode(str, str.length, 0));
    assertEquals(str, instance.exports.encode(str, str.length + 20, 10));
  }

  assertThrows(() => instance.exports.encode(null, 0, 0),
               WebAssembly.RuntimeError, "illegal cast");
  assertThrows(() => instance.exports.encode_null_array(),
               WebAssembly.RuntimeError, "dereferencing a null pointer");
  assertThrows(() => instance.exports.encode_null_string(),
               WebAssembly.RuntimeError, "illegal cast");

  for (let str of interestingStrings) {
    let message = "array element access out of bounds";
    assertThrows(() => instance.exports.encode(str, str.length, 1),
                 WebAssembly.RuntimeError, message);
  }
})();
