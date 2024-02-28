'use strict';
const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  len: [64 * 1024 * 1024],
  n: [32],
}, {
  test: { len: 256 },
});

function main({ n, len }) {
  const b = Buffer.allocUnsafe(len);
  let s = '';
  let i;
  for (i = 0; i < 256; ++i) s += String.fromCharCode(i);
  for (i = 0; i < len; i += 256) b.write(s, i, 256, 'ascii');

  let tmp;

  bench.start();

  for (i = 0; i < n; ++i)
    tmp = b.toString('base64url');

  bench.end(n);

  assert.strictEqual(typeof tmp, 'string');
}
