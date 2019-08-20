// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --expose-wasm

'use strict';

load('test/mjsunit/wasm/wasm-module-builder.js');

function module(bytes) {
  let buffer = bytes;
  if (typeof buffer === 'string') {
    buffer = new ArrayBuffer(bytes.length);
    let view = new Uint8Array(buffer);
    for (let i = 0; i < bytes.length; ++i) {
      view[i] = bytes.charCodeAt(i);
    }
  }
  return new WebAssembly.Module(buffer);
}

function testErrorPosition(bytes, pos, test_name) {
  assertThrowsAsync(
      WebAssembly.compile(bytes.trunc_buffer()), WebAssembly.CompileError,
      new RegExp('@\\+' + pos));
}

(function testInvalidMagic() {
  let bytes = new Binary;
  bytes.emit_bytes([
    kWasmH0, kWasmH1 + 1, kWasmH2, kWasmH3, kWasmV0, kWasmV1, kWasmV2, kWasmV3
  ]);
  // Error at pos==0 because that's where the magic word is.
  testErrorPosition(bytes, 0, 'testInvalidMagic');
})();

(function testInvalidVersion() {
  let bytes = new Binary;
  bytes.emit_bytes([
    kWasmH0, kWasmH1, kWasmH2, kWasmH3, kWasmV0, kWasmV1 + 1, kWasmV2, kWasmV3
  ]);
  // Error at pos==4 because that's where the version word is.
  testErrorPosition(bytes, 4, 'testInvalidVersion');
})();

(function testSectionLengthInvalidVarint() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_u8(kTypeSectionCode);
  bytes.emit_bytes([0x80, 0x80, 0x80, 0x80, 0x80, 0x00]);
  let pos = bytes.length - 1 - 1;
  testErrorPosition(bytes, pos, 'testSectionLengthInvalidVarint');
})();

(function testSectionLengthTooBig() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_u8(kTypeSectionCode);
  bytes.emit_u32v(0xffffff23);
  let pos = bytes.length - 1;
  testErrorPosition(bytes, pos, 'testSectionLengthTooBig');
})();

(function testFunctionsCountInvalidVarint() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,  // section id
      1,                 // section length
      0                  // number of types
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      1,                     // section length
      0                      // number of functions
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
  ]);
  // Functions count
  bytes.emit_bytes([0x80, 0x80, 0x80, 0x80, 0x80, 0x00]);

  let pos = bytes.length - 1 - 1;
  testErrorPosition(bytes, pos, 'testFunctionsCountInvalidVarint');
})();

(function testFunctionsCountTooBig() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,  // section id
      1,                 // section length
      0                  // number of types
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      1,                     // section length
      0                      // number of functions
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
  ]);
  // Functions count
  bytes.emit_u32v(0xffffff23);

  let pos = bytes.length - 1;
  testErrorPosition(bytes, pos, 'testFunctionsCountTooBig');
})();

(function testFunctionsCountDoesNotMatch() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,  // section id
      1,                 // section length
      0                  // number of types
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      1,                     // section length
      0                      // number of functions
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
  ]);
  // Functions count (different than the count in the functions section.
  bytes.emit_u32v(5);

  let pos = bytes.length - 1;
  testErrorPosition(bytes, pos, 'testFunctionsCountDoesNotMatch');
})();

(function testBodySizeInvalidVarint() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
      1                  // functions count
  ]);
  // Invalid function body size.
  bytes.emit_bytes([0x80, 0x80, 0x80, 0x80, 0x80, 0x00]);

  let pos = bytes.length - 1 - 1;
  testErrorPosition(bytes, pos, 'testBodySizeInvalidVarint');
})();

(function testBodySizeTooBig() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
      1                  // functions count
  ]);
  // Invalid function body size.
  bytes.emit_u32v(0xffffff23);

  let pos = bytes.length - 1;
  testErrorPosition(bytes, pos, 'testBodySizeTooBig');
})();

(function testBodySizeDoesNotFit() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
      1                  // functions count
  ]);
  // Invalid function body size (does not fit into the code section).
  bytes.emit_u32v(20);

  let pos = bytes.length - 1;
  testErrorPosition(bytes, pos, 'testBodySizeDoesNotFit');
})();

(function testBodySizeIsZero() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (arbitrary value > 6)
      1                  // functions count
  ]);
  // Invalid function body size (body size of 0 is invalid).
  bytes.emit_u32v(0);

  let pos = bytes.length - 1;
  testErrorPosition(bytes, pos, 'testBodySizeIsZero');
})();

(function testStaleCodeSectionBytes() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      20,                // section length (too big)
      1,                 // functions count
      2,                 // body size
      0,                 // locals count
      kExprEnd           // body
  ]);

  let pos = bytes.length - 1;
  testErrorPosition(bytes, pos, 'testStaleCodeSectionBytes');
})();

(function testInvalidCode() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      6,                 // section length (too big)
      1,                 // functions count
      4,                 // body size
      0,                 // locals count
      kExprGetLocal, 0,  // Access a non-existing local
      kExprEnd           // --
  ]);

  // Find error at the index of kExprGetLocal.
  let pos = bytes.length - 1 - 1;
  testErrorPosition(bytes, pos, 'testInvalidCode');
})();

(function testCodeSectionSizeZero() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      4,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      0,                      // number of parameter
      0                       // number of returns
  ]);
  bytes.emit_bytes([
      kFunctionSectionCode,  // section id
      2,                     // section length
      1,                     // number of functions
      0                      // signature index
  ]);
  bytes.emit_bytes([
      kCodeSectionCode,  // section id
      0,                 // section length (too big)
  ]);

  // Find error at the index of kExprGetLocal.
  let pos = bytes.length - 1;
  testErrorPosition(bytes, pos, 'testCodeSectionSizeZero');
})();

(function testInvalidSection() {
  let bytes = new Binary;
  bytes.emit_header();
  bytes.emit_bytes([
      kTypeSectionCode,       // section id
      5,                      // section length
      1,                      // number of types
      kWasmFunctionTypeForm,  // type
      1,                      // number of parameter
      0x7b,                   // invalid type
      0                       // number of returns
  ]);

  let pos = bytes.length - 1 - 1;
  testErrorPosition(bytes, pos, 'testInvalidSection');
})();
