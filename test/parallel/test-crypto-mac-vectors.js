// Flags: --expose-internals

'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3) || process.features.openssl_is_boringssl) {
  common.skip('OpenSSL 3 EVP_MAC support is required');
}

const assert = require('node:assert');
const { encodingsMap } = require('internal/util');
const {
  createMac,
  getCiphers,
  getMacs,
} = require('node:crypto');

const availableMacs = new Set(getMacs());
const availableCiphers = new Set(getCiphers());
const kmacVectors = require('../fixtures/crypto/kmac')();
const gmacIVStorage = Uint8Array.from([
  0xff,
  ...Buffer.alloc(12),
  0xff,
]);
const blake2bSaltStorage = Uint8Array.from([
  0xff,
  ...Buffer.from('000102030405060708090a0b0c0d0e0f', 'hex'),
  0xff,
]);
const blake2sSaltStorage = Uint8Array.from([
  0xff,
  ...Buffer.from('0001020304050607', 'hex'),
  0xff,
]);

const vectors = [
  {
    label: 'CMAC-AES-128',
    algorithm: 'cmac',
    options: { cipher: 'aes-128-cbc' },
    key: '2b7e151628aed2a6abf7158809cf4f3c',
    data: '',
    expected: 'bb1d6929e95937287fa37d129b756746',
    cipher: 'aes-128-cbc',
  },
  {
    label: 'GMAC-AES-128',
    algorithm: 'gmac',
    options: {
      cipher: 'aes-128-gcm',
      iv: new DataView(gmacIVStorage.buffer, 1, 12),
    },
    key: '00000000000000000000000000000000',
    data: '',
    expected: '58e2fccefa7e3061367f1d57a4e7455a',
    cipher: 'aes-128-gcm',
  },
  {
    label: 'Poly1305',
    algorithm: 'poly1305',
    key: '85d6be7857556d337f4452fe42d506a8' +
         '0103808afb0db2fd4abff6af4149f51b',
    data: Buffer.from('Cryptographic Forum Research Group').toString('hex'),
    expected: 'a8061dc1305136c6c22b8baf0c0127a9',
  },
  {
    label: 'SipHash-2-4',
    algorithm: 'siphash',
    options: { outputLength: 8 },
    key: '000102030405060708090a0b0c0d0e0f',
    data: '',
    expected: '310e0edd47db6f72',
  },
  {
    label: 'BLAKE2b MAC',
    algorithm: 'blake2bmac',
    options: {
      outputLength: 32,
      salt: new DataView(blake2bSaltStorage.buffer, 1, 16),
    },
    key: '000102030405060708090a0b0c0d0e0f',
    data: Buffer.from('abc').toString('hex'),
    expected: '6e583b101a126f2d1fb6d1fff9834f3a' +
              '0d0e23c17b902cca4f1a0d7abfb327fa',
  },
  {
    label: 'BLAKE2s MAC',
    algorithm: 'blake2smac',
    options: {
      outputLength: 16,
      salt: new DataView(blake2sSaltStorage.buffer, 1, 8),
    },
    key: '000102030405060708090a0b0c0d0e0f',
    data: Buffer.from('abc').toString('hex'),
    expected: '18adff242af55a56c7b7646df6c3d9ba',
  },
];

for (const index of [0, 3]) {
  const vector = kmacVectors[index];
  const algorithm = vector.algorithm.toLowerCase();
  const options = {
    outputLength: vector.outputLength / 8,
  };
  if (vector.customization !== undefined) {
    const storage = Uint8Array.from([
      0xff,
      ...vector.customization,
      0xff,
    ]);
    options.customization = new DataView(
      storage.buffer, 1, vector.customization.length);
  }
  vectors.push({
    label: vector.algorithm,
    algorithm,
    options,
    key: vector.key.toString('hex'),
    data: vector.data.toString('hex'),
    expected: vector.expected.toString('hex'),
  });
}

for (const vector of vectors) {
  if (!availableMacs.has(vector.algorithm) ||
      (vector.cipher !== undefined &&
       !availableCiphers.has(vector.cipher))) {
    common.printSkipMessage(`${vector.label} is not available`);
    continue;
  }

  const key = Buffer.from(vector.key, 'hex');
  const data = Buffer.from(vector.data, 'hex');
  const expected = Buffer.from(vector.expected, 'hex');
  assert.deepStrictEqual(
    createMac(vector.algorithm, key, vector.options).update(data).final(),
    expected,
  );
}

if (availableMacs.has('kmac128')) {
  const vector = kmacVectors[0];
  const algorithm = 'kmac128';
  const options = { outputLength: 0 };
  for (const outputEncoding of Object.keys(encodingsMap)) {
    if (outputEncoding === 'buffer') continue;
    assert.strictEqual(
      createMac(algorithm, vector.key, options)
        .update(vector.data)
        .final(outputEncoding),
      '',
    );
  }

  for (const result of [
    createMac(algorithm, vector.key, options)
      .update(vector.data)
      .final(),
    createMac(algorithm, vector.key, options)
      .update(vector.data)
      .final('buffer'),
  ]) {
    assert(Buffer.isBuffer(result));
    assert.deepStrictEqual(result, Buffer.alloc(0));
  }

  const streamed = createMac(algorithm, vector.key, options);
  streamed.on('data', common.mustNotCall());
  streamed.on('end', common.mustCall());
  streamed.end(vector.data);
}
