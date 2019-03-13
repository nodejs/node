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
  // Passing an existing public key object to createPublicKey should throw.
  const publicKey = createPublicKey(publicPem);
  common.expectsError(() => createPublicKey(publicKey), {
    type: TypeError,
    code: 'ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE',
    message: 'Invalid key object type public, expected private.'
  });

  // Constructing a private key from a public key should be impossible, even
  // if the public key was derived from a private key.
  common.expectsError(() => createPrivateKey(createPublicKey(privatePem)), {
    type: TypeError,
    code: 'ERR_INVALID_ARG_TYPE',
    message: 'The "key" argument must be one of type string, Buffer, ' +
             'TypedArray, or DataView. Received type object'
  });

  // Similarly, passing an existing private key object to createPrivateKey
  // should throw.
  const privateKey = createPrivateKey(privatePem);
  common.expectsError(() => createPrivateKey(privateKey), {
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

  // It should be possible to derive a public key from a private key.
  const derivedPublicKey = createPublicKey(privateKey);
  assert.strictEqual(derivedPublicKey.type, 'public');
  assert.strictEqual(derivedPublicKey.asymmetricKeyType, 'rsa');
  assert.strictEqual(derivedPublicKey.symmetricKeySize, undefined);

  // Test exporting with an invalid options object, this should throw.
  for (const opt of [undefined, null, 'foo', 0, NaN]) {
    common.expectsError(() => publicKey.export(opt), {
      type: TypeError,
      code: 'ERR_INVALID_ARG_TYPE',
      message: 'The "options" argument must be of type object. Received type ' +
               typeof opt
    });
  }

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
    // Encrypt using the public key.
    publicEncrypt(publicKey, plaintext),
    publicEncrypt({ key: publicKey }, plaintext),

    // Encrypt using the private key.
    publicEncrypt(privateKey, plaintext),
    publicEncrypt({ key: privateKey }, plaintext),

    // Encrypt using a public key derived from the private key.
    publicEncrypt(derivedPublicKey, plaintext),
    publicEncrypt({ key: derivedPublicKey }, plaintext),

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

[
  { private: fixtures.readSync('test_ed25519_privkey.pem', 'ascii'),
    public: fixtures.readSync('test_ed25519_pubkey.pem', 'ascii'),
    keyType: 'ed25519' },
  { private: fixtures.readSync('test_ed448_privkey.pem', 'ascii'),
    public: fixtures.readSync('test_ed448_pubkey.pem', 'ascii'),
    keyType: 'ed448' }
].forEach((info) => {
  const keyType = info.keyType;

  {
    const exportOptions = { type: 'pkcs8', format: 'pem' };
    const key = createPrivateKey(info.private);
    assert.strictEqual(key.type, 'private');
    assert.strictEqual(key.asymmetricKeyType, keyType);
    assert.strictEqual(key.symmetricKeySize, undefined);
    assert.strictEqual(key.export(exportOptions), info.private);
  }

  {
    const exportOptions = { type: 'spki', format: 'pem' };
    [info.private, info.public].forEach((pem) => {
      const key = createPublicKey(pem);
      assert.strictEqual(key.type, 'public');
      assert.strictEqual(key.asymmetricKeyType, keyType);
      assert.strictEqual(key.symmetricKeySize, undefined);
      assert.strictEqual(key.export(exportOptions), info.public);
    });
  }
});
