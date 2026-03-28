'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const {
  generateKeyPair,
  generateKeyPairSync,
  createPublicKey,
  createPrivateKey,
} = require('crypto');
const { hasOpenSSL } = require('../common/crypto');

// Test generateKeyPairSync with raw encoding for EdDSA/ECDH key types.
{
  const types = ['ed25519', 'x25519'];
  if (!process.features.openssl_is_boringssl) {
    types.push('ed448', 'x448');
  }
  for (const type of types) {
    const { publicKey, privateKey } = generateKeyPairSync(type, {
      publicKeyEncoding: { format: 'raw-public' },
      privateKeyEncoding: { format: 'raw-private' },
    });

    assert(Buffer.isBuffer(publicKey));
    assert(Buffer.isBuffer(privateKey));

    // Roundtrip: import from raw, re-export, and compare.
    const importedPub = createPublicKey({
      key: publicKey,
      format: 'raw-public',
      asymmetricKeyType: type,
    });
    const importedPriv = createPrivateKey({
      key: privateKey,
      format: 'raw-private',
      asymmetricKeyType: type,
    });

    assert.deepStrictEqual(importedPub.export({ format: 'raw-public' }),
                           publicKey);
    assert.deepStrictEqual(importedPriv.export({ format: 'raw-private' }),
                           privateKey);
  }
}

// Test async generateKeyPair with raw encoding for EdDSA/ECDH key types.
{
  const types = ['ed25519', 'x25519'];
  if (!process.features.openssl_is_boringssl) {
    types.push('ed448', 'x448');
  }
  for (const type of types) {
    generateKeyPair(type, {
      publicKeyEncoding: { format: 'raw-public' },
      privateKeyEncoding: { format: 'raw-private' },
    }, common.mustSucceed((publicKey, privateKey) => {
      assert(Buffer.isBuffer(publicKey));
      assert(Buffer.isBuffer(privateKey));
    }));
  }
}

// Test generateKeyPairSync with raw encoding for EC keys.
{
  for (const namedCurve of ['P-256', 'P-384', 'P-521']) {
    const { publicKey, privateKey } = generateKeyPairSync('ec', {
      namedCurve,
      publicKeyEncoding: { format: 'raw-public' },
      privateKeyEncoding: { format: 'raw-private' },
    });

    assert(Buffer.isBuffer(publicKey));
    assert(Buffer.isBuffer(privateKey));

    // Roundtrip with EC.
    const importedPub = createPublicKey({
      key: publicKey,
      format: 'raw-public',
      asymmetricKeyType: 'ec',
      namedCurve,
    });
    const importedPriv = createPrivateKey({
      key: privateKey,
      format: 'raw-private',
      asymmetricKeyType: 'ec',
      namedCurve,
    });

    assert.deepStrictEqual(importedPub.export({ format: 'raw-public' }),
                           publicKey);
    assert.deepStrictEqual(importedPriv.export({ format: 'raw-private' }),
                           privateKey);
  }
}

// Test EC raw-public with compressed and uncompressed point formats.
{
  const { publicKey: uncompressed } = generateKeyPairSync('ec', {
    namedCurve: 'P-256',
    publicKeyEncoding: { format: 'raw-public', type: 'uncompressed' },
    privateKeyEncoding: { format: 'raw-private' },
  });
  const { publicKey: compressed } = generateKeyPairSync('ec', {
    namedCurve: 'P-256',
    publicKeyEncoding: { format: 'raw-public', type: 'compressed' },
    privateKeyEncoding: { format: 'raw-private' },
  });

  // Uncompressed P-256 public key is 65 bytes, compressed is 33 bytes.
  assert.strictEqual(uncompressed.length, 65);
  assert.strictEqual(compressed.length, 33);
}

// Test mixed: one side raw, other side pem.
{
  const { publicKey, privateKey } = generateKeyPairSync('ed25519', {
    publicKeyEncoding: { format: 'raw-public' },
    privateKeyEncoding: { format: 'pem', type: 'pkcs8' },
  });

  assert(Buffer.isBuffer(publicKey));
  assert.strictEqual(typeof privateKey, 'string');
  assert(privateKey.startsWith('-----BEGIN PRIVATE KEY-----'));
}

{
  const { publicKey, privateKey } = generateKeyPairSync('ed25519', {
    publicKeyEncoding: { format: 'pem', type: 'spki' },
    privateKeyEncoding: { format: 'raw-private' },
  });

  assert.strictEqual(typeof publicKey, 'string');
  assert(publicKey.startsWith('-----BEGIN PUBLIC KEY-----'));
  assert(Buffer.isBuffer(privateKey));
}

// Test mixed: one side raw, other side KeyObject (no encoding).
{
  const { publicKey, privateKey } = generateKeyPairSync('ed25519', {
    publicKeyEncoding: { format: 'raw-public' },
  });

  assert(Buffer.isBuffer(publicKey));
  assert.strictEqual(privateKey.type, 'private');
}

