'use strict';

const common = require('../common.js');
const assert = require('node:assert');
const {
  createCipheriv,
  createDecipheriv,
  getCiphers,
} = require('node:crypto');

const configurations = {
  'aes-128-cbc': { keyLength: 16, ivLength: 16 },
  'aes-128-gcm': { keyLength: 16, ivLength: 12 },
  'aes-128-cbc-cts': { keyLength: 16, ivLength: 16 },
  'aes-128-wrap-inv': { keyLength: 16, ivLength: 8 },
  'aes128-wrap-inv': {
    keyLength: 16,
    ivLength: 8,
    warmupCipher: 'aes-128-wrap-inv',
  },
};

const ciphers = ['aes-128-cbc', 'aes-128-gcm'];
const availableCiphers = new Set(getCiphers());
for (const cipher of [
  'aes-128-cbc-cts',
  'aes-128-wrap-inv',
  'aes128-wrap-inv',
]) {
  if (availableCiphers.has(cipher)) {
    ciphers.push(cipher);
  }
}

const bench = common.createBenchmark(main, {
  n: [1e5],
  cipher: ciphers,
  operation: ['encrypt', 'decrypt'],
});

function main({ n, cipher, operation }) {
  const {
    keyLength,
    ivLength,
    warmupCipher = cipher,
  } = configurations[cipher];
  const key = Buffer.alloc(keyLength);
  const iv = Buffer.alloc(ivLength);
  const results = new Array(n);
  const method = operation === 'encrypt' ? createCipheriv : createDecipheriv;

  const warmup = method(warmupCipher, key, iv);
  assert.strictEqual(typeof warmup, 'object');

  bench.start();
  for (let i = 0; i < n; ++i) {
    results[i] = method(cipher, key, iv);
  }
  bench.end(n);

  assert.strictEqual(typeof results[n - 1], 'object');
}
