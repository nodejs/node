'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasFIPS, hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3))
  common.skip('requires OpenSSL >= 3');

const assert = require('assert');
const { subtle } = globalThis.crypto;
const fips = hasFIPS();
const fips4 = hasFIPS(4);

const vectors = require('../fixtures/crypto/kmac')();

function isFipsProviderUnsupported(err) {
  return err.name === 'OperationError' &&
    err.cause?.code === 'ERR_OSSL_EVP_UNSUPPORTED';
}

function usesNonFipsImplementation({ key, keyLength, outputLength }) {
  const keyLengthInBits = keyLength ?? key.byteLength * 8;
  return outputLength === 0 ||
    outputLength % 8 !== 0 ||
    keyLengthInBits < 32 ||
    keyLengthInBits % 8 !== 0;
}

function isFips4Incompatible({ key, keyLength, outputLength }) {
  const keyLengthInBits = keyLength ?? key.byteLength * 8;
  return keyLengthInBits < 128 ||
    keyLengthInBits % 8 !== 0 ||
    outputLength % 8 !== 0;
}

async function testVerify({ algorithm,
                            key,
                            keyLength,
                            data,
                            customization,
                            outputLength,
                            expected }) {
  const importAlgorithm = keyLength === undefined ?
    { name: algorithm } :
    { name: algorithm, length: keyLength };
  const [
    verifyKey,
    noVerifyKey,
    keyPair,
  ] = await Promise.all([
    subtle.importKey(
      'raw-secret',
      key,
      importAlgorithm,
      false,
      ['verify']),
    subtle.importKey(
      'raw-secret',
      key,
      importAlgorithm,
      false,
      ['sign']),
    subtle.generateKey(
      'Ed25519',
      false,
      ['sign']),
  ]);

  const signParams = {
    name: algorithm,
    outputLength,
    customization,
  };

  assert(await subtle.verify(signParams, verifyKey, expected, data));

  // Test verification with altered buffers
  const copy = Buffer.from(data);
  const sigcopy = Buffer.from(expected);
  const p = subtle.verify(signParams, verifyKey, sigcopy, copy);
  copy[0] = 255 - copy[0];
  sigcopy[0] = 255 - sigcopy[0];
  assert(await p);

  // Test failure when using wrong key
  await assert.rejects(
    subtle.verify(signParams, noVerifyKey, expected, data), {
      message: /Unable to use this key to verify/
    });

  // Test failure when using the wrong algorithms
  await assert.rejects(
    subtle.verify(signParams, keyPair.publicKey, expected, data), {
      message: /Key algorithm mismatch/
    });

  // Test failure when signature is altered
  {
    const copy = Buffer.from(expected);
    copy[0] = 255 - copy[0];
    assert(!(await subtle.verify(
      signParams,
      verifyKey,
      copy,
      data)));
    assert(!(await subtle.verify(
      signParams,
      verifyKey,
      copy.slice(1),
      data)));
  }

  // Test failure when data is altered
  {
    const copy = Buffer.from(data);
    copy[0] = 255 - copy[0];
    assert(!(await subtle.verify(signParams, verifyKey, expected, copy)));
  }

  // Test failure when wrong algorithm is used
  {
    const otherAlgorithm = algorithm === 'KMAC128' ? 'KMAC256' : 'KMAC128';
    const keyWithOtherAlgorithm = await subtle.importKey(
      'raw-secret',
      key,
      { name: otherAlgorithm },
      false,
      ['verify']);
    const otherParams = { ...signParams, name: otherAlgorithm };
    assert(!(await subtle.verify(otherParams, keyWithOtherAlgorithm, expected, data)));
  }

  // Test failure when output length is different
  {
    assert(!(await subtle.verify({
      ...signParams,
      outputLength: outputLength === 256 ? 512 : 256,
    }, verifyKey, expected, data)));
  }
}

