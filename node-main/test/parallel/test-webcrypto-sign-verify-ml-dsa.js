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
                            context,
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

  assert(await subtle.verify({ name, context }, publicKey, signature, data));
  assert(!(await subtle.verify({ name, context: crypto.randomBytes(30) }, publicKey, signature, data)));
  if (context.byteLength === 0) {
    assert(await subtle.verify({ name }, publicKey, signature, data));
  }

  // Test verification with altered buffers
  const copy = Buffer.from(data);
  const sigcopy = Buffer.from(signature);
  const p = subtle.verify({ name, context }, publicKey, sigcopy, copy);
  copy[0] = 255 - copy[0];
  sigcopy[0] = 255 - sigcopy[0];
  assert(await p);

  // Test failure when using wrong key
  await assert.rejects(
    subtle.verify({ name, context }, privateKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  await assert.rejects(
    subtle.verify({ name, context }, noVerifyPublicKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  // Test failure when using the wrong algorithms
  await assert.rejects(
    subtle.verify({ name, context }, hmacKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  await assert.rejects(
    subtle.verify({ name, context }, rsaKeys.publicKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  await assert.rejects(
    subtle.verify({ name, context }, ecKeys.publicKey, signature, data), {
      message: /Unable to use this key to verify/
    });

  // Test failure when too long context
  await assert.rejects(
    subtle.verify({ name, context: new Uint8Array(256) }, publicKey, signature, data), (err) => {
      assert.strictEqual(err.name, 'OperationError');
      assert.strictEqual(err.cause.code, 'ERR_OUT_OF_RANGE');
      assert.strictEqual(err.cause.message, 'context string must be at most 255 bytes');
      return true;
    });

  // Test failure when signature is altered
  {
    const copy = Buffer.from(signature);
    copy[0] = 255 - copy[0];
    assert(!(await subtle.verify(
      { name, context },
      publicKey,
      copy,
      data)));
    assert(!(await subtle.verify(
      { name, context },
      publicKey,
      copy.slice(1),
      data)));
  }

  // Test failure when data is altered
  {
    const copy = Buffer.from(data);
    copy[0] = 255 - copy[0];
    assert(!(await subtle.verify({ name, context }, publicKey, signature, copy)));
  }
}

async function testSign({ name,
                          context,
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
    const sig = await subtle.sign({ name, context }, privateKey, data);
    assert.strictEqual(sig.byteLength, signature.byteLength);
    assert(await subtle.verify({ name, context }, publicKey, sig, data));
  }

  {
    const copy = Buffer.from(data);
    const p = subtle.sign({ name, context }, privateKey, copy);
    copy[0] = 255 - copy[0];
    const sig = await p;
    assert(await subtle.verify({ name, context }, publicKey, sig, data));
  }

  // Test failure when using wrong key
  await assert.rejects(
    subtle.sign({ name, context }, publicKey, data), {
      message: /Unable to use this key to sign/
    });

  // Test failure when using the wrong algorithms
  await assert.rejects(
    subtle.sign({ name, context }, hmacKey, data), {
      message: /Unable to use this key to sign/
    });

  await assert.rejects(
    subtle.sign({ name, context }, rsaKeys.privateKey, data), {
      message: /Unable to use this key to sign/
    });

  await assert.rejects(
    subtle.sign({ name, context }, ecKeys.privateKey, data), {
      message: /Unable to use this key to sign/
    });

  // Test failure when too long context
  await assert.rejects(
    subtle.sign({ name, context: new Uint8Array(256) }, privateKey, data), (err) => {
      assert.strictEqual(err.name, 'OperationError');
      assert.strictEqual(err.cause.code, 'ERR_OUT_OF_RANGE');
      assert.strictEqual(err.cause.message, 'context string must be at most 255 bytes');
      return true;
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
