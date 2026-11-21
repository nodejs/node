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
  const expectedLength = length ?? (name === 'KMAC128' ? 128 : 256);
  const algorithm = { name };
  if (length !== undefined)
    algorithm.length = length;

  const generatedKey = await subtle.generateKey(algorithm, true, usages);

  assert(generatedKey);
  assert(isCryptoKey(generatedKey));

  assert.strictEqual(generatedKey.type, 'secret');
  assert.strictEqual(generatedKey.toString(), '[object CryptoKey]');
  assert.strictEqual(generatedKey.extractable, true);
  assert.deepStrictEqual(generatedKey.usages, usages);
  assert.strictEqual(generatedKey.algorithm.name, name);
  assert.strictEqual(generatedKey.algorithm.length, expectedLength);
  assert.strictEqual(generatedKey.algorithm, generatedKey.algorithm);
  assert.strictEqual(generatedKey.usages, generatedKey.usages);

  const raw = await subtle.exportKey('raw-secret', generatedKey);
  assert.strictEqual(raw.byteLength, Math.ceil(expectedLength / 8));
}

const kTests = [
  ['KMAC128', 32],
  ['KMAC128', 128],
  ['KMAC128', 256],
  ['KMAC128'],
  ['KMAC256', 32],
  ['KMAC256', 128],
  ['KMAC256', 256],
  ['KMAC256'],
];

const tests = Promise.all(kTests.map((args) => test(...args)));

tests.then(common.mustCall());

(async () => {
  for (const name of ['KMAC128', 'KMAC256']) {
    for (const length of [0, 8, 16, 24]) {
      await assert.rejects(
        subtle.generateKey({ name, length }, true, usages),
        { name: 'NotSupportedError', message: 'Invalid key length' });
    }
  }
})().then(common.mustCall());
