// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasFIPS } = require('../common/crypto');
const { subtle } = globalThis.crypto;
const requiresLongHkdfInputs = hasFIPS(3) && !hasFIPS(3, 5);
const rejectsWeakPbkdf2Inputs = hasFIPS(4);
const rejectsXCurves = hasFIPS(3, 5);

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
    [requiresLongHkdfInputs ? 'hello hello hello' : 'hello',
     'there', requiresLongHkdfInputs ? 'my friend indeed' : 'my friend',
     'SHA-256', 512,
     requiresLongHkdfInputs ?
       'bc2b7841512a6f4563f723c317909ac305ddbfbdec1daf0055d0587b5db8d635' +
       'a22f97b0dfbcc12dcd2d096123385227b16e95e5bccc0d6751491f38c5e48428' :
       '14d93b0ccd99d4f2cbd9fbfe9c830b5b8a43e3e45e329' +
       '41ef21bdeb0fa87b6b6bfa5c54466aa5bf76cdc2685fb' +
       'a4408ea5b94c049fe035649b46f92fdc519374'],
    [requiresLongHkdfInputs ? 'hello hello hello' : 'hello',
     'there', requiresLongHkdfInputs ? 'my friend indeed' : 'my friend',
     'SHA-384', 128,
     requiresLongHkdfInputs ? 'ee2d1d7dc759c26f2ab8ee6d7cfa0c23' :
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
    ['password', 'there there here', 1000, 'SHA-256', 512,
     '8802c34ee684a523f9304a6335394c0a5f02350d51383d' +
     '17d3cf89fa0808591ddede3c832fe4691c7f361ade53b9' +
     '36bf94347055bcf86fd662abe038fb945d17'],
    ['password', 'there there here', 2000, 'SHA-384', 128,
     '7c650b88798cea1a390802a6f97e05b0'],
  ];

  const tests = Promise.all(kTests.map((args) => test(...args)));

  tests.then(common.mustCall());

  if (rejectsWeakPbkdf2Inputs) {
    assert.rejects(
      test('hello', 'there', 10, 'SHA-256', 512),
      { name: 'OperationError' })
      .then(common.mustCall());
  }
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

  if (rejectsXCurves) {
    for (const name of ['X25519', 'X448']) {
      assert.rejects(
        test(name),
        (err) => err.name === 'OperationError' &&
                 err.cause?.code === 'ERR_OSSL_EVP_UNSUPPORTED')
        .then(common.mustCall());
    }
  } else {
    test('X25519').then(common.mustCall());
    if (!process.features.openssl_is_boringssl) {
      test('X448').then(common.mustCall());
    } else {
      common.printSkipMessage('Skipping unsupported X448 test case');
    }
  }
}
