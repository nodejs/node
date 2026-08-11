'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL, hasFIPS } = require('../common/crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;
const { KeyObject } = require('crypto');
const rejectsXCurves = hasFIPS(3, 5);
const fips4 = hasFIPS(4);

// This is only a partial test. The WebCrypto Web Platform Tests
// will provide much greater coverage.

// Test ECDH key derivation
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

// Test HKDF key derivation
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
    ['hello hello hello', 'there', 'my friend indeed', 'SHA-1',
     'aac1ecdc73147af6a418393da6875bff5f566c0a473e25d54b4dfc3cb7cb2ace'],
    ['hello hello hello', 'there', 'my friend indeed', 'SHA-256',
     'bc2b7841512a6f4563f723c317909ac305ddbfbdec1daf0055d0587b5db8d635'],
    ['hello hello hello', 'there', 'my friend indeed', 'SHA-384',
     'ee2d1d7dc759c26f2ab8ee6d7cfa0c2313e82650a4514673c867063dc1849040'],
    ['hello hello hello', 'there', 'my friend indeed', 'SHA-512',
     'a7abd704d0be364c6d4a530b6f93fcaff95474a2eee5a127ff86c5d095a2a812'],
  ];

  if (!process.features.openssl_is_boringssl) {
    kTests.push(
      ['hello hello hello', 'there', 'my friend indeed', 'SHA3-256',
       '89b3751df2ada85322a57ec82f7d0a5c233c6def91c92e681bc5118bd5768dca'],
      ['hello hello hello', 'there', 'my friend indeed', 'SHA3-384',
       'b4fa7b9929a595bbaa370eb959b194c1232d5a329abd02a5fa166a1424962fcf'],
      ['hello hello hello', 'there', 'my friend indeed', 'SHA3-512',
       'ac5d90a6bc848961e78a491887539b29c532a9c0d0b39cec464df071a63e0061'],
    );
  } else {
    common.printSkipMessage('Skipping unsupported SHA-3 test cases');
  }

  const tests = Promise.all(kTests.map((args) => test(...args)));

  tests.then(common.mustCall());
}

// Test PBKDF2 key derivation
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
    ['hello hello hello', 'my friend indeed', 1000, 'SHA-1',
     'b747604ca226287ccae90d8d8c119645a80d1154625a56b2debb3f9b172eb134'],
    ['hello hello hello', 'my friend indeed', 1000, 'SHA-256',
     '3cc64f6cfcbdb9c42b63b471016f17d1966b70934b4719a12ce95382940252f2'],
    ['hello hello hello', 'my friend indeed', 1000, 'SHA-384',
     '5ce64241beef3a3931dbfac6eef7303b5bdbea13449d4eeb4f89c3e9f9357c65'],
    ['hello hello hello', 'my friend indeed', 1000, 'SHA-512',
     '12790ce09027db067d680670f4dc704715b5120d139e8fde810afc34fb66f9f1'],
  ];

  if (!process.features.openssl_is_boringssl) {
    kTests.push(
      ['hello hello hello', 'my friend indeed', 1000, 'SHA3-256',
       '0f69b46660cba27b95215d5676492c64ed6abf6d426669a4a02b0ca3a1c36c11'],
      ['hello hello hello', 'my friend indeed', 1000, 'SHA3-384',
       'a2e86a2d4cdf9844d70ae37f71302356ce2b9a899f5d778fc9af64d32e351d70'],
      ['hello hello hello', 'my friend indeed', 1000, 'SHA3-512',
       '03431052c37d626ae3fc1df582ff2a4d610642fc27e1b8130ca5980c0b0756ac']
    );
  } else {
    common.printSkipMessage('Skipping unsupported SHA-3 test cases');
  }

  const promises = kTests.map((args) => test(...args));
  if (fips4) {
    promises.push(assert.rejects(
      test('hello', 'there', 5, 'SHA-256', ''),
      { name: 'OperationError' }));
  }
  const tests = Promise.all(promises);

  tests.then(common.mustCall());
}

// Test default key lengths
{
  const vectors = [
    ['PBKDF2', 'deriveKey', 528],
    ['HKDF', 'deriveKey', 528],
    [{ name: 'HMAC', hash: 'SHA-1' }, 'sign', 512],
    [{ name: 'HMAC', hash: 'SHA-256' }, 'sign', 512],
    // Not long enough secret generated by ECDH
    [{ name: 'HMAC', hash: 'SHA-384' }, 'sign', 1024],
    [{ name: 'HMAC', hash: 'SHA-512' }, 'sign', 1024],
  ];

  if (!process.features.openssl_is_boringssl) {
    vectors.push(
      [{ name: 'HMAC', hash: 'SHA3-256', length: 256 }, 'sign', 256],
      [{ name: 'HMAC', hash: 'SHA3-384', length: 384 }, 'sign', 384],
      [{ name: 'HMAC', hash: 'SHA3-512', length: 512 }, 'sign', 512]
      // This interaction is not defined for now.
      // https://github.com/WICG/webcrypto-modern-algos/issues/23
      // [{ name: 'HMAC', hash: 'SHA3-256' }, 'sign', 256],
      // [{ name: 'HMAC', hash: 'SHA3-384' }, 'sign', 384],
      // [{ name: 'HMAC', hash: 'SHA3-512' }, 'sign', 512],
    );
  } else {
    common.printSkipMessage('Skipping unsupported SHA-3 test cases');
  }

  if (hasOpenSSL(3)) {
    vectors.push(
      ['KMAC128', 'sign', 128],
      [{ name: 'KMAC128', length: 384 }, 'sign', 384],
      ['KMAC256', 'sign', 256],
      [{ name: 'KMAC256', length: 384 }, 'sign', 384],
    );
  }

  (async () => {
    const keyPair = await subtle.generateKey({ name: 'ECDH', namedCurve: 'P-521' }, false, ['deriveKey']);
    for (const [derivedKeyAlgorithm, usage, expected] of vectors) {
      const [result] = await Promise.allSettled([subtle.deriveKey(
        { name: 'ECDH', public: keyPair.publicKey },
        keyPair.privateKey,
        derivedKeyAlgorithm,
        false,
        [usage])]);

      if (expected > 528) {
        assert.strictEqual(result.status, 'rejected');
        assert.match(result.reason.message, /derived bit length is too small/);
      } else {
        assert.strictEqual(result.status, 'fulfilled');
        const derived = result.value;
        if (derived.algorithm.name === 'HMAC' || derived.algorithm.name.startsWith('KMAC')) {
          assert.strictEqual(derived.algorithm.length, expected);
        } else {
          // KDFs cannot be exportable and do not indicate their length
          const secretKey = KeyObject.from(derived);
          assert.strictEqual(secretKey.symmetricKeySize, expected / 8);
        }
      }
    }
  })().then(common.mustCall());
}