async function testSign({ algorithm,
                          key,
                          keyLength,
                          data,
                          customization,
                          outputLength,
                          expected }) {
  const importAlgorithm = keyLength === undefined ?
    { name: algorithm } :
    { name: algorithm, length: keyLength };
  const [
    signKey,
    noSignKey,
    keyPair,
  ] = await Promise.all([
    subtle.importKey(
      'raw-secret',
      key,
      importAlgorithm,
      false,
      ['verify', 'sign']),
    subtle.importKey(
      'raw-secret',
      key,
      importAlgorithm,
      false,
      ['verify']),
    subtle.generateKey(
      'Ed25519',
      false,
      ['sign']),
  ]);

  const signParams = {
    name: algorithm,
    outputLength,
    customization,
  };

  {
    const sig = await subtle.sign(signParams, signKey, data);
    assert.strictEqual(
      Buffer.from(sig).toString('hex'),
      Buffer.from(expected).toString('hex'));
    assert(await subtle.verify(signParams, signKey, sig, data));
  }

  {
    const copy = Buffer.from(data);
    const p = subtle.sign(signParams, signKey, copy);
    copy[0] = 255 - copy[0];
    const sig = await p;
    assert(await subtle.verify(signParams, signKey, sig, data));
  }

  // Test failure when no sign usage
  await assert.rejects(
    subtle.sign(signParams, noSignKey, data), {
      message: /Unable to use this key to sign/
    });

  // Test failure when using the wrong algorithms
  await assert.rejects(
    subtle.sign(signParams, keyPair.privateKey, data), {
      message: /Key algorithm mismatch/
    });
}

(async function() {
  const variations = [];

  for (const vector of vectors) {
    if (fips && usesNonFipsImplementation(vector)) continue;

    if (fips4 && isFips4Incompatible(vector)) {
      variations.push(assert.rejects(
        testVerify(vector), isFipsProviderUnsupported));
      variations.push(assert.rejects(
        testSign(vector), isFipsProviderUnsupported));
    } else {
      variations.push(testVerify(vector));
      variations.push(testSign(vector));
    }
  }

  await Promise.all(variations);
})().then(common.mustCall());

(async function() {
  const key = await subtle.importKey(
    'raw-secret',
    new Uint8Array(16),
    { name: 'KMAC128' },
    false,
    ['sign', 'verify']);
  const algorithm = {
    name: 'KMAC128',
    outputLength: fips ? 16 : 9,
    customization: new Uint8Array(),
  };
  const data = new Uint8Array([1, 2, 3]);

  const signature = await subtle.sign(algorithm, key, data);
  assert.strictEqual(signature.byteLength, 2);
  if (!fips)
    assert.strictEqual(new Uint8Array(signature)[1] & 0b01111111, 0);
  assert(await subtle.verify(algorithm, key, signature, data));

  if (fips) {
    const signature128 = await subtle.sign({
      ...algorithm,
      outputLength: 128,
    }, key, data);
    assert.strictEqual(signature128.byteLength, 16);
    assert(await subtle.verify({
      ...algorithm,
      outputLength: 128,
    }, key, signature128, data));
  } else {
    const signature16 = new Uint8Array(await subtle.sign({
      ...algorithm,
      outputLength: 16,
    }, key, data));
    signature16[1] &= 0b10000000;
    assert.notDeepStrictEqual(new Uint8Array(signature), signature16);
  }

  const invalidSignature = new Uint8Array(signature);
  if (fips)
    invalidSignature[0] ^= 0b00000001;
  else
    invalidSignature[1] |= 0b00000001;
  assert(!(await subtle.verify(algorithm, key, invalidSignature, data)));

  if (!fips) {
    const nonByteKey = await subtle.importKey(
      'raw-secret',
      new Uint8Array([0xff, 0xff, 0xff, 0xff]),
      { name: 'KMAC128', length: 25 },
      false,
      ['sign', 'verify']);
    const nonByteKeySignature = subtle.sign({
      ...algorithm,
      outputLength: 16,
    }, nonByteKey, data);
    const result = await nonByteKeySignature;
    assert.strictEqual(result.byteLength, 2);
    assert(await subtle.verify({
      ...algorithm,
      outputLength: 16,
    }, nonByteKey, result, data));
  }
})().then(common.mustCall());

(async function() {
  if (fips) return;

  const data = new Uint8Array([1, 2, 3]);

  for (const name of ['KMAC128', 'KMAC256']) {
    for (const keyData of [
      new Uint8Array(),
      new Uint8Array([1]),
      new Uint8Array([1, 2, 3]),
    ]) {
      const key = await subtle.importKey(
        'raw-secret',
        keyData,
        { name },
        true,
        ['sign', 'verify']);
      assert.strictEqual(key.algorithm.length, keyData.byteLength * 8);

      const algorithm = { name, outputLength: 256 };
      const signature = subtle.sign(algorithm, key, data);
      const result = await signature;
      assert.strictEqual(result.byteLength, 32);
      assert(await subtle.verify(algorithm, key, result, data));
    }
  }
})().then(common.mustCall());
