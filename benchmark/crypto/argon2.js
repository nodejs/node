'use strict';

const common = require('../common.js');
const assert = require('node:assert');
const {
  argon2,
  argon2Sync,
  randomBytes,
} = require('node:crypto');

const bench = common.createBenchmark(main, {
  sync: [0, 1],
  algorithm: ['ARGON2D', 'ARGON2I', 'ARGON2ID'],
  iter: [2, 3, 4],
  threads: [2, 4, 8],
  lanes: [2, 4, 8],
  memcost: [16 << 10, 64 << 10],
  n: [50],
}, {
  combinationFilter: ({ threads, lanes }) => threads <= lanes,
});

function measureSync(n, pass, salt, options) {
  bench.start();
  for (let i = 0; i < n; ++i)
    argon2Sync(pass, salt, 64, options);
  bench.end(n);
}

function measureAsync(n, pass, salt, options) {
  let remaining = n;
  function done(err) {
    assert.ifError(err);
    if (--remaining === 0)
      bench.end(n);
  }
  bench.start();
  for (let i = 0; i < n; ++i)
    argon2(pass, salt, 64, options, done);
}

function main({ n, sync, ...options }) {
  // pass, salt, secret, ad & output length does not affect performance
  const pass = randomBytes(32);
  const salt = randomBytes(16);
  if (sync)
    measureSync(n, pass, salt, options);
  else
    measureAsync(n, pass, salt, options);
}
