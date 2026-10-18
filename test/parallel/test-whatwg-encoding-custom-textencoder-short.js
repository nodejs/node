'use strict';

// Tests TextEncoder.prototype.encode() around the short string path.

require('../common');

const assert = require('assert');

const encoder = new TextEncoder();

const inputs = [
  '',
  'a',
  'a'.repeat(32),
  'a'.repeat(33),
  '\xff'.repeat(32),
  'ğ'.repeat(32),
  'ࠀ'.repeat(32),
  '\u{1F44B}'.repeat(16),
  '\u{1F44B}'.repeat(17),
  '\ud800',
  'a\udc00b',
  'abc\ud83d',
];

for (const input of inputs) {
  const result = encoder.encode(input);
  const expected = Buffer.from(input, 'utf8');
  assert.strictEqual(Object.getPrototypeOf(result), Uint8Array.prototype);
  assert.strictEqual(result.byteOffset, 0);
  assert.strictEqual(result.buffer.byteLength, expected.length);
  assert.deepStrictEqual(Buffer.from(result), expected);
}

