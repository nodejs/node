'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  len: [32, 256, 1024, 1024 * 8],
  n: [1e6],
  type: ['one-byte-string', 'two-byte-string', 'ascii'],
  op: ['encode', 'encodeInto'],
});

function main({ n, op, len, type }) {
  const encoder = new TextEncoder();
  let base = '';

  switch (type) {
    case 'ascii':
      base = 'a';
      break;
    case 'one-byte-string':
      base = '\xff';
      break;
    case 'two-byte-string':
      base = 'ğ';
      break;
  }

  const input = base.repeat(len);
  if (op === 'encode') {
    const expected = encoder.encode(input);
    let result;
    bench.start();
    for (let i = 0; i < n; i++)
      result = encoder.encode(input);
    bench.end(n);
    assert.deepStrictEqual(result, expected);
  } else {
    const expected = new Uint8Array(len);
    const subarray = new Uint8Array(len);
    const expectedStats = encoder.encodeInto(input, expected);
    let result;
    bench.start();
    for (let i = 0; i < n; i++)
      result = encoder.encodeInto(input, subarray);
    bench.end(n);
    assert.deepStrictEqual(subarray, expected);
    assert.deepStrictEqual(result, expectedStats);
  }
}