// Test error: raw with RSA.
{
  assert.throws(() => generateKeyPairSync('rsa', {
    modulusLength: 2048,
    publicKeyEncoding: { format: 'raw-public' },
    privateKeyEncoding: { format: 'raw-private' },
  }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
}

// Test error: raw with DSA.
{
  assert.throws(() => generateKeyPairSync('dsa', {
    modulusLength: 2048,
    publicKeyEncoding: { format: 'raw-public' },
    privateKeyEncoding: { format: 'raw-private' },
  }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
}

// Test error: raw-private in publicKeyEncoding.
{
  assert.throws(() => generateKeyPairSync('ed25519', {
    publicKeyEncoding: { format: 'raw-private' },
    privateKeyEncoding: { format: 'raw-private' },
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// Test error: raw-seed in publicKeyEncoding.
{
  assert.throws(() => generateKeyPairSync('ed25519', {
    publicKeyEncoding: { format: 'raw-seed' },
    privateKeyEncoding: { format: 'raw-private' },
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// Test error: raw-public in privateKeyEncoding.
{
  assert.throws(() => generateKeyPairSync('ed25519', {
    publicKeyEncoding: { format: 'raw-public' },
    privateKeyEncoding: { format: 'raw-public' },
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// Test error: passphrase with raw private key encoding.
{
  assert.throws(() => generateKeyPairSync('ed25519', {
    publicKeyEncoding: { format: 'raw-public' },
    privateKeyEncoding: {
      format: 'raw-private',
      cipher: 'aes-256-cbc',
      passphrase: 'secret',
    },
  }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
}

// PQC key types
if (hasOpenSSL(3, 5)) {
  // Test raw encoding for ML-DSA key types (raw-public + raw-seed only).
  {
    for (const type of ['ml-dsa-44', 'ml-dsa-65', 'ml-dsa-87']) {
      const { publicKey, privateKey } = generateKeyPairSync(type, {
        publicKeyEncoding: { format: 'raw-public' },
        privateKeyEncoding: { format: 'raw-seed' },
      });

      assert(Buffer.isBuffer(publicKey));
      assert(Buffer.isBuffer(privateKey));
      // raw-seed output should be 32 bytes for ML-DSA.
      assert.strictEqual(privateKey.length, 32);
    }
  }

  // Test error: raw-private with ML-DSA (not supported).
  {
    assert.throws(() => generateKeyPairSync('ml-dsa-44', {
      publicKeyEncoding: { format: 'raw-public' },
      privateKeyEncoding: { format: 'raw-private' },
    }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
  }

  // Test raw encoding for ML-KEM key types (raw-public + raw-seed only).
  {
    for (const type of ['ml-kem-512', 'ml-kem-768', 'ml-kem-1024']) {
      const { publicKey, privateKey } = generateKeyPairSync(type, {
        publicKeyEncoding: { format: 'raw-public' },
        privateKeyEncoding: { format: 'raw-seed' },
      });

      assert(Buffer.isBuffer(publicKey));
      assert(Buffer.isBuffer(privateKey));
      // raw-seed output should be 64 bytes for ML-KEM.
      assert.strictEqual(privateKey.length, 64);
    }
  }

  // Test error: raw-private with ML-KEM (not supported).
  {
    assert.throws(() => generateKeyPairSync('ml-kem-512', {
      publicKeyEncoding: { format: 'raw-public' },
      privateKeyEncoding: { format: 'raw-private' },
    }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
  }

  // Test raw encoding for SLH-DSA key types.
  {
    for (const type of ['slh-dsa-sha2-128f', 'slh-dsa-shake-128f']) {
      const { publicKey, privateKey } = generateKeyPairSync(type, {
        publicKeyEncoding: { format: 'raw-public' },
        privateKeyEncoding: { format: 'raw-private' },
      });

      assert(Buffer.isBuffer(publicKey));
      assert(Buffer.isBuffer(privateKey));
    }
  }

  // Test error: raw-seed with SLH-DSA (not supported).
  {
    assert.throws(() => generateKeyPairSync('slh-dsa-sha2-128f', {
      publicKeyEncoding: { format: 'raw-public' },
      privateKeyEncoding: { format: 'raw-seed' },
    }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
  }

  // Test async generateKeyPair with raw encoding for PQC types.
  {
    generateKeyPair('ml-dsa-44', {
      publicKeyEncoding: { format: 'raw-public' },
      privateKeyEncoding: { format: 'raw-seed' },
    }, common.mustSucceed((publicKey, privateKey) => {
      assert(Buffer.isBuffer(publicKey));
      assert(Buffer.isBuffer(privateKey));
      assert.strictEqual(privateKey.length, 32);
    }));
  }
}
