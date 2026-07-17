'use strict';

const common = require('../common.js');
const { createHmac } = require('crypto');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  algo: ['sha1', 'sha256', 'sha512'],
  keylen: [0, 16, 64, 1024],
});

function main({ n, algo, keylen }) {
  const key = Buffer.alloc(keylen, 'k');
  const hmacs = new Array(n);

  bench.start();
  for (let i = 0; i < n; ++i) {
    hmacs[i] = createHmac(algo, key);
  }
  bench.end(n);

  assert.strictEqual(typeof hmacs[n - 1], 'object');
}
