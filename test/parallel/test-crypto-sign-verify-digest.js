'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');
const {
  hasOpenSSL,
} = require('../common/crypto');

// Test crypto.signDigest() and crypto.verifyDigest() one-shot APIs.
// These accept a pre-hashed digest and sign/verify it directly.

const data = Buffer.from('Hello world');

// --- RSA PKCS#1 v1.5 ---
{
  const privKey = fixtures.readKey('rsa_private_2048.pem', 'ascii');
  const pubKey = fixtures.readKey('rsa_public_2048.pem', 'ascii');

  const digest = crypto.createHash('sha256').update(data).digest();

  const sig = crypto.signDigest('sha256', digest, privKey);
  assert(Buffer.isBuffer(sig));
  assert.strictEqual(sig.length, 256);

  assert.strictEqual(crypto.verifyDigest('sha256', digest, pubKey, sig), true);

  // Cross-verify: sign with crypto.sign, verify with crypto.verifyDigest
  const sig2 = crypto.sign('sha256', data, privKey);
  assert.strictEqual(crypto.verifyDigest('sha256', digest, pubKey, sig2), true);

  // Cross-verify: sign with crypto.signDigest, verify with crypto.verify
  assert.strictEqual(crypto.verify('sha256', data, pubKey, sig), true);

  // Wrong digest should fail verification
  const wrongDigest = crypto.createHash('sha256').update(Buffer.from('wrong')).digest();
  assert.strictEqual(crypto.verifyDigest('sha256', wrongDigest, pubKey, sig), false);

  // KeyObject forms
  const privKeyObj = crypto.createPrivateKey(privKey);
  const pubKeyObj = crypto.createPublicKey(pubKey);

  const sig3 = crypto.signDigest('sha256', digest, privKeyObj);
  assert.strictEqual(crypto.verifyDigest('sha256', digest, pubKeyObj, sig3), true);

  // Verify with private key (extracts public)
  assert.strictEqual(crypto.verifyDigest('sha256', digest, privKeyObj, sig3), true);
}

// --- RSA-PSS ---
{
  const privKey = fixtures.readKey('rsa_private_2048.pem', 'ascii');
  const pubKey = fixtures.readKey('rsa_public_2048.pem', 'ascii');

  const digest = crypto.createHash('sha256').update(data).digest();

  const sig = crypto.signDigest('sha256', digest, {
    key: privKey,
    padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
    saltLength: 32,
  });
  assert(Buffer.isBuffer(sig));

  assert.strictEqual(crypto.verifyDigest('sha256', digest, {
    key: pubKey,
    padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
    saltLength: 32,
  }, sig), true);

  // Verify with auto salt length
  assert.strictEqual(crypto.verifyDigest('sha256', digest, {
    key: pubKey,
    padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
    saltLength: crypto.constants.RSA_PSS_SALTLEN_AUTO,
  }, sig), true);

  // Cross-verify: sign with crypto.signDigest, verify with crypto.verify
  assert.strictEqual(crypto.verify('sha256', data, {
    key: pubKey,
    padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
    saltLength: crypto.constants.RSA_PSS_SALTLEN_AUTO,
  }, sig), true);

  // Cross-verify: sign with crypto.sign, verify with crypto.verifyDigest
  const sig2 = crypto.sign('sha256', data, {
    key: privKey,
    padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
    saltLength: 32,
  });
  assert.strictEqual(crypto.verifyDigest('sha256', digest, {
    key: pubKey,
    padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
    saltLength: crypto.constants.RSA_PSS_SALTLEN_AUTO,
  }, sig2), true);

  // Wrong digest
  const wrongDigest = crypto.createHash('sha256').update(Buffer.from('wrong')).digest();
  assert.strictEqual(crypto.verifyDigest('sha256', wrongDigest, {
    key: pubKey,
    padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
    saltLength: crypto.constants.RSA_PSS_SALTLEN_AUTO,
  }, sig), false);
}

