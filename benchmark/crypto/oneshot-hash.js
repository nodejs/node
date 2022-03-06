'use strict';
const common = require('../common.js');
const { hash, createHash } = require('crypto');

const bench = common.createBenchmark(main, {
  mode: ['oneshot', 'stateful'],
  algo: ['sha1', 'sha256', 'sha512'],
  len: [2, 128, 1024, 64 * 1024],
  n: [1e5],
});

function testOneshot(algo, data, n) {
  bench.start();
  for (let i = 0; i < n; i++)
    hash(algo, data);
  bench.end(n);
}

function testStateful(algo, data, n) {
  bench.start();
  for (let i = 0; i < n; i++)
    createHash(algo).update(data).digest();
  bench.end(n);
}

function main({ mode, algo, len, n }) {
  const data = Buffer.alloc(len);
  if (mode === 'oneshot') {
    testOneshot(algo, data, n);
  } else {
    testStateful(algo, data, n);
  }
}
