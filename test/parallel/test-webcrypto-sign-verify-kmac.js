'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3))
  common.skip('requires OpenSSL >= 3');

const assert = require('assert');
const { subtle } = globalThis.crypto;

const vectors = require('../fixtures/crypto/kmac')();

async function testVerify({ algorithm,
                            key,
                            data,
                            customization,
                            length,
                            expected }) {
  const [
    verifyKey,
    noVerifyKey,
    keyPair,
  ] = await Promise.all([
    subtle.importKey(
      'raw-secret',
      key,
      { name: algorithm },
      false,
      ['verify']),
    subtle.importKey(
      'raw-secret',
      key,
      { name: algorithm },
      false,
      ['sign']),
    subtle.generateKey(
      'Ed25519',
      false,
      ['sign']),
  ]);

  const signParams = {
    name: algorithm,
    length,
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
      message: /Unable to use this key to verify/
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
      length: length === 256 ? 512 : 256,
    }, verifyKey, expected, data)));
  }
}

async function testSign({ algorithm,
                          key,
                          data,
                          customization,
                          length,
                          expected }) {
  const [
    signKey,
    noSignKey,
    keyPair,
  ] = await Promise.all([
    subtle.importKey(
      'raw-secret',
      key,
      { name: algorithm },
      false,
      ['verify', 'sign']),
    subtle.importKey(
      'raw-secret',
      key,
      { name: algorithm },
      false,
      ['verify']),
    subtle.generateKey(
      'Ed25519',
      false,
      ['sign']),
  ]);

  const signParams = {
    name: algorithm,
    length,
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
      message: /Unable to use this key to sign/
    });
}

(async function() {
  const variations = [];

  for (const vector of vectors) {
    variations.push(testVerify(vector));
    variations.push(testSign(vector));
  }

  await Promise.all(variations);
})().then(common.mustCall());
