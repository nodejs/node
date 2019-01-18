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
  assert.strictEqual(key.type, 'secret');
  assert.strictEqual(key.symmetricKeySize, 32);
  assert.strictEqual(key.asymmetricKeyType, undefined);

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
  // Passing an existing key object should throw.
  const publicKey = createPublicKey(publicPem);
  common.expectsError(() => createPublicKey(publicKey), {
    type: TypeError,
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "key" argument must be one of type string, Buffer, ' +
             'TypedArray, or DataView. Received type object'
  });
}

{
  const publicKey = createPublicKey(publicPem);
  assert.strictEqual(publicKey.type, 'public');
  assert.strictEqual(publicKey.asymmetricKeyType, 'rsa');
  assert.strictEqual(publicKey.symmetricKeySize, undefined);

  const privateKey = createPrivateKey(privatePem);
  assert.strictEqual(privateKey.type, 'private');
  assert.strictEqual(privateKey.asymmetricKeyType, 'rsa');
  assert.strictEqual(privateKey.symmetricKeySize, undefined);

  const publicDER = publicKey.export({
    format: 'der',
    type: 'pkcs1'
  });

  const privateDER = privateKey.export({
    format: 'der',
    type: 'pkcs1'
  });

  assert(Buffer.isBuffer(publicDER));
  assert(Buffer.isBuffer(privateDER));

  const plaintext = Buffer.from('Hello world', 'utf8');
  const ciphertexts = [
    publicEncrypt(publicKey, plaintext),
    publicEncrypt({ key: publicKey }, plaintext),
    // Test distinguishing PKCS#1 public and private keys based on the
    // DER-encoded data only.
    publicEncrypt({ format: 'der', type: 'pkcs1', key: publicDER }, plaintext),
    publicEncrypt({ format: 'der', type: 'pkcs1', key: privateDER }, plaintext)
  ];

  const decryptionKeys = [
    privateKey,
    { format: 'pem', key: privatePem },
    { format: 'der', type: 'pkcs1', key: privateDER }
  ];

  for (const ciphertext of ciphertexts) {
    for (const key of decryptionKeys) {
      const deciphered = privateDecrypt(key, ciphertext);
      assert(plaintext.equals(deciphered));
    }
  }
}

{
  // This should not cause a crash: https://github.com/nodejs/node/issues/25247
  assert.throws(() => {
    createPrivateKey({ key: '' });
  }, /null/);
}
