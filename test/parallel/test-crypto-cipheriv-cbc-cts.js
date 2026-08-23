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

const algorithm = 'aes-128-cbc-cts';
const key = Buffer.from('636869636b656e207465726979616b69', 'hex');
const iv = Buffer.alloc(16);

for (const create of [createCipheriv, createDecipheriv]) {
  for (const ctsMode of ['cs1', 'CS4', '']) {
    assert.throws(
      () => create('aes-128-cbc', key, iv, { ctsMode }),
      { code: 'ERR_INVALID_ARG_VALUE' });
  }
  for (const ctsMode of [1, true, {}]) {
    assert.throws(
      () => create('aes-128-cbc', key, iv, { ctsMode }),
      { code: 'ERR_INVALID_ARG_TYPE' });
  }
  assert.throws(
    () => create('aes-128-cbc', key, iv, { ctsMode: 'CS1' }),
    { code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION' });
}

if (!getCiphers().includes(algorithm)) {
  common.printSkipMessage(`unsupported ${algorithm}`);
  return;
}

// OpenSSL AES-128-CBC-CTS CS1 test vector.
const plaintext = Buffer.from('4920776f756c64206c696b652074686520',
                              'hex');
const expected = Buffer.from('97c6353568f2bf8cb4d8a580362da7ff7f',
                             'hex');

const cipher = createCipheriv(algorithm, key, iv);
const ciphertext = Buffer.concat([
  cipher.update(plaintext),
  cipher.final(),
]);
assert.deepStrictEqual(ciphertext, expected);

const decipher = createDecipheriv(algorithm, key, iv);
const decrypted = Buffer.concat([
  decipher.update(ciphertext),
  decipher.final(),
]);
assert.deepStrictEqual(decrypted, plaintext);

const vectors = [
  {
    plaintext,
    ciphertext: {
      CS1: expected,
      CS2: Buffer.from('c6353568f2bf8cb4d8a580362da7ff7f97', 'hex'),
      CS3: Buffer.from('c6353568f2bf8cb4d8a580362da7ff7f97', 'hex'),
    },
  },
  {
    plaintext: Buffer.from(
      '4920776f756c64206c696b6520746865' +
      '2047656e6572616c2047617527732043', 'hex'),
    ciphertext: {
      CS1: Buffer.from(
        '97687268d6ecccc0c07b25e25ecfe584' +
        '39312523a78662d5be7fcbcc98ebf5a8', 'hex'),
      CS2: Buffer.from(
        '97687268d6ecccc0c07b25e25ecfe584' +
        '39312523a78662d5be7fcbcc98ebf5a8', 'hex'),
      CS3: Buffer.from(
        '39312523a78662d5be7fcbcc98ebf5a8' +
        '97687268d6ecccc0c07b25e25ecfe584', 'hex'),
    },
  },
];

for (const { plaintext, ciphertext } of vectors) {
  for (const ctsMode of ['CS1', 'CS2', 'CS3']) {
    const cipher = createCipheriv(algorithm, key, iv, { ctsMode });
    const encrypted = Buffer.concat([
      cipher.update(plaintext),
      cipher.final(),
    ]);
    assert.deepStrictEqual(encrypted, ciphertext[ctsMode]);

    const decipher = createDecipheriv(algorithm, key, iv, { ctsMode });
    const decrypted = Buffer.concat([
      decipher.update(encrypted),
      decipher.final(),
    ]);
    assert.deepStrictEqual(decrypted, plaintext);
  }
}

const tooShort = createCipheriv(algorithm, key, iv);
assert.throws(() => tooShort.update(Buffer.alloc(15)),
              /Trying to add data in unsupported state/);
assert.deepStrictEqual(Buffer.concat([
  tooShort.update(plaintext),
  tooShort.final(),
]), expected);

for (const [create, input] of [
  [createCipheriv, plaintext],
  [createDecipheriv, expected],
]) {
  const withoutUpdate = create(algorithm, key, iv);
  assert.throws(() => withoutUpdate.final(), /Unsupported state/);

  const multipleUpdates = create(algorithm, key, iv);
  multipleUpdates.update(input.subarray(0, 16));
  assert.throws(() => multipleUpdates.update(input.subarray(16)),
                /Trying to add data in unsupported state/);
}