// --- RSA-PSS key type (hash/padding/salt baked into key) ---
{
  const privKey = fixtures.readKey('rsa_pss_private_2048_sha256_sha256_16.pem', 'ascii');
  const pubKey = fixtures.readKey('rsa_pss_public_2048_sha256_sha256_16.pem', 'ascii');

  const digest = crypto.createHash('sha256').update(data).digest();

  const sig = crypto.signDigest('sha256', digest, privKey);
  assert(Buffer.isBuffer(sig));

  assert.strictEqual(crypto.verifyDigest('sha256', digest, pubKey, sig), true);

  // Cross-verify: sign with crypto.signDigest, verify with crypto.verify
  assert.strictEqual(crypto.verify('sha256', data, pubKey, sig), true);

  // Cross-verify: sign with crypto.sign, verify with crypto.verifyDigest
  const sig2 = crypto.sign('sha256', data, privKey);
  assert.strictEqual(crypto.verifyDigest('sha256', digest, pubKey, sig2), true);

  // Wrong digest
  const wrongDigest = crypto.createHash('sha256').update(Buffer.from('wrong')).digest();
  assert.strictEqual(crypto.verifyDigest('sha256', wrongDigest, pubKey, sig), false);
}

// --- ECDSA (DER encoding, default) ---
{
  const curves = [
    { priv: 'ec_p256_private.pem', pub: 'ec_p256_public.pem', hash: 'sha256' },
    { priv: 'ec_p384_private.pem', pub: 'ec_p384_public.pem', hash: 'sha384' },
    { priv: 'ec_p521_private.pem', pub: 'ec_p521_public.pem', hash: 'sha512' },
  ];

  for (const { priv, pub, hash } of curves) {
    const privKey = fixtures.readKey(priv, 'ascii');
    const pubKey = fixtures.readKey(pub, 'ascii');

    const digest = crypto.createHash(hash).update(data).digest();

    const sig = crypto.signDigest(hash, digest, privKey);
    assert(Buffer.isBuffer(sig));

    assert.strictEqual(crypto.verifyDigest(hash, digest, pubKey, sig), true);

    // Cross-verify: sign with crypto.signDigest, verify with crypto.verify
    assert.strictEqual(crypto.verify(hash, data, pubKey, sig), true);

    // Cross-verify: sign with crypto.sign, verify with crypto.verifyDigest
    const sig2 = crypto.sign(hash, data, privKey);
    assert.strictEqual(crypto.verifyDigest(hash, digest, pubKey, sig2), true);

    // Wrong digest
    const wrongDigest = crypto.createHash(hash).update(Buffer.from('wrong')).digest();
    assert.strictEqual(crypto.verifyDigest(hash, wrongDigest, pubKey, sig), false);
  }
}

// --- ECDSA (ieee-p1363 encoding) ---
{
  const privKey = fixtures.readKey('ec_p256_private.pem', 'ascii');
  const pubKey = fixtures.readKey('ec_p256_public.pem', 'ascii');

  const digest = crypto.createHash('sha256').update(data).digest();

  const sig = crypto.signDigest('sha256', digest, {
    key: privKey,
    dsaEncoding: 'ieee-p1363',
  });
  assert(Buffer.isBuffer(sig));
  // P-256 ieee-p1363 signature is exactly 64 bytes (2 * 32)
  assert.strictEqual(sig.length, 64);

  assert.strictEqual(crypto.verifyDigest('sha256', digest, {
    key: pubKey,
    dsaEncoding: 'ieee-p1363',
  }, sig), true);

  // Cross-verify: sign with crypto.signDigest, verify with crypto.verify
  assert.strictEqual(crypto.verify('sha256', data, {
    key: pubKey,
    dsaEncoding: 'ieee-p1363',
  }, sig), true);

  // Cross-verify: sign with crypto.sign, verify with crypto.verifyDigest
  const sig2 = crypto.sign('sha256', data, {
    key: privKey,
    dsaEncoding: 'ieee-p1363',
  });
  assert.strictEqual(crypto.verifyDigest('sha256', digest, {
    key: pubKey,
    dsaEncoding: 'ieee-p1363',
  }, sig2), true);

  // Wrong digest
  const wrongDigest = crypto.createHash('sha256').update(Buffer.from('wrong')).digest();
  assert.strictEqual(crypto.verifyDigest('sha256', wrongDigest, {
    key: pubKey,
    dsaEncoding: 'ieee-p1363',
  }, sig), false);
}

