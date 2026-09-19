'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createCipheriv,
  createDecipheriv,
  getCiphers,
} = require('crypto');

const iv = Buffer.from('f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff', 'hex');
const plaintext = Buffer.from(
  '000102030405060708090a0b0c0d0e0f10', 'hex');
const cases = [
  {
    algorithm: 'aes-128-xts',
    key: Buffer.from(
      '000102030405060708090a0b0c0d0e0f' +
      '101112131415161718191a1b1c1d1e1f', 'hex'),
  },
  {
    algorithm: 'sm4-xts',
    key: Buffer.from(
      '2b7e151628aed2a6abf7158809cf4f3c' +
      '000102030405060708090a0b0c0d0e0f', 'hex'),
  },
];

for (const { algorithm, key } of cases) {
  if (!getCiphers().includes(algorithm)) {
    common.printSkipMessage(`unsupported ${algorithm} test`);
    continue;
  }

  const cipher = createCipheriv(algorithm, key, iv);
  const ciphertext = cipher.update(plaintext);
  assert.strictEqual(ciphertext.length, plaintext.length);
  assert.deepStrictEqual(cipher.final(), Buffer.alloc(0));

  const decipher = createDecipheriv(algorithm, key, iv);
  assert.deepStrictEqual(decipher.update(ciphertext), plaintext);
  assert.deepStrictEqual(decipher.final(), Buffer.alloc(0));

  for (const [create, input, expected] of [
    [createCipheriv, plaintext, ciphertext],
    [createDecipheriv, ciphertext, plaintext],
  ]) {
    const withoutUpdate = create(algorithm, key, iv);
    assert.throws(() => withoutUpdate.final(), /Unsupported state/);

    const failedUpdate = create(algorithm, key, iv);
    assert.throws(() => failedUpdate.update(Buffer.alloc(15)),
                  /Trying to add data in unsupported state/);
    assert.throws(() => failedUpdate.final(), /Unsupported state/);

    const retry = create(algorithm, key, iv);
    assert.throws(() => retry.update(Buffer.alloc(15)),
                  /Trying to add data in unsupported state/);
    assert.deepStrictEqual(retry.update(input), expected);
    assert.deepStrictEqual(retry.final(), Buffer.alloc(0));

    const oneUpdate = create(algorithm, key, iv);
    assert.deepStrictEqual(oneUpdate.update(input), expected);
    assert.throws(() => oneUpdate.update(Buffer.alloc(16)),
                  /Trying to add data in unsupported state/);
    assert.deepStrictEqual(oneUpdate.final(), Buffer.alloc(0));
  }
}
