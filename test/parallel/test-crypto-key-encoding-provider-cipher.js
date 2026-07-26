'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasFIPS, isBoringSSL } = require('../common/crypto');
if (isBoringSSL)
  common.skip('OpenSSL providers are required');

const assert = require('assert');
const {
  createPrivateKey,
  createPublicKey,
  generateKeyPair,
  generateKeyPairSync,
  getCiphers,
  sign,
  verify,
} = require('crypto');

// Resolve an AES-256-CBC provider alias and retain the fetched cipher while
// encoding private keys and running asynchronous key generation jobs.
const cipher = '2.16.840.1.101.3.4.1.42';
const passphrase = 'provider cipher passphrase';
const keyOptions = { namedCurve: 'prime256v1' };
const { publicKey, privateKey } = generateKeyPairSync('ec', keyOptions);
const data = Buffer.from('encrypted private key');

function checkPrivateKey(encoded, format, type, expectedPublicKey) {
  const decrypted = createPrivateKey({
    key: encoded,
    format,
    type,
    passphrase,
  });
  assert(createPublicKey(decrypted).equals(expectedPublicKey));
  const signature = sign('sha256', data, decrypted);
  assert(verify('sha256', data, expectedPublicKey, signature));
}

for (const format of ['pem', 'der']) {
  const privateKeyEncoding = {
    format,
    type: 'pkcs8',
    cipher,
    passphrase,
  };

  const exported = privateKey.export(privateKeyEncoding);
  if (format === 'pem')
    assert.match(exported, /^-----BEGIN ENCRYPTED PRIVATE KEY-----/);
  checkPrivateKey(exported, format, 'pkcs8', publicKey);

  const generated = generateKeyPairSync('ec', {
    ...keyOptions,
    privateKeyEncoding,
  });
  checkPrivateKey(generated.privateKey, format, 'pkcs8', generated.publicKey);

  // Async jobs copy the encoding configuration. Its fetched cipher must remain
  // valid after the configuration used to create the job has been destroyed.
  generateKeyPair('ec', {
    ...keyOptions,
    privateKeyEncoding,
  }, common.mustSucceed((generatedPublicKey, generatedPrivateKey) => {
    checkPrivateKey(generatedPrivateKey, format, 'pkcs8', generatedPublicKey);
  }));
}

// Provider lookup must preserve the restrictions of each key format.
const sec1 = { format: 'pem', type: 'sec1', cipher, passphrase };
// Traditional PEM encryption uses MD5 to derive its key.
if (!hasFIPS())
  checkPrivateKey(privateKey.export(sec1), 'pem', 'sec1', publicKey);
assert.throws(() => privateKey.export({ ...sec1, format: 'der' }), {
  code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS',
});

const unknownCipher = {
  format: 'pem',
  type: 'pkcs8',
  cipher: 'unknown-private-key-cipher',
  passphrase,
};
const unknownCipherError = {
  code: 'ERR_CRYPTO_UNKNOWN_CIPHER',
  message: 'Unknown cipher',
};
assert.throws(() => privateKey.export(unknownCipher), unknownCipherError);
assert.throws(() => generateKeyPairSync('ec', {
  ...keyOptions,
  privateKeyEncoding: unknownCipher,
}), unknownCipherError);
assert.throws(() => generateKeyPair('ec', {
  ...keyOptions,
  privateKeyEncoding: unknownCipher,
}, common.mustNotCall()), unknownCipherError);

// Provider-only ciphers must reach the serializer, which still rejects ciphers
// without an ASN.1 identifier when writing PKCS8.
if (getCiphers().includes('aes-128-cbc-cts')) {
  for (const format of ['pem', 'der']) {
    assert.throws(() => privateKey.export({
      format,
      type: 'pkcs8',
      cipher: 'aes-128-cbc-cts',
      passphrase,
    }), {
      code: 'ERR_OSSL_ASN1_CIPHER_HAS_NO_OBJECT_IDENTIFIER',
    });
  }
}
