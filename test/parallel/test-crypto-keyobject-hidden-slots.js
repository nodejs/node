'use strict';

// KeyObject public getters and methods are configurable JS properties.
// Internal consumers must read key state through hidden native-backed
// slots, not through user-replaceable accessors.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const {
  createCipheriv,
  createDecipheriv,
  createHmac,
  createPrivateKey,
  createPublicKey,
  createSecretKey,
  createSign,
  createVerify,
  diffieHellman,
  generateKeyPairSync,
  hkdfSync,
  KeyObject,
  privateDecrypt,
  publicEncrypt,
  sign,
  verify,
  X509Certificate,
} = require('node:crypto');
const { readFileSync } = require('node:fs');
const fixtures = require('../common/fixtures');

function updateFinal(cipher, data = Buffer.alloc(16)) {
  return Buffer.concat([cipher.update(data), cipher.final()]);
}

{
  const secret = createSecretKey(Buffer.alloc(16));
  const secretProto = Object.getPrototypeOf(secret);
  const originalType =
    Object.getOwnPropertyDescriptor(KeyObject.prototype, 'type');
  const originalSize =
    Object.getOwnPropertyDescriptor(secretProto, 'symmetricKeySize');

  Object.defineProperty(KeyObject.prototype, 'type', {
    configurable: true,
    get() { return 'public'; },
  });
  Object.defineProperty(secretProto, 'symmetricKeySize', {
    configurable: true,
    get() { return 1; },
  });

  try {
    assert.strictEqual(secret.type, 'public');
    assert.strictEqual(secret.symmetricKeySize, 1);

    assert.strictEqual(
      createHmac('sha256', secret).update('payload').digest('hex').length,
      64);

    const ciphertext = updateFinal(
      createCipheriv('aes-128-ecb', secret, null));
    const plaintext = updateFinal(
      createDecipheriv('aes-128-ecb', secret, null), ciphertext);
    assert.deepStrictEqual(plaintext, Buffer.alloc(16));

    assert.strictEqual(
      hkdfSync('sha256', secret, Buffer.alloc(0), Buffer.alloc(0), 16)
        .byteLength,
      16);

    const cryptoKey = secret.toCryptoKey(
      { name: 'AES-GCM' }, true, ['encrypt']);
    assert.strictEqual(cryptoKey.algorithm.length, 128);
  } finally {
    Object.defineProperty(KeyObject.prototype, 'type', originalType);
    Object.defineProperty(secretProto, 'symmetricKeySize', originalSize);
  }
}

{
  const {
    privateKey: ecPrivateKey,
    publicKey,
  } = generateKeyPairSync('ec', { namedCurve: 'P-256' });
  const asymmetricProto = Object.getPrototypeOf(Object.getPrototypeOf(publicKey));
  const originalAsymmetricKeyType =
    Object.getOwnPropertyDescriptor(asymmetricProto, 'asymmetricKeyType');

  Object.defineProperty(asymmetricProto, 'asymmetricKeyType', {
    configurable: true,
    get() { return 'rsa'; },
  });

  try {
    assert.strictEqual(publicKey.asymmetricKeyType, 'rsa');
    assert.strictEqual(
      publicKey.export({ format: 'raw-public', type: 'compressed' }).length,
      33);

    assert.strictEqual(
      diffieHellman({ privateKey: ecPrivateKey, publicKey }).byteLength,
      32);
  } finally {
    Object.defineProperty(
      asymmetricProto, 'asymmetricKeyType', originalAsymmetricKeyType);
  }
}

{
  const { publicKey } = generateKeyPairSync('rsa', {
    modulusLength: 1024,
  });

  const details = publicKey.asymmetricKeyDetails;
  assert.strictEqual(details.modulusLength, 1024);
  assert.strictEqual(details.publicExponent, 65537n);

  details.modulusLength = 1;
  details.publicExponent = 3n;
  details.extra = true;

  const freshDetails = publicKey.asymmetricKeyDetails;
  assert.notStrictEqual(freshDetails, details);
  assert.strictEqual(freshDetails.modulusLength, 1024);
  assert.strictEqual(freshDetails.publicExponent, 65537n);
  assert.strictEqual(freshDetails.extra, undefined);
}

{
  Object.defineProperty(Object.prototype, 'publicExponent', {
    configurable: true,
    value: new Uint8Array([1, 0, 1]),
  });

  try {
    const { publicKey } = generateKeyPairSync('ec', { namedCurve: 'P-256' });
    assert.deepStrictEqual(publicKey.asymmetricKeyDetails, {
      namedCurve: 'prime256v1',
    });
    assert.strictEqual(
      Object.hasOwn(publicKey.asymmetricKeyDetails, 'publicExponent'),
      false);
  } finally {
    delete Object.prototype.publicExponent;
  }
}

{
  const { privateKey, publicKey } = generateKeyPairSync('rsa', {
    modulusLength: 1024,
  });
  const originalType =
    Object.getOwnPropertyDescriptor(KeyObject.prototype, 'type');
  const data = Buffer.from('payload');

  Object.defineProperty(KeyObject.prototype, 'type', {
    configurable: true,
    get() { return 'secret'; },
  });

  try {
    assert.strictEqual(privateKey.type, 'secret');
    assert.strictEqual(publicKey.type, 'secret');

    const signature = sign('sha256', data, privateKey);
    assert.strictEqual(verify('sha256', data, publicKey, signature), true);

    const signer = createSign('sha256');
    signer.update(data);
    const streamSignature = signer.sign(privateKey);
    const verifier = createVerify('sha256');
    verifier.update(data);
    assert.strictEqual(verifier.verify(publicKey, streamSignature), true);

    const ciphertext = publicEncrypt(publicKey, data);
    assert.deepStrictEqual(privateDecrypt(privateKey, ciphertext), data);

    assert.strictEqual(publicKey.equals(createPublicKey(privateKey)), true);

    const x509 = new X509Certificate(
      readFileSync(fixtures.path('keys', 'agent1-cert.pem')));
    const x509PrivateKey = createPrivateKey(
      readFileSync(fixtures.path('keys', 'agent1-key.pem')));
    const ca = new X509Certificate(
      readFileSync(fixtures.path('keys', 'ca1-cert.pem')));

    assert.strictEqual(x509.checkPrivateKey(x509PrivateKey), true);
    assert.strictEqual(x509.verify(ca.publicKey), true);
  } finally {
    Object.defineProperty(KeyObject.prototype, 'type', originalType);
  }
}

{
  const a = createSecretKey(Buffer.alloc(16));
  const b = createSecretKey(Buffer.alloc(16, 1));
  const originalEquals =
    Object.getOwnPropertyDescriptor(KeyObject.prototype, 'equals');

  Object.defineProperty(KeyObject.prototype, 'equals', {
    configurable: true,
    value: () => true,
  });

  try {
    assert.notDeepStrictEqual(a, b);
  } finally {
    Object.defineProperty(KeyObject.prototype, 'equals', originalEquals);
  }
}
