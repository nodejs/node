'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  len: [64, 1024],
  n: [1e6],
});

function main({ len, n }) {
  const buf = Buffer.alloc(len);

  for (let i = 0; i < buf.length; i++)
    buf[i] = i & 0xff;

  const hex = buf.toString('hex');
  let tmp;

  bench.start();

  for (let i = 0; i < n; i += 1)
    tmp = Buffer.from(hex, 'hex');

  bench.end(n);

  assert.strictEqual(typeof tmp, 'object');
}
