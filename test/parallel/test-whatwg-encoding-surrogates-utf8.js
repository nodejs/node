'use strict';

// From: https://github.com/w3c/web-platform-tests/blob/fa9436d12c/encoding/api-surrogates-utf8.html

require('../common');

const assert = require('assert');
const {
  TextDecoder,
  TextEncoder
} = require('util');

const badStrings = [
  {
    input: 'abc123',
    expected: [0x61, 0x62, 0x63, 0x31, 0x32, 0x33],
    decoded: 'abc123',
    name: 'Sanity check'
  },
  {
    input: '\uD800',
    expected: [0xef, 0xbf, 0xbd],
    decoded: '\uFFFD',
    name: 'Surrogate half (low)'
  },
  {
    input: '\uDC00',
    expected: [0xef, 0xbf, 0xbd],
    decoded: '\uFFFD',
    name: 'Surrogate half (high)'
  },
  {
    input: 'abc\uD800123',
    expected: [0x61, 0x62, 0x63, 0xef, 0xbf, 0xbd, 0x31, 0x32, 0x33],
    decoded: 'abc\uFFFD123',
    name: 'Surrogate half (low), in a string'
  },
  {
    input: 'abc\uDC00123',
    expected: [0x61, 0x62, 0x63, 0xef, 0xbf, 0xbd, 0x31, 0x32, 0x33],
    decoded: 'abc\uFFFD123',
    name: 'Surrogate half (high), in a string'
  },
  {
    input: '\uDC00\uD800',
    expected: [0xef, 0xbf, 0xbd, 0xef, 0xbf, 0xbd],
    decoded: '\uFFFD\uFFFD',
    name: 'Wrong order'
  }
];

badStrings.forEach((t) => {
  const encoded = new TextEncoder().encode(t.input);
  assert.deepStrictEqual([].slice.call(encoded), t.expected);
  assert.strictEqual(new TextDecoder('utf-8').decode(encoded), t.decoded);
});
