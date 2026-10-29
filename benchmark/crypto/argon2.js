'use strict';

const common = require('../common.js');
const { hasOpenSSL } = require('../../test/common/crypto.js');
const assert = require('node:assert');
const {
  argon2,
  argon2Sync,
  randomBytes,
} = require('node:crypto');

if (!hasOpenSSL(3, 2)) {
  console.log('Skipping: Argon2 requires OpenSSL >= 3.2');
  process.exit(0);
}

const bench = common.createBenchmark(main, {
  mode: ['sync', 'async'],
  algorithm: ['argon2d', 'argon2i', 'argon2id'],
  passes: [3],
  parallelism: [1],
  // Argon2 memory cost dominates runtime. Keep the default suite small enough
  // to finish while still covering a low-cost and a memory-heavy derivation.
  memory: [2 ** 11, 2 ** 16],
  n: [5],
});

function measureSync(n, algorithm, message, nonce, options) {
  bench.start();
  for (let i = 0; i < n; ++i)
    argon2Sync(algorithm, { ...options, message, nonce, tagLength: 64 });
  bench.end(n);
}

function measureAsync(n, algorithm, message, nonce, options) {
  let remaining = n;
  function done(err) {
    assert.ifError(err);
    if (--remaining === 0)
      bench.end(n);
  }
  bench.start();
  for (let i = 0; i < n; ++i)
    argon2(algorithm, { ...options, message, nonce, tagLength: 64 }, done);
}

function main({ n, mode, algorithm, ...options }) {
  // Message, nonce, secret, associated data & tag length do not affect performance
  const message = randomBytes(32);
  const nonce = randomBytes(16);
  if (mode === 'sync')
    measureSync(n, algorithm, message, nonce, options);
  else
    measureAsync(n, algorithm, message, nonce, options);
}
