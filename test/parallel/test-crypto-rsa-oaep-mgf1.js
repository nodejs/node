'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Tests the `mgf1Hash` option of crypto.publicEncrypt() and
// crypto.privateDecrypt(), which allows the MGF1 digest of RSA-OAEP padding to
// differ from the OAEP message digest (`oaepHash`). This is required for
// interoperability with profiles such as XML Encryption's `rsa-oaep-mgf1p`,
// where the OAEP digest may be changed but MGF1 is fixed to SHA-1.

const assert = require('assert');
const crypto = require('crypto');
const fixtures = require('../common/fixtures');
const { hasFIPS } = require('../common/crypto');

const constants = crypto.constants;

const publicKey = fixtures.readKey('rsa_public.pem', 'ascii');
const privateKey = fixtures.readKey('rsa_private.pem', 'ascii');

const input = Buffer.from('the quick brown fox jumps over the lazy dog');

// A round-trip with mismatched OAEP and MGF1 digests must succeed when both
// sides agree on the digests.
{
  const encrypted = crypto.publicEncrypt({
    key: publicKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash: 'sha256',
    mgf1Hash: 'sha1',
  }, input);

  const decrypted = crypto.privateDecrypt({
    key: privateKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash: 'sha256',
    mgf1Hash: 'sha1',
  }, encrypted);

  assert.deepStrictEqual(decrypted, input);
}

// mgf1Hash actually affects the padding: a ciphertext produced with
// oaepHash=sha256 and mgf1Hash=sha1 must NOT decrypt when MGF1 defaults to the
// OAEP digest (sha256), which is the pre-existing behavior.
{
  const encrypted = crypto.publicEncrypt({
    key: publicKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash: 'sha256',
    mgf1Hash: 'sha1',
  }, input);

  assert.throws(() => {
    crypto.privateDecrypt({
      key: privateKey,
      padding: constants.RSA_PKCS1_OAEP_PADDING,
      oaepHash: 'sha256',
      // No mgf1Hash: MGF1 follows oaepHash (sha256) and must fail to unpad.
    }, encrypted);
  }, {
    code: hasFIPS(3, 5) ? 'ERR_OSSL_EVP_PROVIDER_ASYM_CIPHER_FAILURE' :
      'ERR_OSSL_RSA_OAEP_DECODING_ERROR'
  });
}

// Backward compatibility: omitting mgf1Hash on both sides keeps MGF1 == oaepHash
// (the historical behavior), so this round-trips, and setting mgf1Hash equal to
// oaepHash is equivalent to omitting it.
{
  const encrypted = crypto.publicEncrypt({
    key: publicKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash: 'sha256',
  }, input);

  const decrypted = crypto.privateDecrypt({
    key: privateKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash: 'sha256',
    mgf1Hash: 'sha256',
  }, encrypted);

  assert.deepStrictEqual(decrypted, input);
}

// The default oaepHash is sha1, so mgf1Hash defaults to sha1 as well. A
// ciphertext encrypted with all defaults must decrypt with an explicit
// mgf1Hash: 'sha1'.
{
  const encrypted = crypto.publicEncrypt({
    key: publicKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
  }, input);

  const decrypted = crypto.privateDecrypt({
    key: privateKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
    mgf1Hash: 'sha1',
  }, encrypted);

  assert.deepStrictEqual(decrypted, input);
}

// A few other digest combinations round-trip.
for (const [oaepHash, mgf1Hash] of [
  ['sha512', 'sha1'],
  ['sha384', 'sha256'],
  ['sha1', 'sha256'],
]) {
  const encrypted = crypto.publicEncrypt({
    key: publicKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash,
    mgf1Hash,
  }, input);

  const decrypted = crypto.privateDecrypt({
    key: privateKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash,
    mgf1Hash,
  }, encrypted);

  assert.deepStrictEqual(decrypted, input);
}

// mgf1Hash must be a string.
for (const mgf1Hash of [1, true, {}, [], null]) {
  assert.throws(() => {
    crypto.publicEncrypt({
      key: publicKey,
      padding: constants.RSA_PKCS1_OAEP_PADDING,
      oaepHash: 'sha256',
      mgf1Hash,
    }, input);
  }, { code: 'ERR_INVALID_ARG_TYPE' });
}

// An unknown mgf1Hash digest name is rejected.
assert.throws(() => {
  crypto.publicEncrypt({
    key: publicKey,
    padding: constants.RSA_PKCS1_OAEP_PADDING,
    oaepHash: 'sha256',
    mgf1Hash: 'not-a-real-digest',
  }, input);
}, { code: 'ERR_OSSL_EVP_INVALID_DIGEST' });
