'use strict';

const common = require('../common.js');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  length: [32, 1024],
  position: ['start', 'end'],
  encoding: ['valid', 'invalid', 'unicode', 'plain'],
});

function main({ n, length, position, encoding }) {
  const prefix = 'a'.repeat(length);
  const [encoded, decoded] = {
    valid: ['%20', ' '],
    invalid: ['%zz', '%zz'],
    unicode: ['%E2%82%AC', '\u20ac'],
    plain: ['text', 'text'],
  }[encoding];
  const input = position === 'start' ? encoded + prefix : prefix + encoded;
  const expected = position === 'start' ? decoded + prefix : prefix + decoded;
  const query = `key=${input}`;
  for (let index = 0; index < 1000; index++) {
    new URLSearchParams(query);
  }

  let result;
  bench.start();
  for (let index = 0; index < n; index++) {
    result = new URLSearchParams(query);
  }
  bench.end(n);
  assert.deepStrictEqual([...result], [['key', expected]]);
}
