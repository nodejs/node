// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

// This is only a partial test. The WebCrypto Web Platform Tests
// will provide much greater coverage.

// Test ECDH bit derivation
{
  async function test(namedCurve) {
    const [alice, bob] = await Promise.all([
      subtle.generateKey({ name: 'ECDH', namedCurve }, true, ['deriveBits']),
      subtle.generateKey({ name: 'ECDH', namedCurve }, true, ['deriveBits']),
    ]);

    const [secret1, secret2] = await Promise.all([
      subtle.deriveBits({
        name: 'ECDH', namedCurve, public: alice.publicKey
      }, bob.privateKey, 128),
      subtle.deriveBits({
        name: 'ECDH', namedCurve, public: bob.publicKey
      }, alice.privateKey, 128),
    ]);

    assert(secret1 instanceof ArrayBuffer);
    assert(secret2 instanceof ArrayBuffer);
    assert.deepStrictEqual(secret1, secret2);
  }

  test('P-521').then(common.mustCall());
}

// Test HKDF bit derivation
{
  async function test(pass, info, salt, hash, length, expected) {
    const ec = new TextEncoder();
    const key = await subtle.importKey(
      'raw',
      ec.encode(pass),
      { name: 'HKDF', hash },
      false, ['deriveBits']);
    const secret = await subtle.deriveBits({
      name: 'HKDF',
      hash,
      salt: ec.encode(salt),
      info: ec.encode(info)
    }, key, length);
    assert.strictEqual(Buffer.from(secret).toString('hex'), expected);
  }

  const kTests = [
    ['hello', 'there', 'my friend', 'SHA-256', 512,
     '14d93b0ccd99d4f2cbd9fbfe9c830b5b8a43e3e45e329' +
     '41ef21bdeb0fa87b6b6bfa5c54466aa5bf76cdc2685fb' +
     'a4408ea5b94c049fe035649b46f92fdc519374'],
    ['hello', 'there', 'my friend', 'SHA-384', 128,
     'e36cf2cf943d8f3a88adb80f478745c3'],
  ];

  const tests = Promise.all(kTests.map((args) => test(...args)));

  tests.then(common.mustCall());
}

// Test PBKDF2 bit derivation
{
  async function test(pass, salt, iterations, hash, length, expected) {
    const ec = new TextEncoder();
    const key = await subtle.importKey(
      'raw',
      ec.encode(pass),
      { name: 'PBKDF2', hash },
      false, ['deriveBits']);
    const secret = await subtle.deriveBits({
      name: 'PBKDF2',
      hash,
      salt: ec.encode(salt),
      iterations,
    }, key, length);
    assert.strictEqual(Buffer.from(secret).toString('hex'), expected);
  }

  const kTests = [
    ['hello', 'there', 10, 'SHA-256', 512,
     'f72d1cf4853fffbd16a42751765d11f8dc7939498ee7b7' +
     'ce7678b4cb16fad88098110a83e71f4483ce73203f7a64' +
     '719d293280f780f9fafdcf46925c5c0588b3'],
    ['hello', 'there', 5, 'SHA-384', 128,
     '201509b012c9cd2fbe7ea938f0c509b3'],
  ];

  const tests = Promise.all(kTests.map((args) => test(...args)));

  tests.then(common.mustCall());
}

// Test X25519 and X448 bit derivation
{
  async function test(name) {
    const [alice, bob] = await Promise.all([
      subtle.generateKey({ name }, true, ['deriveBits']),
      subtle.generateKey({ name }, true, ['deriveBits']),
    ]);

    const [secret1, secret2] = await Promise.all([
      subtle.deriveBits({
        name, public: alice.publicKey
      }, bob.privateKey, 128),
      subtle.deriveBits({
        name, public: bob.publicKey
      }, alice.privateKey, 128),
    ]);

    assert(secret1 instanceof ArrayBuffer);
    assert(secret2 instanceof ArrayBuffer);
    assert.deepStrictEqual(secret1, secret2);
  }

  if (!process.features.openssl_is_boringssl) {
    test('X25519').then(common.mustCall());
    test('X448').then(common.mustCall());
  }
}
