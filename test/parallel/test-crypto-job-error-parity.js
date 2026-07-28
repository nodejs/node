'use strict';

// Tests that sync and async crypto operations produce errors with matching
// properties. This validates that both code paths go through the same
// CryptoErrorStore-based error creation rather than diverging.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');
const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');

function getError(fn) {
  let err;
  assert.throws(fn, (e) => { err = e; return true; });
  return err;
}

// Compare error properties between sync and async errors.
function assertErrorMatch(syncErr, asyncErr, label) {
  assert(asyncErr instanceof Error, `${label}: expected async Error`);
  assert.strictEqual(asyncErr.message, syncErr.message,
                     `${label}: message mismatch`);
  assert.strictEqual(asyncErr.code, syncErr.code,
                     `${label}: code mismatch`);
  // OpenSSL error decoration properties
  for (const prop of ['library', 'reason']) {
    assert.strictEqual(asyncErr[prop], syncErr[prop],
                       `${label}: ${prop} mismatch`);
  }
}

const data = Buffer.from('test data');

// === crypto.sign / crypto.verify ===

// Sign: ed25519 with explicit digest (OpenSSL error at signInit)
{
  const privateKey = crypto.createPrivateKey(
    fixtures.readKey('ed25519_private.pem'));

  const syncErr = getError(() => crypto.sign('sha256', data, privateKey));
  assert(syncErr.code);
  assert(syncErr.message);

  crypto.sign('sha256', data, privateKey, common.mustCall((asyncErr) => {
    assertErrorMatch(syncErr, asyncErr, 'sign: ed25519 + sha256');
  }));
}

// Sign: context on key that does not support it (CONTEXT_UNSUPPORTED)
{
  const privateKey = crypto.createPrivateKey(
    fixtures.readKey('rsa_private.pem'));
  const keyOpts = { key: privateKey, context: Buffer.alloc(1) };

  const syncErr = getError(() => crypto.sign('sha256', data, keyOpts));
  assert.strictEqual(syncErr.code, 'ERR_CRYPTO_OPERATION_FAILED');
  assert.match(syncErr.message, /Context parameter is unsupported/);

  crypto.sign('sha256', data, keyOpts, common.mustCall((asyncErr) => {
    assertErrorMatch(syncErr, asyncErr, 'sign: RSA + context');
  }));
}

// Sign: RSA key too small for digest (OpenSSL error)
{
  const { privateKey } = crypto.generateKeyPairSync('rsa', {
    modulusLength: 512,
  });

  const syncErr = getError(() => crypto.sign('sha512', data, privateKey));
  assert.match(syncErr.message, /digest[\s_]too[\s_]big[\s_]for[\s_]rsa[\s_]key/i);

  crypto.sign('sha512', data, privateKey, common.mustCall((asyncErr) => {
    assertErrorMatch(syncErr, asyncErr, 'sign: RSA 512 + sha512');
  }));
}

// Sign: illegal RSA padding mode (OpenSSL error)
{
  const privateKey = crypto.createPrivateKey(
    fixtures.readKey('rsa_private.pem'));

  const keyOpts = {
    key: privateKey,
    padding: crypto.constants.RSA_PKCS1_OAEP_PADDING,
  };

  const syncErr = getError(() => crypto.sign('sha256', data, keyOpts));
  assert.match(syncErr.message,
               /illegal[\s_]or[\s_]unsupported[\s_]padding[\s_]mode/i);

  crypto.sign('sha256', data, keyOpts, common.mustCall((asyncErr) => {
    assertErrorMatch(syncErr, asyncErr, 'sign: RSA + OAEP padding');
  }));
}

// Verify: X25519 key (not a signing key - OpenSSL error)
{
  const publicKey = crypto.createPublicKey(
    fixtures.readKey('x25519_public.pem'));
  const sig = Buffer.alloc(64);

  const syncErr = getError(() =>
    crypto.verify(null, data, publicKey, sig));
  assert(syncErr.message);

  crypto.verify(null, data, publicKey, sig, common.mustCall((asyncErr) => {
    assertErrorMatch(syncErr, asyncErr, 'verify: X25519 key');
  }));
}

// === crypto.diffieHellman ===

// DH: Mismatched EC curves (OpenSSL error)
{
  const alicePrivate = crypto.createPrivateKey(
    fixtures.readKey('ec_p256_private.pem'));
  const bobPublic = crypto.createPublicKey(
    fixtures.readKey('ec_p384_public.pem'));

  const syncErr = getError(() =>
    crypto.diffieHellman({
      privateKey: alicePrivate,
      publicKey: bobPublic,
    }));
  assert(syncErr.message);

  crypto.diffieHellman({
    privateKey: alicePrivate,
    publicKey: bobPublic,
  }, common.mustCall((asyncErr) => {
    assertErrorMatch(syncErr, asyncErr, 'DH: mismatched EC curves');
  }));
}

// DH: X25519 with all-zero public key (OpenSSL error)
{
  const privateKey = crypto.createPrivateKey(
    fixtures.readKey('x25519_private.pem'));
  const publicKey = crypto.createPublicKey(
    '-----BEGIN PUBLIC KEY-----\n' +
    'MCowBQYDK2VuAyEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\n' +
    '-----END PUBLIC KEY-----');

  const syncErr = getError(() =>
    crypto.diffieHellman({ publicKey, privateKey }));
  assert(syncErr.message);

  crypto.diffieHellman({ publicKey, privateKey },
                       common.mustCall((asyncErr) => {
                         assertErrorMatch(
                           syncErr, asyncErr, 'DH: X25519 zero key');
                       }));
}

