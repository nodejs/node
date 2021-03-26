// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = require('crypto').webcrypto;

const { internalBinding } = require('internal/test/binding');

// This is only a partial test. The WebCrypto Web Platform Tests
// will provide much greater coverage.

// Test ECDH bit derivation
{
  async function test(namedCurve) {
    const [alice, bob] = await Promise.all([
      subtle.generateKey({ name: 'ECDH', namedCurve }, true, ['deriveKey']),
      subtle.generateKey({ name: 'ECDH', namedCurve }, true, ['deriveKey']),
    ]);

    const [secret1, secret2] = await Promise.all([
      subtle.deriveKey({
        name: 'ECDH', namedCurve, public: alice.publicKey
      }, bob.privateKey, {
        name: 'AES-CBC',
        length: 256
      }, true, ['encrypt']),
      subtle.deriveKey({
        name: 'ECDH', namedCurve, public: bob.publicKey
      }, alice.privateKey, {
        name: 'AES-CBC',
        length: 256
      }, true, ['encrypt']),
    ]);

    const [raw1, raw2] = await Promise.all([
      subtle.exportKey('raw', secret1),
      subtle.exportKey('raw', secret2),
    ]);

    assert.deepStrictEqual(raw1, raw2);
  }

  test('P-521').then(common.mustCall());
}

// Test HKDF bit derivation
{
  async function test(pass, info, salt, hash, expected) {
    const ec = new TextEncoder();
    const key = await subtle.importKey(
      'raw',
      ec.encode(pass),
      { name: 'HKDF', hash },
      false, ['deriveKey']);

    const secret = await subtle.deriveKey({
      name: 'HKDF',
      hash,
      salt: ec.encode(salt),
      info: ec.encode(info)
    }, key, {
      name: 'AES-CTR',
      length: 256
    }, true, ['encrypt']);

    const raw = await subtle.exportKey('raw', secret);

    assert.strictEqual(Buffer.from(raw).toString('hex'), expected);
  }

  const kTests = [
    ['hello', 'there', 'my friend', 'SHA-256',
     '14d93b0ccd99d4f2cbd9fbfe9c830b5b8a43e3e45e32941ef21bdeb0fa87b6b6'],
    ['hello', 'there', 'my friend', 'SHA-384',
     'e36cf2cf943d8f3a88adb80f478745c336ac811b1a86d03a7d10eb0b6b52295c'],
  ];

  const tests = Promise.all(kTests.map((args) => test(...args)));

  tests.then(common.mustCall());
}

// Test PBKDF2 bit derivation
{
  async function test(pass, salt, iterations, hash, expected) {
    const ec = new TextEncoder();
    const key = await subtle.importKey(
      'raw',
      ec.encode(pass),
      { name: 'PBKDF2', hash },
      false, ['deriveKey']);
    const secret = await subtle.deriveKey({
      name: 'PBKDF2',
      hash,
      salt: ec.encode(salt),
      iterations,
    }, key, {
      name: 'AES-CTR',
      length: 256
    }, true, ['encrypt']);

    const raw = await subtle.exportKey('raw', secret);

    assert.strictEqual(Buffer.from(raw).toString('hex'), expected);
  }

  const kTests = [
    ['hello', 'there', 10, 'SHA-256',
     'f72d1cf4853fffbd16a42751765d11f8dc7939498ee7b7ce7678b4cb16fad880'],
    ['hello', 'there', 5, 'SHA-384',
     '201509b012c9cd2fbe7ea938f0c509b36ecb140f38bf9130e96923f55f46756d'],
  ];

  const tests = Promise.all(kTests.map((args) => test(...args)));

  tests.then(common.mustCall());
}

// Test Scrypt bit derivation
if (typeof internalBinding('crypto').ScryptJob === 'function') {
  async function test(pass, salt, expected) {
    const ec = new TextEncoder();
    const key = await subtle.importKey(
      'raw',
      ec.encode(pass),
      { name: 'NODE-SCRYPT' },
      false, ['deriveKey']);
    const secret = await subtle.deriveKey({
      name: 'NODE-SCRYPT',
      salt: ec.encode(salt),
    }, key, {
      name: 'AES-CTR',
      length: 256
    }, true, ['encrypt']);

    const raw = await subtle.exportKey('raw', secret);

    assert.strictEqual(Buffer.from(raw).toString('hex'), expected);
  }

  const kTests = [
    ['hello', 'there',
     '30ddda6feabaac788eb81cc38f496cd5d9a165d320c537ea05331fe720db1061'],
  ];

  const tests = Promise.all(kTests.map((args) => test(...args)));

  tests.then(common.mustCall());
}
