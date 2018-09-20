'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  createCipheriv,
  createDecipheriv,
  createSecretKey,
  createPublicKey,
  createPrivateKey,
  randomBytes,
  publicEncrypt,
  privateDecrypt
} = require('crypto');

const fixtures = require('../common/fixtures');

const publicPem = fixtures.readSync('test_rsa_pubkey.pem', 'ascii');
const privatePem = fixtures.readSync('test_rsa_privkey.pem', 'ascii');

{
  // Attempting to create an empty key should throw.
  common.expectsError(() => {
    createSecretKey(Buffer.alloc(0));
  }, {
    type: RangeError,
    code: 'ERR_OUT_OF_RANGE',
    message: 'The value of "key.byteLength" is out of range. ' +
             'It must be > 0. Received 0'
  });
}

{
  const keybuf = randomBytes(32);
  const key = createSecretKey(keybuf);
  assert.strictEqual(key.getType(), 'secret');
  assert.strictEqual(key.getSymmetricKeySize(), 32);

  const exportedKey = key.export();
  assert(keybuf.equals(exportedKey));

  const plaintext = Buffer.from('Hello world', 'utf8');

  const cipher = createCipheriv('aes-256-ecb', key, null);
  const ciphertext = Buffer.concat([
    cipher.update(plaintext), cipher.final()
  ]);

  const decipher = createDecipheriv('aes-256-ecb', key, null);
  const deciphered = Buffer.concat([
    decipher.update(ciphertext), decipher.final()
  ]);

  assert(plaintext.equals(deciphered));
}

{
  const publicKey = createPublicKey(publicPem);
  assert.strictEqual(publicKey.getType(), 'public');
  assert.strictEqual(publicKey.getAsymmetricKeyType(), 'rsa');

  const privateKey = createPrivateKey(privatePem);
  assert.strictEqual(privateKey.getType(), 'private');
  assert.strictEqual(privateKey.getAsymmetricKeyType(), 'rsa');

  const plaintext = Buffer.from('Hello world', 'utf8');
  const ciphertexts = [
    publicEncrypt(publicKey, plaintext),
    publicEncrypt({ key: publicKey }, plaintext)
  ];

  for (const ciphertext of ciphertexts) {
    const deciphered = privateDecrypt(privateKey, ciphertext);
    assert(plaintext.equals(deciphered));
  }
}