// --- DSA ---
{
  const privKey = fixtures.readKey('dsa_private.pem', 'ascii');
  const pubKey = fixtures.readKey('dsa_public.pem', 'ascii');

  const digest = crypto.createHash('sha256').update(data).digest();

  const sig = crypto.signDigest('sha256', digest, privKey);
  assert(Buffer.isBuffer(sig));

  assert.strictEqual(crypto.verifyDigest('sha256', digest, pubKey, sig), true);

  // Cross-verify: sign with crypto.signDigest, verify with crypto.verify
  assert.strictEqual(crypto.verify('sha256', data, pubKey, sig), true);

  // Cross-verify: sign with crypto.sign, verify with crypto.verifyDigest
  const sig2 = crypto.sign('sha256', data, privKey);
  assert.strictEqual(crypto.verifyDigest('sha256', digest, pubKey, sig2), true);

  // Wrong digest
  const wrongDigest = crypto.createHash('sha256').update(Buffer.from('wrong')).digest();
  assert.strictEqual(crypto.verifyDigest('sha256', wrongDigest, pubKey, sig), false);
}

// --- Ed25519ph ---
if (hasOpenSSL(3, 2)) {
  const privKey = fixtures.readKey('ed25519_private.pem', 'ascii');
  const pubKey = fixtures.readKey('ed25519_public.pem', 'ascii');

  // Ed25519ph expects a SHA-512 prehash (64 bytes)
  const digest = crypto.createHash('sha512').update(data).digest();
  assert.strictEqual(digest.length, 64);

  const sig = crypto.signDigest(null, digest, privKey);
  assert(Buffer.isBuffer(sig));
  assert.strictEqual(sig.length, 64);

  assert.strictEqual(crypto.verifyDigest(null, digest, pubKey, sig), true);

  // Wrong digest should fail
  const wrongDigest = crypto.createHash('sha512').update(Buffer.from('wrong')).digest();
  assert.strictEqual(crypto.verifyDigest(null, wrongDigest, pubKey, sig), false);

  // Ed25519ph signatures are NOT compatible with Ed25519 signatures.
  // Different domain separation means cross-verify must fail.
  assert.strictEqual(crypto.verify(null, data, pubKey, sig), false);

  // Ed25519 pure signatures are NOT compatible with Ed25519ph.
  // sign() output cannot be verified by verifyDigest().
  const pureSig = crypto.sign(null, data, privKey);
  assert.strictEqual(crypto.verifyDigest(null, digest, pubKey, pureSig), false);

  // KeyObject forms
  const privKeyObj = crypto.createPrivateKey(privKey);
  const pubKeyObj = crypto.createPublicKey(pubKey);

  const sig2 = crypto.signDigest(null, digest, privKeyObj);
  assert.strictEqual(crypto.verifyDigest(null, digest, pubKeyObj, sig2), true);

  // Ed25519ph with context string
  {
    const context = Buffer.from('my context');
    const sig3 = crypto.signDigest(null, digest, { key: privKey, context });
    assert.strictEqual(crypto.verifyDigest(null, digest, { key: pubKey, context }, sig3), true);

    // Wrong context should fail
    assert.strictEqual(crypto.verifyDigest(null, digest, { key: pubKey }, sig3), false);
    assert.strictEqual(crypto.verifyDigest(null, digest, {
      key: pubKey,
      context: Buffer.from('other'),
    }, sig3), false);

    // Ed25519ph+context signatures are NOT compatible with verify(ctx).
    assert.strictEqual(crypto.verify(null, data, { key: pubKey, context }, sig3), false);
  }
}