// DH: Mismatched DH group params (OpenSSL error)
{
  const alice = crypto.generateKeyPairSync('dh', { group: 'modp5' });
  const bob = crypto.generateKeyPairSync('dh', { group: 'modp18' });

  const syncErr = getError(() =>
    crypto.diffieHellman({
      privateKey: alice.privateKey,
      publicKey: bob.publicKey,
    }));
  assert(syncErr.message);

  crypto.diffieHellman({
    privateKey: alice.privateKey,
    publicKey: bob.publicKey,
  }, common.mustCall((asyncErr) => {
    assertErrorMatch(syncErr, asyncErr, 'DH: mismatched DH groups');
  }));
}

// === crypto.encapsulate / crypto.decapsulate ===
if (hasOpenSSL(3)) {
  // KEM: Decapsulate with wrong private key type
  {
    const rsaPublicKey = crypto.createPublicKey(
      fixtures.readKey('rsa_public.pem'));
    const x25519Private = crypto.createPrivateKey(
      fixtures.readKey('x25519_private.pem'));

    const { ciphertext } = crypto.encapsulate(rsaPublicKey);

    const syncErr = getError(() =>
      crypto.decapsulate(x25519Private, ciphertext));
    assert.strictEqual(syncErr.code, 'ERR_CRYPTO_OPERATION_FAILED');
    assert.match(syncErr.message, /Decapsulation failed/);

    crypto.decapsulate(x25519Private, ciphertext,
                       common.mustCall((asyncErr) => {
                         assertErrorMatch(
                           syncErr, asyncErr, 'KEM: wrong key decapsulate');
                       }));
  }
}

// === crypto.hkdf / crypto.hkdfSync ===

// HKDF: Invalid digest (AdditionalConfig error - should throw in both paths)
{
  const key = crypto.randomBytes(32);
  const salt = Buffer.alloc(0);
  const info = Buffer.alloc(0);

  const syncErr = getError(() =>
    crypto.hkdfSync('nonexistent', key, salt, info, 64));
  assert.strictEqual(syncErr.code, 'ERR_CRYPTO_INVALID_DIGEST');

  // Async hkdf also throws synchronously for setup errors
  const asyncErr = getError(() =>
    crypto.hkdf('nonexistent', key, salt, info, 64, common.mustNotCall()));
  assertErrorMatch(syncErr, asyncErr, 'HKDF: invalid digest');
}

// HKDF: keylen too large (AdditionalConfig error)
{
  const key = crypto.randomBytes(32);
  const salt = Buffer.alloc(0);
  const info = Buffer.alloc(0);
  // sha256 output is 32 bytes, max HKDF output is 255 * 32 = 8160
  const tooLarge = 255 * 32 + 1;

  const syncErr = getError(() =>
    crypto.hkdfSync('sha256', key, salt, info, tooLarge));
  assert.strictEqual(syncErr.code, 'ERR_CRYPTO_INVALID_KEYLEN');

  const asyncErr = getError(() =>
    crypto.hkdf('sha256', key, salt, info, tooLarge, common.mustNotCall()));
  assertErrorMatch(syncErr, asyncErr, 'HKDF: keylen too large');
}

// === crypto.pbkdf2 / crypto.pbkdf2Sync ===

// PBKDF2: Invalid digest (AdditionalConfig error)
{
  const syncErr = getError(() =>
    crypto.pbkdf2Sync('password', 'salt', 1, 32, 'nonexistent'));
  assert.strictEqual(syncErr.code, 'ERR_CRYPTO_INVALID_DIGEST');

  // pbkdf2 also throws synchronously for setup errors
  const asyncErr = getError(() =>
    crypto.pbkdf2('password', 'salt', 1, 32, 'nonexistent',
                  common.mustNotCall()));
  assertErrorMatch(syncErr, asyncErr, 'PBKDF2: invalid digest');
}

// === crypto.scrypt / crypto.scryptSync ===

// Scrypt: Invalid params (AdditionalConfig error sent to OpenSSL)
{
  const syncErr = getError(() =>
    crypto.scryptSync('password', 'salt', 64, { N: 3 }));
  assert.strictEqual(syncErr.code, 'ERR_CRYPTO_INVALID_SCRYPT_PARAMS');

  const asyncErr = getError(() =>
    crypto.scrypt('password', 'salt', 64, { N: 3 },
                  common.mustNotCall()));
  assertErrorMatch(syncErr, asyncErr, 'Scrypt: N not power of 2');
}

{
  const syncErr = getError(() =>
    crypto.scryptSync('password', 'salt', 64, { N: 1 }));
  assert.strictEqual(syncErr.code, 'ERR_CRYPTO_INVALID_SCRYPT_PARAMS');

  const asyncErr = getError(() =>
    crypto.scrypt('password', 'salt', 64, { N: 1 },
                  common.mustNotCall()));
  assertErrorMatch(syncErr, asyncErr, 'Scrypt: N=1');
}
