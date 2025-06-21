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

  const plain = buf;

  bench.start();

  let tmp;

  for (let i = 0; i < n; i += 1)
    tmp = plain.toString('hex');

  bench.end(n);

  assert.strictEqual(typeof tmp, 'string');
}
