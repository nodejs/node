// Flags: --js-immutable-arraybuffer
'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');

// Native Buffer mutators must preserve immutable backing storage, whether they
// reject the operation or return without modifying the bytes.
function checkImmutable(mutate) {
  const expected = Array.from({ length: 64 }, (_, i) => i);
  const ab = Uint8Array.from(expected).buffer.transferToImmutable();
  const buffer = Buffer.from(ab);

  try {
    mutate(buffer);
  } catch (err) {
    if (!(err instanceof TypeError)) throw err;
  }

  assert.deepStrictEqual(Array.from(new Uint8Array(ab)), expected);
}

const encodings = [
  ['utf8', 'hello'],
  ['utf-8', 'hello'],
  ['ascii', 'hello'],
  ['latin1', 'hello'],
  ['binary', 'hello'],
  ['hex', '68656c6c6f'],
  ['base64', 'aGVsbG8='],
  ['base64url', 'aGVsbG8'],
  ['ucs2', 'hello'],
  ['ucs-2', 'hello'],
  ['utf16le', 'hello'],
  ['utf-16le', 'hello'],
];

for (const [encoding, value] of encodings) {
  test(`Buffer.write with ${encoding} preserves immutable bytes`, () => {
    checkImmutable((buffer) => buffer.write(value, encoding));
  });
}

const writers = [
  ['utf8Write', 'hello'],
  ['asciiWrite', 'hello'],
  ['latin1Write', 'hello'],
  ['hexWrite', '68656c6c6f'],
  ['base64Write', 'aGVsbG8='],
  ['base64urlWrite', 'aGVsbG8'],
  ['ucs2Write', 'hello'],
];

for (const [method, value] of writers) {
  test(`Buffer.${method} preserves immutable bytes`, () => {
    checkImmutable((buffer) => buffer[method](value, 0, buffer.length));
  });
}

const fills = [
  ['string', 'AB'],
  ['non-ASCII character', 'é'],
  ['Buffer', Buffer.from([0xfe, 0xff])],
  ['coerced value', true],
];

for (const [name, value] of fills) {
  test(`Buffer.fill with ${name} preserves immutable bytes`, () => {
    checkImmutable((buffer) => buffer.fill(value));
  });
}

for (const method of ['swap16', 'swap32', 'swap64']) {
  test(`Buffer.${method} preserves immutable bytes`, () => {
    // A 64-byte buffer reaches the native path for all three swap methods.
    checkImmutable((buffer) => buffer[method]());
  });
}
