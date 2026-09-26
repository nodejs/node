'use strict';

const common = require('../common.js');
const buffer = require('node:buffer');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e7],
  length: ['short', 'long'],
  // onebyte: one-byte representation (O(1) check)
  // twobyte: two-byte representation containing only code units <= 0xFF
  // invalid: ends with a code unit > 0xFF
  input: ['onebyte', 'twobyte', 'invalid'],
  method: ['isByteString', 'loop', 'regex'],
});

function loop(str) {
  for (let i = 0; i < str.length; i++) {
    if (str.charCodeAt(i) > 255) return false;
  }
  return true;
}

const notByteStringRe = /[\u0100-\uffff]/;
function regex(str) {
  return !notByteStringRe.test(str);
}

const methods = { isByteString: buffer.isByteString, loop, regex };

function main({ n, length, input, method }) {
  const base = length === 'short' ? 'hello w\u00f6rld' : 'hello w\u00f6rld'.repeat(200);
  let str;
  switch (input) {
    case 'onebyte':
      str = base;
      break;
    case 'twobyte':
      // Slicing a two-byte string keeps the two-byte representation.
      str = ('\u0100' + base).slice(1);
      break;
    case 'invalid':
      str = base + '\u0100';
      break;
  }
  const expected = input !== 'invalid';
  const fn = methods[method];
  assert.strictEqual(fn(str), expected);

  bench.start();
  let result;
  for (let i = 0; i < n; ++i) {
    result = fn(str);
  }
  bench.end(n);
  assert.strictEqual(result, expected);
}
