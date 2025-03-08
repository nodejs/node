'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { subtle } = globalThis.crypto;

const kTests = [
  {
    name: 'X25519',
    size: 32,
    pkcs8: '302e020100300506032b656e04220420c8838e76d057dfb7d8c95a69e138160ad' +
           'd6373fd71a4d276bb56e3a81b64ff61',
    spki: '302a300506032b656e0321001cf2b1e6022ec537371ed7f53e54fa1154d83e98eb' +
          '64ea51fae5b3307cfe9706',
    result: '2768409dfab99ec23b8c89b93ff5880295f76176088f89e43dfebe7ea1950008'
  },
  {
    name: 'X448',
    size: 56,
    pkcs8: '3046020100300506032b656f043a043858c7d29a3eb519b29d00cfb191bb64fc6' +
           'd8a42d8f17176272b89f2272d1819295c6525c0829671b052ef0727530f188e31' +
           'd0cc53bf26929e',
    spki: '3042300506032b656f033900b604a1d1a5cd1d9426d561ef630a9eb16cbe69d5b9' +
          'ca615edc53633efb52ea31e6e6a0a1dbacc6e76cbce6482d7e4ba3d55d9e802765' +
          'ce6f',
    result: 'f0f6c5f17f94f4291eab7178866d37ec8906dd6c514143dc85be7cf28deff39b'
  },
];

async function prepareKeys() {
  const keys = {};
  await Promise.all(
    kTests.map(async ({ name, size, pkcs8, spki, result }) => {
      const [
        privateKey,
        publicKey,
      ] = await Promise.all([
        subtle.importKey(
          'pkcs8',
          Buffer.from(pkcs8, 'hex'),
          { name },
          true,
          ['deriveKey', 'deriveBits']),
        subtle.importKey(
          'spki',
          Buffer.from(spki, 'hex'),
          { name },
          true,
          []),
      ]);
      keys[name] = {
        privateKey,
        publicKey,
        size,
        result,
      };
    }));
  return keys;
}

(async function() {
  const keys = await prepareKeys();
  const otherArgs = [
    { name: 'HMAC', hash: 'SHA-256', length: 256 },
    true,
    ['sign', 'verify']];

  await Promise.all(
    Object.keys(keys).map(async (name) => {
      const { result, privateKey, publicKey } = keys[name];

      {
        // Good parameters
        const key = await subtle.deriveKey({
          name,
          public: publicKey
        }, privateKey, ...otherArgs);

        const raw = await subtle.exportKey('raw', key);

        assert.strictEqual(Buffer.from(raw).toString('hex'), result);
      }

      {
        // Case insensitivity
        const key = await subtle.deriveKey({
          name: name.toLowerCase(),
          public: publicKey
        }, privateKey, {
          name: 'HmAc',
          hash: 'SHA-256',
          length: 256
        }, true, ['sign', 'verify']);

        const raw = await subtle.exportKey('raw', key);

        assert.strictEqual(Buffer.from(raw).toString('hex'), result);
      }
    }));

  // Error tests
  {
    // Missing public property
    await assert.rejects(
      subtle.deriveKey(
        { name: 'X448' },
        keys.X448.privateKey,
        ...otherArgs),
      { code: 'ERR_MISSING_OPTION' });
  }

  {
    // The public property is not a CryptoKey
    await assert.rejects(
      subtle.deriveKey(
        {
          name: 'X448',
          public: { message: 'Not a CryptoKey' }
        },
        keys.X448.privateKey,
        ...otherArgs),
      { code: 'ERR_INVALID_ARG_TYPE' });
  }

  {
    // Mismatched named curves
    await assert.rejects(
      subtle.deriveKey(
        {
          name: 'X448',
          public: keys.X25519.publicKey
        },
        keys.X448.privateKey,
        ...otherArgs),
      { message: 'algorithm.public must be an X448 key' });
  }

  {
    // Base key is not a private key
    await assert.rejects(
      subtle.deriveKey(
        {
          name: 'X448',
          public: keys.X448.publicKey
        },
        keys.X448.publicKey,
        ...otherArgs),
      { name: 'InvalidAccessError' });
  }

  {
    // Public is not a public key
    await assert.rejects(
      subtle.deriveKey(
        {
          name: 'X448',
          public: keys.X448.privateKey
        },
        keys.X448.privateKey,
        ...otherArgs),
      { name: 'InvalidAccessError' });
  }

  {
    // Public is a secret key
    const keyData = globalThis.crypto.getRandomValues(new Uint8Array(32));
    const key = await subtle.importKey(
      'raw',
      keyData,
      { name: 'AES-CBC', length: 256 },
      false, ['encrypt']);

    await assert.rejects(
      subtle.deriveKey(
        {
          name: 'X448',
          public: key
        },
        keys.X448.publicKey,
        ...otherArgs),
      { name: 'InvalidAccessError' });
  }
})().then(common.mustCall());
