// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test circular structure error message with a max-length constructor name
// (via Symbol.toStringTag).
const longTag = 'a'.repeat(%StringMaxLength());
const obj = {[Symbol.toStringTag]: longTag};
obj.self = obj;

const expectedTagMessage =
    'Converting circular structure to JSON\n' +
    '    --> starting at object with constructor ' +
    `'${'a'.repeat(1000)}<...>'\n` +
    "    --- property 'self' closes the circle";
assertThrows(() => JSON.stringify(obj), TypeError, expectedTagMessage);

// Test circular structure error message with a long property name.
const longKey = 'b'.repeat(2000);
const obj2 = {};
obj2[longKey] = obj2;

const expectedKeyMessage =
    'Converting circular structure to JSON\n' +
    "    --> starting at object with constructor 'Object'\n" +
    `    --- property '${'b'.repeat(1000)}<...>' closes the circle`;
assertThrows(() => JSON.stringify(obj2), TypeError, expectedKeyMessage);

// Exact boundary length (1000) should not be truncated.
const exactTag = 'c'.repeat(1000);
const obj3 = {[Symbol.toStringTag]: exactTag};
obj3.self = obj3;
const expectedExactMessage =
    'Converting circular structure to JSON\n' +
    `    --> starting at object with constructor '${exactTag}'\n` +
    "    --- property 'self' closes the circle";
assertThrows(() => JSON.stringify(obj3), TypeError, expectedExactMessage);

// Two-byte max-length constructor name exercises the two-byte copy path in
// AppendStringCapped without flattening.
const longTwoByteTag = '\u1234'.repeat(%StringMaxLength());
const obj4 = {[Symbol.toStringTag]: longTwoByteTag};
obj4.self = obj4;
const expectedTwoByteMessage =
    'Converting circular structure to JSON\n' +
    '    --> starting at object with constructor ' +
    `'${'\u1234'.repeat(1000)}<...>'\n` +
    "    --- property 'self' closes the circle";
assertThrows(() => JSON.stringify(obj4), TypeError, expectedTwoByteMessage);
