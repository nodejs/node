'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');
const { getFips } = require('crypto');
const { subtle } = globalThis.crypto;

// This is only a partial test. The WebCrypto Web Platform Tests
// will provide much greater coverage.

async function testRSAOaep(publicKey, privateKey) {
  const buf = globalThis.crypto.getRandomValues(new Uint8Array(50));
  const label = new TextEncoder().encode('a label');
  const ciphertext = await subtle.encrypt({ name: 'RSA-OAEP', label }, publicKey, buf);
  const plaintext = await subtle.decrypt({ name: 'RSA-OAEP', label }, privateKey, ciphertext);

  assert.strictEqual(
    Buffer.from(plaintext).toString('hex'),
    Buffer.from(buf).toString('hex'));

  await assert.rejects(() => subtle.encrypt({
    name: 'RSA-OAEP',
  }, privateKey, buf), {
    name: 'InvalidAccessError',
    message: 'Unable to use this key to encrypt'
  });

  await assert.rejects(() => subtle.decrypt({
    name: 'RSA-OAEP',
  }, publicKey, ciphertext), {
    name: 'InvalidAccessError',
    message: 'Unable to use this key to decrypt'
  });
}

(async function() {
  const { publicKey, privateKey } = await subtle.generateKey({
    name: 'RSA-OAEP',
    modulusLength: 2048,
    publicExponent: new Uint8Array([1, 0, 1]),
    hash: 'SHA-384',
  }, true, ['encrypt', 'decrypt']);

  await testRSAOaep(publicKey, privateKey);

  if (!isBoringSSL) {
    // Import the same key material with SHA-3 to avoid another RSA keygen.
    const [spki, pkcs8] = await Promise.all([
      subtle.exportKey('spki', publicKey),
      subtle.exportKey('pkcs8', privateKey),
    ]);
    const algorithm = { name: 'RSA-OAEP', hash: 'SHA3-384' };
    const [sha3PublicKey, sha3PrivateKey] = await Promise.all([
      subtle.importKey('spki', spki, algorithm, false, ['encrypt']),
      subtle.importKey('pkcs8', pkcs8, algorithm, false, ['decrypt']),
    ]);
    await testRSAOaep(sha3PublicKey, sha3PrivateKey);
  }
})().then(common.mustCall());

// Test Encrypt/Decrypt AES-CTR
{
  const buf = globalThis.crypto.getRandomValues(new Uint8Array(50));
  const counter = globalThis.crypto.getRandomValues(new Uint8Array(16));

  async function test() {
    const key = await subtle.generateKey({
      name: 'AES-CTR',
      length: 256
    }, true, ['encrypt', 'decrypt']);

    const ciphertext = await subtle.encrypt(
      { name: 'AES-CTR', counter, length: 64 }, key, buf,
    );

    const plaintext = await subtle.decrypt(
      { name: 'AES-CTR', counter, length: 64 }, key, ciphertext,
    );

    assert.strictEqual(
      Buffer.from(plaintext).toString('hex'),
      Buffer.from(buf).toString('hex'));
  }

  test().then(common.mustCall());
}

// Test Encrypt/Decrypt AES-CBC
{
  const buf = globalThis.crypto.getRandomValues(new Uint8Array(50));
  const iv = globalThis.crypto.getRandomValues(new Uint8Array(16));

  async function test() {
    const key = await subtle.generateKey({
      name: 'AES-CBC',
      length: 256
    }, true, ['encrypt', 'decrypt']);

    const ciphertext = await subtle.encrypt(
      { name: 'AES-CBC', iv }, key, buf,
    );

    const plaintext = await subtle.decrypt(
      { name: 'AES-CBC', iv }, key, ciphertext,
    );

    assert.strictEqual(
      Buffer.from(plaintext).toString('hex'),
      Buffer.from(buf).toString('hex'));
  }

  test().then(common.mustCall());
}

// Test Encrypt/Decrypt AES-GCM
{
  const buf = globalThis.crypto.getRandomValues(new Uint8Array(50));
  const iv = globalThis.crypto.getRandomValues(new Uint8Array(12));

  async function test() {
    const key = await subtle.generateKey({
      name: 'AES-GCM',
      length: 256
    }, true, ['encrypt', 'decrypt']);

    const ciphertext = await subtle.encrypt(
      { name: 'AES-GCM', iv }, key, buf,
    );

    const plaintext = await subtle.decrypt(
      { name: 'AES-GCM', iv }, key, ciphertext,
    );

    assert.strictEqual(
      Buffer.from(plaintext).toString('hex'),
      Buffer.from(buf).toString('hex'));
  }

  test().then(common.mustCall());
}

// Test Encrypt/Decrypt AES-OCB
if (hasOpenSSL(3)) {
  const buf = globalThis.crypto.getRandomValues(new Uint8Array(50));
  const iv = globalThis.crypto.getRandomValues(new Uint8Array(12));

  async function test() {
    const key = await subtle.generateKey({
      name: 'AES-OCB',
      length: 256
    }, true, ['encrypt', 'decrypt']);

    const ciphertext = await subtle.encrypt(
      { name: 'AES-OCB', iv }, key, buf,
    );

    const plaintext = await subtle.decrypt(
      { name: 'AES-OCB', iv }, key, ciphertext,
    );

    assert.strictEqual(
      Buffer.from(plaintext).toString('hex'),
      Buffer.from(buf).toString('hex'));
  }

  if (getFips() === 1) {
    assert.rejects(
      test(),
      { name: 'NotSupportedError', message: 'Unrecognized algorithm name' })
      .then(common.mustCall());
  } else {
    test().then(common.mustCall());
  }
} else {
  common.printSkipMessage('Skipping unsupported AES-OCB test cases');
}