// --- Ed448ph ---
if (hasOpenSSL(3, 2)) {
  const privKey = fixtures.readKey('ed448_private.pem', 'ascii');
  const pubKey = fixtures.readKey('ed448_public.pem', 'ascii');

  // Ed448ph expects a SHAKE256 prehash (64 bytes)
  const digest = crypto.createHash('shake256', { outputLength: 64 }).update(data).digest();
  assert.strictEqual(digest.length, 64);

  const sig = crypto.signDigest(null, digest, privKey);
  assert(Buffer.isBuffer(sig));
  assert.strictEqual(sig.length, 114);

  assert.strictEqual(crypto.verifyDigest(null, digest, pubKey, sig), true);

  // Wrong digest
  const wrongDigest = crypto.createHash('shake256', { outputLength: 64 }).update(Buffer.from('wrong')).digest();
  assert.strictEqual(crypto.verifyDigest(null, wrongDigest, pubKey, sig), false);

  // Ed448ph signatures are NOT compatible with Ed448 signatures.
  // Different domain separation means cross-verify must fail.
  assert.strictEqual(crypto.verify(null, data, pubKey, sig), false);

  // Ed448 pure signatures are NOT compatible with Ed448ph.
  // sign() output cannot be verified by verifyDigest().
  const pureSig = crypto.sign(null, data, privKey);
  assert.strictEqual(crypto.verifyDigest(null, digest, pubKey, pureSig), false);

  // Ed448ph with context string
  {
    const context = Buffer.from('my context');
    const sig2 = crypto.signDigest(null, digest, { key: privKey, context });
    assert.strictEqual(crypto.verifyDigest(null, digest, { key: pubKey, context }, sig2), true);

    // Wrong context should fail
    assert.strictEqual(crypto.verifyDigest(null, digest, { key: pubKey }, sig2), false);
    assert.strictEqual(crypto.verifyDigest(null, digest, {
      key: pubKey,
      context: Buffer.from('other'),
    }, sig2), false);

    // Ed448ph+context signatures are NOT compatible with verify(ctx).
    assert.strictEqual(crypto.verify(null, data, { key: pubKey, context }, sig2), false);
  }

  // Ed448+context signatures are NOT compatible with Ed448ph.
  // sign(ctx) output cannot be verified by verifyDigest(ctx).
  {
    const context = Buffer.from('my context');
    const ctxSig = crypto.sign(null, data, { key: privKey, context });
    assert.strictEqual(crypto.verifyDigest(null, digest, { key: pubKey, context }, ctxSig), false);
  }

  // Ed448ph with empty context string
  {
    const context = new Uint8Array();
    const sig3 = crypto.signDigest(null, digest, { key: privKey, context });
    assert.strictEqual(crypto.verifyDigest(null, digest, { key: pubKey, context }, sig3), true);
    // Empty context and no context should both verify for Ed448ph
    assert.strictEqual(crypto.verifyDigest(null, digest, { key: pubKey }, sig3), true);
  }
}

// --- Async (callback) mode ---
{
  const privKey = fixtures.readKey('rsa_private_2048.pem', 'ascii');
  const pubKey = fixtures.readKey('rsa_public_2048.pem', 'ascii');

  const digest = crypto.createHash('sha256').update(data).digest();

  crypto.signDigest('sha256', digest, privKey, common.mustSucceed((sig) => {
    assert(Buffer.isBuffer(sig));

    crypto.verifyDigest('sha256', digest, pubKey, sig, common.mustSucceed((result) => {
      assert.strictEqual(result, true);
    }));
  }));
}

// --- Async (callback) error handling ---
{
  // Non-signing key type error is delivered via callback
  const x25519Priv = fixtures.readKey('x25519_private.pem', 'ascii');
  crypto.signDigest('sha256', Buffer.alloc(32), x25519Priv, common.mustCall((err) => {
    assert(err);
    assert.match(err.message, /operation not supported for this keytype/);
  }));
}

if (hasOpenSSL(3, 2)) {
  // Wrong digest length error is delivered via callback
  const edPrivKey = fixtures.readKey('ed25519_private.pem', 'ascii');
  crypto.signDigest(null, Buffer.alloc(32), edPrivKey, common.mustCall((err) => {
    assert(err);
    assert.match(err.message, /invalid digest length/);
  }));
}

// --- Error: unsupported key type for prehashed signing ---
{
  // Ed25519ph/Ed448ph require OpenSSL >= 3.2. On older versions, they
  // should throw PrehashUnsupported.
  if (!hasOpenSSL(3, 2)) {
    const edPrivKey = fixtures.readKey('ed25519_private.pem', 'ascii');
    const edPubKey = fixtures.readKey('ed25519_public.pem', 'ascii');

    assert.throws(() => {
      crypto.signDigest(null, Buffer.alloc(64), edPrivKey);
    }, { code: 'ERR_CRYPTO_OPERATION_FAILED', message: /Prehashed signing is not supported/ });

    assert.throws(() => {
      crypto.verifyDigest(null, Buffer.alloc(64), edPubKey, Buffer.alloc(64));
    }, { code: 'ERR_CRYPTO_OPERATION_FAILED', message: /Prehashed signing is not supported/ });
  }

  // Invalid algorithm argument type
  assert.throws(() => {
    crypto.signDigest(123, Buffer.alloc(32), fixtures.readKey('rsa_private_2048.pem', 'ascii'));
  }, { code: 'ERR_INVALID_ARG_TYPE' });

  // ML-DSA keys are not supported with signDigest/verifyDigest.
  if (hasOpenSSL(3, 5)) {
    for (const alg of ['ml_dsa_44', 'ml_dsa_65', 'ml_dsa_87']) {
      const privKey = fixtures.readKey(`${alg}_private.pem`, 'ascii');
      const pubKey = fixtures.readKey(`${alg}_public.pem`, 'ascii');

      assert.throws(() => {
        crypto.signDigest(null, Buffer.alloc(64), privKey);
      }, { code: 'ERR_CRYPTO_OPERATION_FAILED', message: /Prehashed signing is not supported/ });

      assert.throws(() => {
        crypto.verifyDigest(null, Buffer.alloc(64), pubKey, Buffer.alloc(64));
      }, { code: 'ERR_CRYPTO_OPERATION_FAILED', message: /Prehashed signing is not supported/ });
    }
  }
}