{
  const vectors = [
    [{ name: 'HMAC', hash: 'SHA-1' }, 'sign', 512],
    [{ name: 'HMAC', hash: 'SHA-256' }, 'sign', 512],
    [{ name: 'HMAC', hash: 'SHA-384' }, 'sign', 1024],
    [{ name: 'HMAC', hash: 'SHA-512' }, 'sign', 1024],
  ];

  if (!process.features.openssl_is_boringssl) {
    vectors.push(
      [{ name: 'HMAC', hash: 'SHA3-256', length: 256 }, 'sign', 256],
      [{ name: 'HMAC', hash: 'SHA3-384', length: 384 }, 'sign', 384],
      [{ name: 'HMAC', hash: 'SHA3-512', length: 512 }, 'sign', 512],
      // This interaction is not defined for now.
      // https://github.com/WICG/webcrypto-modern-algos/issues/23
      // [{ name: 'HMAC', hash: 'SHA3-256' }, 'sign', 256],
      // [{ name: 'HMAC', hash: 'SHA3-384' }, 'sign', 384],
      // [{ name: 'HMAC', hash: 'SHA3-512' }, 'sign', 512],
    );
  } else {
    common.printSkipMessage('Skipping unsupported SHA-3 test cases');
  }

  if (hasOpenSSL(3)) {
    vectors.push(
      ['KMAC128', 'sign', 128],
      [{ name: 'KMAC128', length: 384 }, 'sign', 384],
      ['KMAC256', 'sign', 256],
      [{ name: 'KMAC256', length: 384 }, 'sign', 384],
    );
  }

  (async () => {
    for (const [derivedKeyAlgorithm, usage, expected] of vectors) {
      const derived = await subtle.deriveKey(
        {
          name: 'PBKDF2',
          salt: new Uint8Array(16),
          hash: 'SHA-256',
          iterations: 1000,
        },
        await subtle.importKey(
          'raw',
          new Uint8Array(8),
          { name: 'PBKDF2' },
          false,
          ['deriveKey']),
        derivedKeyAlgorithm,
        false,
        [usage]);

      assert.strictEqual(derived.algorithm.length, expected);
    }
  })().then(common.mustCall());
}

if (hasOpenSSL(3) && !hasFIPS()) {
  (async () => {
    const derivedKeyAlgorithm = { name: 'KMAC128', length: 0 };
    const usages = ['sign'];
    for (const [algorithm, baseKeyAlgorithm] of [
      [
        {
          name: 'HKDF',
          salt: new Uint8Array(16),
          info: new Uint8Array(),
          hash: 'SHA-256',
        },
        { name: 'HKDF' },
      ],
      [
        {
          name: 'PBKDF2',
          salt: new Uint8Array(16),
          hash: 'SHA-256',
          iterations: 1000,
        },
        { name: 'PBKDF2' },
      ],
    ]) {
      const baseKey = await subtle.importKey(
        'raw',
        new Uint8Array(baseKeyAlgorithm.name === 'HKDF' ? 16 : 8),
        baseKeyAlgorithm,
        false,
        ['deriveKey']);
      const derived = await subtle.deriveKey(
        algorithm,
        baseKey,
        derivedKeyAlgorithm,
        false,
        usages);
      assert.strictEqual(derived.algorithm.length, 0);

      const signature = subtle.sign({
        name: 'KMAC128',
        outputLength: 256,
      }, derived, new Uint8Array());
      assert.strictEqual((await signature).byteLength, 32);
    }
  })().then(common.mustCall());
}

// Test X25519 and X448 key derivation
{
  async function test(name) {
    const [alice, bob] = await Promise.all([
      subtle.generateKey({ name }, true, ['deriveKey']),
      subtle.generateKey({ name }, true, ['deriveKey']),
    ]);

    const [secret1, secret2] = await Promise.all([
      subtle.deriveKey({
        name, public: alice.publicKey
      }, bob.privateKey, {
        name: 'AES-CBC',
        length: 256
      }, true, ['encrypt']),
      subtle.deriveKey({
        name, public: bob.publicKey
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
