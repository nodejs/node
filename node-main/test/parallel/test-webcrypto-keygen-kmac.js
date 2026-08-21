'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3))
  common.skip('requires OpenSSL >= 3');

const assert = require('assert');
const { types: { isCryptoKey } } = require('util');
const { subtle } = globalThis.crypto;

const usages = ['sign', 'verify'];

async function test(name, length) {
  length = length ?? name === 'KMAC128' ? 128 : 256;
  const key = await subtle.generateKey({
    name,
    length,
  }, true, usages);

  assert(key);
  assert(isCryptoKey(key));

  assert.strictEqual(key.type, 'secret');
  assert.strictEqual(key.toString(), '[object CryptoKey]');
  assert.strictEqual(key.extractable, true);
  assert.deepStrictEqual(key.usages, usages);
  assert.strictEqual(key.algorithm.name, name);
  assert.strictEqual(key.algorithm.length, length);
  assert.strictEqual(key.algorithm, key.algorithm);
  assert.strictEqual(key.usages, key.usages);
}

const kTests = [
  ['KMAC128', 128],
  ['KMAC128', 256],
  ['KMAC128'],
  ['KMAC256', 128],
  ['KMAC256', 256],
  ['KMAC256'],
];

const tests = Promise.all(kTests.map((args) => test(...args)));

tests.then(common.mustCall());