// --- Error: non-signing key types (X25519, X448) ---
{
  const x25519Priv = fixtures.readKey('x25519_private.pem', 'ascii');
  const x25519Pub = fixtures.readKey('x25519_public.pem', 'ascii');

  assert.throws(() => {
    crypto.signDigest('sha256', Buffer.alloc(32), x25519Priv);
  }, { code: 'ERR_OSSL_EVP_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE' });

  assert.throws(() => {
    crypto.verifyDigest('sha256', Buffer.alloc(32), x25519Pub, Buffer.alloc(64));
  }, { code: 'ERR_OSSL_EVP_OPERATION_NOT_SUPPORTED_FOR_THIS_KEYTYPE' });
}

// --- Error: invalid/unsupported digest algorithm ---
{
  const privKey = fixtures.readKey('rsa_private_2048.pem', 'ascii');

  assert.throws(() => {
    crypto.signDigest('nonexistent', Buffer.alloc(32), privKey);
  }, { code: 'ERR_CRYPTO_INVALID_DIGEST' });
}

// --- Error: string inputs are rejected (digest must be binary) ---
{
  const privKey = fixtures.readKey('rsa_private_2048.pem', 'ascii');
  const pubKey = fixtures.readKey('rsa_public_2048.pem', 'ascii');

  // 64-byte string (same length as a SHA-512 digest)
  const strDigest64 = 'a'.repeat(64);
  assert.throws(() => {
    crypto.signDigest('sha256', strDigest64, privKey);
  }, { code: 'ERR_INVALID_ARG_TYPE' });

  assert.throws(() => {
    crypto.verifyDigest('sha256', strDigest64, pubKey, Buffer.alloc(256));
  }, { code: 'ERR_INVALID_ARG_TYPE' });

  // 128-byte string
  const strDigest128 = 'b'.repeat(128);
  assert.throws(() => {
    crypto.signDigest('sha256', strDigest128, privKey);
  }, { code: 'ERR_INVALID_ARG_TYPE' });

  assert.throws(() => {
    crypto.verifyDigest('sha256', strDigest128, pubKey, Buffer.alloc(256));
  }, { code: 'ERR_INVALID_ARG_TYPE' });
}

// --- ECDSA: hash larger than curve order (cross-verify) ---
// Using SHA-512 (64 bytes) with P-256 (32-byte order) and P-384 (48-byte order).
// The digest is larger than the curve's order; ECDSA truncates it internally.
{
  const curves = [
    { priv: 'ec_p256_private.pem', pub: 'ec_p256_public.pem' },
    { priv: 'ec_p384_private.pem', pub: 'ec_p384_public.pem' },
  ];

  for (const { priv, pub } of curves) {
    const privKey = fixtures.readKey(priv, 'ascii');
    const pubKey = fixtures.readKey(pub, 'ascii');

    const digest = crypto.createHash('sha512').update(data).digest();

    // signDigest + verifyDigest
    const sig = crypto.signDigest('sha512', digest, privKey);
    assert.strictEqual(crypto.verifyDigest('sha512', digest, pubKey, sig), true);

    // Cross-verify: sign with crypto.signDigest, verify with crypto.verify
    assert.strictEqual(crypto.verify('sha512', data, pubKey, sig), true);

    // Cross-verify: sign with crypto.sign, verify with crypto.verifyDigest
    const sig2 = crypto.sign('sha512', data, privKey);
    assert.strictEqual(crypto.verifyDigest('sha512', digest, pubKey, sig2), true);
  }
}

