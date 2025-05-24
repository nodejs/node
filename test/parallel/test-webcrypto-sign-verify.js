'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

// This is only a partial test. The WebCrypto Web Platform Tests
// will provide much greater coverage.

// Test Sign/Verify RSASSA-PKCS1-v1_5
{
  async function test(data) {
    const ec = new TextEncoder();
    const { publicKey, privateKey } = await subtle.generateKey({
      name: 'RSASSA-PKCS1-v1_5',
      modulusLength: 1024,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256'
    }, true, ['sign', 'verify']);

    const signature = await subtle.sign({
      name: 'RSASSA-PKCS1-v1_5'
    }, privateKey, ec.encode(data));

    assert(await subtle.verify({
      name: 'RSASSA-PKCS1-v1_5'
    }, publicKey, signature, ec.encode(data)));
  }

  test('hello world').then(common.mustCall());
}

// Test Sign/Verify RSA-PSS
{
  async function test(data) {
    const ec = new TextEncoder();
    const { publicKey, privateKey } = await subtle.generateKey({
      name: 'RSA-PSS',
      modulusLength: 4096,
      publicExponent: new Uint8Array([1, 0, 1]),
      hash: 'SHA-256'
    }, true, ['sign', 'verify']);

    const signature = await subtle.sign({
      name: 'RSA-PSS',
      saltLength: 256,
    }, privateKey, ec.encode(data));

    assert(await subtle.verify({
      name: 'RSA-PSS',
      saltLength: 256,
    }, publicKey, signature, ec.encode(data)));
  }

  test('hello world').then(common.mustCall());
}

// Test Sign/Verify ECDSA
{
  async function test(data) {
    const ec = new TextEncoder();
    const { publicKey, privateKey } = await subtle.generateKey({
      name: 'ECDSA',
      namedCurve: 'P-384',
    }, true, ['sign', 'verify']);

    const signature = await subtle.sign({
      name: 'ECDSA',
      hash: 'SHA-384',
    }, privateKey, ec.encode(data));

    assert(await subtle.verify({
      name: 'ECDSA',
      hash: 'SHA-384',
    }, publicKey, signature, ec.encode(data)));
  }

  test('hello world').then(common.mustCall());
}

// Test Sign/Verify HMAC
{
  async function test(data) {
    const ec = new TextEncoder();

    const key = await subtle.generateKey({
      name: 'HMAC',
      length: 256,
      hash: 'SHA-256'
    }, true, ['sign', 'verify']);

    const signature = await subtle.sign({
      name: 'HMAC',
    }, key, ec.encode(data));

    assert(await subtle.verify({
      name: 'HMAC',
    }, key, signature, ec.encode(data)));
  }

  test('hello world').then(common.mustCall());
}

// Test Sign/Verify Ed25519
{
  async function test(data) {
    const ec = new TextEncoder();
    const { publicKey, privateKey } = await subtle.generateKey({
      name: 'Ed25519',
    }, true, ['sign', 'verify']);

    const signature = await subtle.sign({
      name: 'Ed25519',
    }, privateKey, ec.encode(data));

    assert(await subtle.verify({
      name: 'Ed25519',
    }, publicKey, signature, ec.encode(data)));
  }
  if (!process.features.openssl_is_boringssl) {
    test('hello world').then(common.mustCall());
  }
}

// Test Sign/Verify Ed448
{
  async function test(data) {
    const ec = new TextEncoder();
    const { publicKey, privateKey } = await subtle.generateKey({
      name: 'Ed448',
    }, true, ['sign', 'verify']);

    const signature = await subtle.sign({
      name: 'Ed448',
    }, privateKey, ec.encode(data));

    assert(await subtle.verify({
      name: 'Ed448',
    }, publicKey, signature, ec.encode(data)));
  }

  if (!process.features.openssl_is_boringssl) {
    test('hello world').then(common.mustCall());
  }
}
