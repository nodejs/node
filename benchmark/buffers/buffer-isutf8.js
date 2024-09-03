'use strict';

const common = require('../common.js');
const buffer = require('node:buffer');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  n: [2e7],
  length: ['short', 'long'],
  input: ['regular string', 'ğ'],
});


function main({ n, input }) {
  const normalizedInput = input === 'short' ? input : input.repeat(300);
  const encoder = new TextEncoder();
  const buff = encoder.encode(normalizedInput);
  bench.start();
  for (let i = 0; i < n; ++i) {
    assert.ok(buffer.isUtf8(buff));
  }
  bench.end(n);
}