// --- Error: wrong digest length for Ed25519ph/Ed448ph ---
if (hasOpenSSL(3, 2)) {
  // Ed25519ph requires exactly 64-byte SHA-512 digest
  {
    const privKey = fixtures.readKey('ed25519_private.pem', 'ascii');
    assert.throws(() => {
      crypto.signDigest(null, Buffer.alloc(32), privKey);
    }, { code: 'ERR_OSSL_INVALID_DIGEST_LENGTH' });
  }

  // Ed448ph requires exactly 64-byte SHAKE256 digest
  {
    const privKey = fixtures.readKey('ed448_private.pem', 'ascii');
    assert.throws(() => {
      crypto.signDigest(null, Buffer.alloc(32), privKey);
    }, { code: 'ERR_OSSL_INVALID_DIGEST_LENGTH' });
  }

  // Ed448ph rejects a valid 128-byte SHAKE256 digest (must be exactly 64 bytes)
  {
    const privKey = fixtures.readKey('ed448_private.pem', 'ascii');
    const shake256_128 = crypto.createHash('shake256', { outputLength: 128 }).update(data).digest();
    assert.strictEqual(shake256_128.length, 128);

    assert.throws(() => {
      crypto.signDigest(null, shake256_128, privKey);
    }, { code: 'ERR_OSSL_INVALID_DIGEST_LENGTH' });
  }
}

// --- Error: wrong digest length for RSA/ECDSA ---
// OpenSSL rejects wrong-length digests when signing. When verifying,
// it returns false (signature mismatch) rather than throwing.
{
  const rsaDigestLengthError = hasOpenSSL(3, 0) ?
    'ERR_OSSL_INVALID_DIGEST_LENGTH' :
    'ERR_OSSL_RSA_INVALID_DIGEST_LENGTH';

  // RSA PKCS#1 v1.5
  {
    const privKey = fixtures.readKey('rsa_private_2048.pem', 'ascii');
    const pubKey = fixtures.readKey('rsa_public_2048.pem', 'ascii');
    const correctDigest = crypto.createHash('sha256').update(data).digest();
    const goodSig = crypto.signDigest('sha256', correctDigest, privKey);

    // Too short
    assert.throws(() => {
      crypto.signDigest('sha256', Buffer.alloc(16), privKey);
    }, { code: rsaDigestLengthError });

    // Too long
    assert.throws(() => {
      crypto.signDigest('sha256', Buffer.alloc(64), privKey);
    }, { code: rsaDigestLengthError });

    // Verify with wrong-length digest returns false
    assert.strictEqual(
      crypto.verifyDigest('sha256', Buffer.alloc(16), pubKey, goodSig), false);
  }

  // RSA-PSS
  {
    const privKey = fixtures.readKey('rsa_private_2048.pem', 'ascii');
    const pubKey = fixtures.readKey('rsa_public_2048.pem', 'ascii');
    const correctDigest = crypto.createHash('sha256').update(data).digest();
    const goodSig = crypto.signDigest('sha256', correctDigest, {
      key: privKey,
      padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
      saltLength: 32,
    });

    assert.throws(() => {
      crypto.signDigest('sha256', Buffer.alloc(16), {
        key: privKey,
        padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
        saltLength: 32,
      });
    }, { code: rsaDigestLengthError });

    assert.strictEqual(
      crypto.verifyDigest('sha256', Buffer.alloc(16), {
        key: pubKey,
        padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
        saltLength: 32,
      }, goodSig), false);
  }

  // ECDSA — OpenSSL 3.x rejects wrong-length digests; 1.1.x accepts them.
  {
    const privKey = fixtures.readKey('ec_p256_private.pem', 'ascii');
    const pubKey = fixtures.readKey('ec_p256_public.pem', 'ascii');
    const correctDigest = crypto.createHash('sha256').update(data).digest();
    const goodSig = crypto.signDigest('sha256', correctDigest, privKey);

    if (hasOpenSSL(3, 0)) {
      const ecdsaDigestLengthError = hasOpenSSL(3, 2) ?
        'ERR_OSSL_EVP_PROVIDER_SIGNATURE_FAILURE' :
        'ERR_CRYPTO_OPERATION_FAILED';

      // Too short
      assert.throws(() => {
        crypto.signDigest('sha256', Buffer.alloc(16), privKey);
      }, { code: ecdsaDigestLengthError });

      // Too long
      assert.throws(() => {
        crypto.signDigest('sha256', Buffer.alloc(64), privKey);
      }, { code: ecdsaDigestLengthError });
    } else {
      // OpenSSL 1.1.x signs any length digest for ECDSA without error
      assert.ok(crypto.signDigest('sha256', Buffer.alloc(16), privKey));
      assert.ok(crypto.signDigest('sha256', Buffer.alloc(64), privKey));
    }

    // Verify with wrong-length digest returns false on all versions
    assert.strictEqual(
      crypto.verifyDigest('sha256', Buffer.alloc(16), pubKey, goodSig), false);
  }
}
