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

  await Promise.all(
    Object.keys(keys).map(async (name) => {
      const { size, result, privateKey, publicKey } = keys[name];

      {
        // Good parameters
        const bits = await subtle.deriveBits({
          name,
          public: publicKey
        }, privateKey, 8 * size);

        assert(bits instanceof ArrayBuffer);
        assert.strictEqual(Buffer.from(bits).toString('hex'), result);
      }

      {
        // Case insensitivity
        const bits = await subtle.deriveBits({
          name: name.toLowerCase(),
          public: publicKey
        }, privateKey, 8 * size);

        assert.strictEqual(Buffer.from(bits).toString('hex'), result);
      }

      {
        // Null length
        const bits = await subtle.deriveBits({
          name,
          public: publicKey
        }, privateKey, null);

        assert.strictEqual(Buffer.from(bits).toString('hex'), result);
      }

      {
        // Default length
        const bits = await subtle.deriveBits({
          name,
          public: publicKey
        }, privateKey);

        assert.strictEqual(Buffer.from(bits).toString('hex'), result);
      }

      {
        // Short Result
        const bits = await subtle.deriveBits({
          name,
          public: publicKey
        }, privateKey, 8 * size - 32);

        assert.strictEqual(
          Buffer.from(bits).toString('hex'),
          result.slice(0, -8));
      }

      {
        // Too long result
        await assert.rejects(subtle.deriveBits({
          name,
          public: publicKey
        }, privateKey, 8 * size + 8), {
          message: /derived bit length is too small/
        });
      }

      {
        // Non-multiple of 8
        const bits = await subtle.deriveBits({
          name,
          public: publicKey
        }, privateKey, 8 * size - 11);

        assert.strictEqual(
          Buffer.from(bits).toString('hex'),
          result.slice(0, -2));
      }
    }));

  // Error tests
  {
    // Missing public property
    await assert.rejects(
      subtle.deriveBits(
        { name: 'X25519' },
        keys.X25519.privateKey,
        8 * keys.X25519.size),
      { code: 'ERR_MISSING_OPTION' });
  }

  {
    // The public property is not a CryptoKey
    await assert.rejects(
      subtle.deriveBits(
        {
          name: 'X25519',
          public: { message: 'Not a CryptoKey' }
        },
        keys.X25519.privateKey,
        8 * keys.X25519.size),
      { code: 'ERR_INVALID_ARG_TYPE' });
  }

  {
    // Base key is not a private key
    await assert.rejects(subtle.deriveBits({
      name: 'X25519',
      public: keys.X25519.publicKey
    }, keys.X25519.publicKey, null), {
      name: 'InvalidAccessError'
    });
  }

  {
    // Base key is not a private key
    await assert.rejects(subtle.deriveBits({
      name: 'X25519',
      public: keys.X25519.privateKey
    }, keys.X25519.publicKey, null), {
      name: 'InvalidAccessError'
    });
  }

  {
    // Public is a secret key
    const keyData = globalThis.crypto.getRandomValues(new Uint8Array(32));
    const key = await subtle.importKey(
      'raw',
      keyData,
      { name: 'AES-CBC', length: 256 },
      false, ['encrypt']);

    await assert.rejects(subtle.deriveBits({
      name: 'X25519',
      public: key
    }, keys.X25519.publicKey, null), {
      name: 'InvalidAccessError'
    });
  }
})().then(common.mustCall());
