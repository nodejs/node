'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');
const { hasOpenSSL } = require('../common/crypto');

// EC: NIST and OpenSSL curve names are both recognized for raw-public and raw-private
{
  const pubKeyObj = crypto.createPublicKey(
    fixtures.readKey('ec_p256_public.pem', 'ascii'));
  const privKeyObj = crypto.createPrivateKey(
    fixtures.readKey('ec_p256_private.pem', 'ascii'));

  const rawPub = pubKeyObj.export({ format: 'raw-public' });
  const rawPriv = privKeyObj.export({ format: 'raw-private' });

  for (const namedCurve of ['P-256', 'prime256v1']) {
    const importedPub = crypto.createPublicKey({
      key: rawPub, format: 'raw-public', asymmetricKeyType: 'ec', namedCurve,
    });
    assert.strictEqual(importedPub.equals(pubKeyObj), true);

    const importedPriv = crypto.createPrivateKey({
      key: rawPriv, format: 'raw-private', asymmetricKeyType: 'ec', namedCurve,
    });
    assert.strictEqual(importedPriv.equals(privKeyObj), true);
  }
}

// Key types that don't support raw-* formats
{
  for (const [type, pub, priv] of [
    ['rsa', 'rsa_public_2048.pem', 'rsa_private_2048.pem'],
    ['dsa', 'dsa_public.pem', 'dsa_private.pem'],
  ]) {
    const pubKeyObj = crypto.createPublicKey(
      fixtures.readKey(pub, 'ascii'));
    const privKeyObj = crypto.createPrivateKey(
      fixtures.readKey(priv, 'ascii'));

    assert.throws(() => pubKeyObj.export({ format: 'raw-public' }),
                  { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
    assert.throws(() => privKeyObj.export({ format: 'raw-private' }),
                  { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
    assert.throws(() => privKeyObj.export({ format: 'raw-seed' }),
                  { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });

    for (const format of ['raw-public', 'raw-private', 'raw-seed']) {
      assert.throws(() => crypto.createPublicKey({
        key: Buffer.alloc(32), format, asymmetricKeyType: type,
      }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
    }
  }

  // DH keys also don't support raw formats
  {
    const privKeyObj = crypto.createPrivateKey(
      fixtures.readKey('dh_private.pem', 'ascii'));
    assert.throws(() => privKeyObj.export({ format: 'raw-private' }),
                  { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });

    for (const format of ['raw-public', 'raw-private', 'raw-seed']) {
      assert.throws(() => crypto.createPrivateKey({
        key: Buffer.alloc(32), format, asymmetricKeyType: 'dh',
      }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
    }
  }
}

// PQC import throws when PQC is not supported
if (!hasOpenSSL(3, 5)) {
  for (const asymmetricKeyType of [
    'ml-dsa-44', 'ml-dsa-65', 'ml-dsa-87',
    'ml-kem-512', 'ml-kem-768', 'ml-kem-1024',
    'slh-dsa-sha2-128f', 'slh-dsa-shake-128f',
  ]) {
    for (const format of ['raw-public', 'raw-private', 'raw-seed']) {
      assert.throws(() => crypto.createPublicKey({
        key: Buffer.alloc(32), format, asymmetricKeyType,
      }), { code: 'ERR_INVALID_ARG_VALUE' });
    }
  }
}

// EC: P-256 and P-384 keys cannot be imported as private/public of the other type
{
  const p256Pub = crypto.createPublicKey(
    fixtures.readKey('ec_p256_public.pem', 'ascii'));
  const p384Pub = crypto.createPublicKey(
    fixtures.readKey('ec_p384_public.pem', 'ascii'));
  const p256Priv = crypto.createPrivateKey(
    fixtures.readKey('ec_p256_private.pem', 'ascii'));
  const p384Priv = crypto.createPrivateKey(
    fixtures.readKey('ec_p384_private.pem', 'ascii'));

  const p256RawPub = p256Pub.export({ format: 'raw-public' });
  const p384RawPub = p384Pub.export({ format: 'raw-public' });
  const p256RawPriv = p256Priv.export({ format: 'raw-private' });
  const p384RawPriv = p384Priv.export({ format: 'raw-private' });

  // P-256 public imported as P-384
  assert.throws(() => crypto.createPublicKey({
    key: p256RawPub, format: 'raw-public',
    asymmetricKeyType: 'ec', namedCurve: 'P-384',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  // P-384 public imported as P-256
  assert.throws(() => crypto.createPublicKey({
    key: p384RawPub, format: 'raw-public',
    asymmetricKeyType: 'ec', namedCurve: 'P-256',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  // P-256 private imported as P-384
  assert.throws(() => crypto.createPrivateKey({
    key: p256RawPriv, format: 'raw-private',
    asymmetricKeyType: 'ec', namedCurve: 'P-384',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  // P-384 private imported as P-256
  assert.throws(() => crypto.createPrivateKey({
    key: p384RawPriv, format: 'raw-private',
    asymmetricKeyType: 'ec', namedCurve: 'P-256',
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// ML-KEM: -768 and -512 public keys cannot be imported as the other type
if (hasOpenSSL(3, 5)) {
  const mlKem512Pub = crypto.createPublicKey(
    fixtures.readKey('ml_kem_512_public.pem', 'ascii'));
  const mlKem768Pub = crypto.createPublicKey(
    fixtures.readKey('ml_kem_768_public.pem', 'ascii'));

  const mlKem512RawPub = mlKem512Pub.export({ format: 'raw-public' });
  const mlKem768RawPub = mlKem768Pub.export({ format: 'raw-public' });

  assert.throws(() => crypto.createPublicKey({
    key: mlKem512RawPub, format: 'raw-public', asymmetricKeyType: 'ml-kem-768',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => crypto.createPublicKey({
    key: mlKem768RawPub, format: 'raw-public', asymmetricKeyType: 'ml-kem-512',
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// ML-DSA: -44 and -65 public keys cannot be imported as the other type
if (hasOpenSSL(3, 5)) {
  const mlDsa44Pub = crypto.createPublicKey(
    fixtures.readKey('ml_dsa_44_public.pem', 'ascii'));
  const mlDsa65Pub = crypto.createPublicKey(
    fixtures.readKey('ml_dsa_65_public.pem', 'ascii'));

  const mlDsa44RawPub = mlDsa44Pub.export({ format: 'raw-public' });
  const mlDsa65RawPub = mlDsa65Pub.export({ format: 'raw-public' });

  assert.throws(() => crypto.createPublicKey({
    key: mlDsa44RawPub, format: 'raw-public', asymmetricKeyType: 'ml-dsa-65',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => crypto.createPublicKey({
    key: mlDsa65RawPub, format: 'raw-public', asymmetricKeyType: 'ml-dsa-44',
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// SLH-DSA: mismatched key types with different sizes are rejected
if (hasOpenSSL(3, 5)) {
  const slh128fPub = crypto.createPublicKey(
    fixtures.readKey('slh_dsa_sha2_128f_public.pem', 'ascii'));
  const slh192fPub = crypto.createPublicKey(
    fixtures.readKey('slh_dsa_sha2_192f_public.pem', 'ascii'));
  const slh128fPriv = crypto.createPrivateKey(
    fixtures.readKey('slh_dsa_sha2_128f_private.pem', 'ascii'));
  const slh192fPriv = crypto.createPrivateKey(
    fixtures.readKey('slh_dsa_sha2_192f_private.pem', 'ascii'));

  const rawPub128f = slh128fPub.export({ format: 'raw-public' });
  const rawPub192f = slh192fPub.export({ format: 'raw-public' });
  const rawPriv128f = slh128fPriv.export({ format: 'raw-private' });
  const rawPriv192f = slh192fPriv.export({ format: 'raw-private' });

  assert.throws(() => crypto.createPublicKey({
    key: rawPub128f, format: 'raw-public',
    asymmetricKeyType: 'slh-dsa-sha2-192f',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => crypto.createPublicKey({
    key: rawPub192f, format: 'raw-public',
    asymmetricKeyType: 'slh-dsa-sha2-128f',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => crypto.createPrivateKey({
    key: rawPriv128f, format: 'raw-private',
    asymmetricKeyType: 'slh-dsa-sha2-192f',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => crypto.createPrivateKey({
    key: rawPriv192f, format: 'raw-private',
    asymmetricKeyType: 'slh-dsa-sha2-128f',
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// Passphrase cannot be used with raw formats
{
  const privKeyObj = crypto.createPrivateKey(
    fixtures.readKey('ed25519_private.pem', 'ascii'));

  assert.throws(() => privKeyObj.export({
    format: 'raw-private', passphrase: 'test',
  }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });

  assert.throws(() => privKeyObj.export({
    format: 'raw-seed', passphrase: 'test',
  }), { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
}

// raw-seed export is rejected for key types that do not support seeds
{
  const ecPriv = crypto.createPrivateKey(
    fixtures.readKey('ec_p256_private.pem', 'ascii'));
  assert.throws(() => ecPriv.export({ format: 'raw-seed' }),
                { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });

  for (const type of ['ed25519', 'ed448', 'x25519', 'x448']) {
    const priv = crypto.createPrivateKey(
      fixtures.readKey(`${type}_private.pem`, 'ascii'));
    assert.throws(() => priv.export({ format: 'raw-seed' }),
                  { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
  }

  if (hasOpenSSL(3, 5)) {
    const slhPriv = crypto.createPrivateKey(
      fixtures.readKey('slh_dsa_sha2_128f_private.pem', 'ascii'));
    assert.throws(() => slhPriv.export({ format: 'raw-seed' }),
                  { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
  }
}

// raw-private cannot be used for ml-kem and ml-dsa
if (hasOpenSSL(3, 5)) {
  for (const type of ['ml-kem-512', 'ml-dsa-44']) {
    const priv = crypto.createPrivateKey(
      fixtures.readKey(`${type.replaceAll('-', '_')}_private.pem`, 'ascii'));
    assert.throws(() => priv.export({ format: 'raw-private' }),
                  { code: 'ERR_CRYPTO_INCOMPATIBLE_KEY_OPTIONS' });
  }
}

// EC: defaults to uncompressed, can be switched to compressed, both can be imported
{
  const pubKeyObj = crypto.createPublicKey(
    fixtures.readKey('ec_p256_public.pem', 'ascii'));

  const defaultExport = pubKeyObj.export({ format: 'raw-public' });
  const compressed = pubKeyObj.export({ format: 'raw-public', type: 'compressed' });
  const uncompressed = pubKeyObj.export({ format: 'raw-public', type: 'uncompressed' });

  // Default is uncompressed
  assert.deepStrictEqual(defaultExport, uncompressed);

  // Compressed starts with 0x02 or 0x03 and is 33 bytes for P-256
  assert.strictEqual(compressed.byteLength, 33);
  assert(compressed[0] === 0x02 || compressed[0] === 0x03);

  // Uncompressed starts with 0x04 and is 65 bytes for P-256
  assert.strictEqual(uncompressed.byteLength, 65);
  assert.strictEqual(uncompressed[0], 0x04);

  // Both can be imported
  const fromCompressed = crypto.createPublicKey({
    key: compressed, format: 'raw-public',
    asymmetricKeyType: 'ec', namedCurve: 'P-256',
  });
  assert.strictEqual(fromCompressed.equals(pubKeyObj), true);

  const fromUncompressed = crypto.createPublicKey({
    key: uncompressed, format: 'raw-public',
    asymmetricKeyType: 'ec', namedCurve: 'P-256',
  });
  assert.strictEqual(fromUncompressed.equals(pubKeyObj), true);
}

// Compressed and uncompressed are the only recognized options
{
  const pubKeyObj = crypto.createPublicKey(
    fixtures.readKey('ec_p256_public.pem', 'ascii'));

  assert.throws(() => pubKeyObj.export({ format: 'raw-public', type: 'hybrid' }),
                { code: 'ERR_INVALID_ARG_VALUE' });

  assert.throws(() => pubKeyObj.export({ format: 'raw-public', type: 'invalid' }),
                { code: 'ERR_INVALID_ARG_VALUE' });
}

// None of the raw types can be used with symmetric key objects
{
  const secretKey = crypto.createSecretKey(Buffer.alloc(32));

  for (const format of ['raw-public', 'raw-private', 'raw-seed']) {
    assert.throws(() => secretKey.export({ format }),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  }
}

// Private key objects created from raw-seed or raw-private can be passed to createPublicKey()
{
  // Ed25519 raw-private -> createPublicKey
  const edPriv = crypto.createPrivateKey(
    fixtures.readKey('ed25519_private.pem', 'ascii'));
  const edPub = crypto.createPublicKey(
    fixtures.readKey('ed25519_public.pem', 'ascii'));
  const rawPriv = edPriv.export({ format: 'raw-private' });
  const importedPriv = crypto.createPrivateKey({
    key: rawPriv, format: 'raw-private', asymmetricKeyType: 'ed25519',
  });
  const derivedPub = crypto.createPublicKey(importedPriv);
  assert.strictEqual(derivedPub.equals(edPub), true);
  // Private key must not be extractable from the derived public key.
  assert.throws(() => derivedPub.export({ format: 'pem', type: 'pkcs8' }),
                { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(() => derivedPub.export({ format: 'der', type: 'pkcs8' }),
                { code: 'ERR_INVALID_ARG_VALUE' });

  // EC raw-private -> createPublicKey
  const ecPriv = crypto.createPrivateKey(
    fixtures.readKey('ec_p256_private.pem', 'ascii'));
  const ecPub = crypto.createPublicKey(
    fixtures.readKey('ec_p256_public.pem', 'ascii'));
  const ecRawPriv = ecPriv.export({ format: 'raw-private' });
  const ecImportedPriv = crypto.createPrivateKey({
    key: ecRawPriv, format: 'raw-private',
    asymmetricKeyType: 'ec', namedCurve: 'P-256',
  });
  const ecDerivedPub = crypto.createPublicKey(ecImportedPriv);
  assert.strictEqual(ecDerivedPub.equals(ecPub), true);
  // Private key must not be extractable from the derived public key.
  assert.throws(() => ecDerivedPub.export({ format: 'pem', type: 'pkcs8' }),
                { code: 'ERR_INVALID_ARG_VALUE' });
  assert.throws(() => ecDerivedPub.export({ format: 'pem', type: 'sec1' }),
                { code: 'ERR_INVALID_ARG_VALUE' });

  // PQC raw-seed -> createPublicKey
  if (hasOpenSSL(3, 5)) {
    const mlDsaPriv = crypto.createPrivateKey(
      fixtures.readKey('ml_dsa_44_private.pem', 'ascii'));
    const mlDsaPub = crypto.createPublicKey(
      fixtures.readKey('ml_dsa_44_public.pem', 'ascii'));
    const mlDsaRawSeed = mlDsaPriv.export({ format: 'raw-seed' });
    const mlDsaImportedPriv = crypto.createPrivateKey({
      key: mlDsaRawSeed, format: 'raw-seed', asymmetricKeyType: 'ml-dsa-44',
    });
    const mlDsaDerivedPub = crypto.createPublicKey(mlDsaImportedPriv);
    assert.strictEqual(mlDsaDerivedPub.equals(mlDsaPub), true);
    // Private key must not be extractable from the derived public key.
    assert.throws(() => mlDsaDerivedPub.export({ format: 'pem', type: 'pkcs8' }),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  }
}

// raw-public EC keys that are garbage/not on curve are rejected
{
  const garbage = Buffer.alloc(33, 0xff);
  garbage[0] = 0x02; // Valid compressed prefix but invalid point

  assert.throws(() => crypto.createPublicKey({
    key: garbage, format: 'raw-public',
    asymmetricKeyType: 'ec', namedCurve: 'P-256',
  }), { code: 'ERR_INVALID_ARG_VALUE' });

  // Totally random garbage
  assert.throws(() => crypto.createPublicKey({
    key: Buffer.alloc(10, 0xab), format: 'raw-public',
    asymmetricKeyType: 'ec', namedCurve: 'P-256',
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// Unrecognized namedCurve values are rejected
{
  assert.throws(() => crypto.createPublicKey({
    key: Buffer.alloc(33), format: 'raw-public',
    asymmetricKeyType: 'ec', namedCurve: 'not-a-curve',
  }), { code: 'ERR_CRYPTO_INVALID_CURVE' });

  assert.throws(() => crypto.createPrivateKey({
    key: Buffer.alloc(32), format: 'raw-private',
    asymmetricKeyType: 'ec', namedCurve: 'not-a-curve',
  }), { code: 'ERR_CRYPTO_INVALID_CURVE' });
}

// x25519, ed25519, x448, and ed448 cannot be used as 'ec' namedCurve values
{
  for (const type of ['ed25519', 'x25519', 'ed448', 'x448']) {
    const priv = crypto.createPrivateKey(
      fixtures.readKey(`${type}_private.pem`, 'ascii'));
    const pub = crypto.createPublicKey(
      fixtures.readKey(`${type}_public.pem`, 'ascii'));

    const rawPub = pub.export({ format: 'raw-public' });
    const rawPriv = priv.export({ format: 'raw-private' });

    // Try to import as EC - must fail
    assert.throws(() => crypto.createPublicKey({
      key: rawPub, format: 'raw-public',
      asymmetricKeyType: 'ec', namedCurve: type,
    }), { code: 'ERR_CRYPTO_INVALID_CURVE' });

    assert.throws(() => crypto.createPrivateKey({
      key: rawPriv, format: 'raw-private',
      asymmetricKeyType: 'ec', namedCurve: type,
    }), { code: 'ERR_CRYPTO_INVALID_CURVE' });
  }
}

// Missing asymmetricKeyType option
{
  assert.throws(() => crypto.createPublicKey({
    key: Buffer.alloc(32), format: 'raw-public',
  }), { code: 'ERR_INVALID_ARG_TYPE' });
}

// Unknown asymmetricKeyType value
{
  assert.throws(() => crypto.createPublicKey({
    key: Buffer.alloc(32), format: 'raw-public',
    asymmetricKeyType: 'unknown',
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

// Non-buffer key data
{
  assert.throws(() => crypto.createPublicKey({
    key: 12345, format: 'raw-public',
    asymmetricKeyType: 'ec', namedCurve: 'P-256',
  }), { code: 'ERR_INVALID_ARG_TYPE' });
}

// Missing namedCurve for EC
{
  assert.throws(() => crypto.createPublicKey({
    key: Buffer.alloc(33), format: 'raw-public',
    asymmetricKeyType: 'ec',
  }), { code: 'ERR_INVALID_ARG_TYPE' });
}
