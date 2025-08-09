'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 5))
  common.skip('requires OpenSSL >= 3.5');

const assert = require('assert');
const crypto = require('crypto');
const { subtle } = globalThis.crypto;

const vectors = require('../fixtures/crypto/ml-dsa')();

async function testVerify({ name,
                            publicKeyPem,
                            privateKeyPem,
                            signature,
                            data }) {
  const [
    publicKey,
    noVerifyPublicKey,
    privateKey,
    hmacKey,
    rsaKeys,
    ecKeys,
  ] = await Promise.all([
    crypto.createPublicKey(publicKeyPem)
      .toCryptoKey(name, false, ['verify']),
    crypto.createPublicKey(publicKeyPem)
      .toCryptoKey(name, false, [ /* No usages */ ]),
    crypto.createPrivateKey(privateKeyPem)
      .toCryptoKey(name, false, ['sign']),
    subtle.generateKey(
      { name: 'HMAC', hash: 'SHA-256' },
      false,
      ['sign']),
    subtle.generateKey(
      {
        name: 'RSA-PSS',
        modulusLength: 1024,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-256',
      },
      false,
      ['sign']),
    subtle.generateKey(
      {
        name: 'ECDSA',
        namedCurve: 'P-256'
      },
      false,
      ['sign']),
  ]);

  assert(await subtle.verify({ name }, publicKey, signature, data));

  // Test verification with altered buffers
  const copy = Buffer.from(data);
  const sigcopy = Buffer.from(signature);
  const p = subtle.verify({ name }, publicKey, sigcopy, copy);
  copy[0] = 255 - copy[0];
  sigcopy[0] = 255 - sigcopy[0];
  assert(await p);

  // Test failure when using wrong key
  await assert.rejects(
    subtle.verify({ name }, privateKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  await assert.rejects(
    subtle.verify({ name }, noVerifyPublicKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  // Test failure when using the wrong algorithms
  await assert.rejects(
    subtle.verify({ name }, hmacKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  await assert.rejects(
    subtle.verify({ name }, rsaKeys.publicKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  await assert.rejects(
    subtle.verify({ name }, ecKeys.publicKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  // Test failure when signature is altered
  {
    const copy = Buffer.from(signature);
    copy[0] = 255 - copy[0];
    assert(!(await subtle.verify(
      { name },
      publicKey,
      copy,
      data)));
    assert(!(await subtle.verify(
      { name },
      publicKey,
      copy.slice(1),
      data)));
  }

  // Test failure when data is altered
  {
    const copy = Buffer.from(data);
    copy[0] = 255 - copy[0];
    assert(!(await subtle.verify({ name }, publicKey, signature, copy)));
  }
}

async function testSign({ name,
                          publicKeyPem,
                          privateKeyPem,
                          signature,
                          data }) {
  const [
    publicKey,
    privateKey,
    hmacKey,
    rsaKeys,
    ecKeys,
  ] = await Promise.all([
    crypto.createPublicKey(publicKeyPem)
      .toCryptoKey(name, false, ['verify']),
    crypto.createPrivateKey(privateKeyPem)
      .toCryptoKey(name, false, ['sign']),
    subtle.generateKey(
      { name: 'HMAC', hash: 'SHA-256' },
      false,
      ['sign']),
    subtle.generateKey(
      {
        name: 'RSA-PSS',
        modulusLength: 1024,
        publicExponent: new Uint8Array([1, 0, 1]),
        hash: 'SHA-256',
      },
      false,
      ['sign']),
    subtle.generateKey(
      {
        name: 'ECDSA',
        namedCurve: 'P-256'
      },
      false,
      ['sign']),
  ]);

  {
    const sig = await subtle.sign({ name }, privateKey, data);
    assert.strictEqual(sig.byteLength, signature.byteLength);
    assert(await subtle.verify({ name }, publicKey, sig, data));
  }

  {
    const copy = Buffer.from(data);
    const p = subtle.sign({ name }, privateKey, copy);
    copy[0] = 255 - copy[0];
    const sig = await p;
    assert(await subtle.verify({ name }, publicKey, sig, data));
  }

  // Test failure when using wrong key
  await assert.rejects(
    subtle.sign({ name }, publicKey, data), {
      message: /Unable to use this key to sign/
    });

  // Test failure when using the wrong algorithms
  await assert.rejects(
    subtle.sign({ name }, hmacKey, data), {
      message: /Unable to use this key to sign/
    });

  await assert.rejects(
    subtle.sign({ name }, rsaKeys.privateKey, data), {
      message: /Unable to use this key to sign/
    });

  await assert.rejects(
    subtle.sign({ name }, ecKeys.privateKey, data), {
      message: /Unable to use this key to sign/
    });
}

(async function() {
  const variations = [];

  vectors.forEach((vector) => {
    variations.push(testVerify(vector));
    variations.push(testSign(vector));
  });

  await Promise.all(variations);
})().then(common.mustCall());

// ContextParams context not supported
{
  const vector = vectors[0];
  const name = vector.name;
  const publicKey = crypto.createPublicKey(vector.publicKeyPem)
      .toCryptoKey(vector.name, false, ['verify']);
  const privateKey = crypto.createPrivateKey(vector.privateKeyPem)
        .toCryptoKey(vector.name, false, ['sign']);

  (async () => {
    const sig = await subtle.sign({ name, context: Buffer.alloc(0) }, privateKey, vector.data);
    assert.strictEqual(
      await subtle.verify({ name, context: Buffer.alloc(0) }, publicKey, sig, vector.data), true);

    await assert.rejects(subtle.sign({ name, context: Buffer.alloc(1) }, privateKey, vector.data), {
      message: /Non zero-length ContextParams\.context is not supported/
    });
    await assert.rejects(subtle.verify({ name, context: Buffer.alloc(1) }, publicKey, sig, vector.data), {
      message: /Non zero-length ContextParams\.context is not supported/
    });
  })().then(common.mustCall());
}
