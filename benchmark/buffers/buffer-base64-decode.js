'use strict';
const assert = require('assert');
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [32],
  size: [8 << 20]
});

function main({ n, size }) {
  const s = 'abcd'.repeat(size);
  // eslint-disable-next-line node-core/no-unescaped-regexp-dot
  s.match(/./);  // Flatten string.
  assert.strictEqual(s.length % 4, 0);
  const b = Buffer.allocUnsafe(s.length / 4 * 3);
  b.write(s, 0, s.length, 'base64');
  bench.start();
  for (var i = 0; i < n; i += 1) b.base64Write(s, 0, s.length);
  bench.end(n);
}
