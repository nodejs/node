'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');
const assert = require('assert');
const crypto = require('crypto');

// SM4-GCM and SM4-CCM are provider-only algorithms in OpenSSL 3.x (they
// have no legacy EVP_CIPHER implementation) and are available in the
// default provider since OpenSSL 3.1. SM4-XTS was added in OpenSSL 3.2.
// Refs: https://github.com/nodejs/node/issues/64866
if (!hasOpenSSL(3, 1)) common.skip('SM4 AEAD modes require OpenSSL >= 3.1');

if (crypto.getFips()) common.skip('SM4 is not FIPS-approved');

const ciphers = crypto.getCiphers();
if (!ciphers.includes('sm4-cbc'))
  common.skip('SM4 support is disabled in this build');

// The provider-only SM4 modes must be reported by getCiphers().
assert(ciphers.includes('sm4-gcm'));
assert(ciphers.includes('sm4-ccm'));
const hasSm4Xts = hasOpenSSL(3, 2);
if (hasSm4Xts) assert(ciphers.includes('sm4-xts'));

// getCipherInfo() must resolve provider-only ciphers, both by name and
// by nid.
{
  const info = crypto.getCipherInfo('sm4-gcm');
  assert(info);
  assert.strictEqual(info.name, 'sm4-gcm');
  assert.strictEqual(info.nid, 1248);
  assert.strictEqual(info.mode, 'gcm');
  assert.strictEqual(info.keyLength, 16);
  assert.strictEqual(info.ivLength, 12);
  assert.deepStrictEqual(crypto.getCipherInfo(info.nid), info);
}

{
  const info = crypto.getCipherInfo('sm4-ccm');
  assert(info);
  assert.strictEqual(info.name, 'sm4-ccm');
  assert.strictEqual(info.nid, 1249);
  assert.strictEqual(info.mode, 'ccm');
  assert.strictEqual(info.keyLength, 16);
  assert.strictEqual(info.ivLength, 12);
  assert.deepStrictEqual(crypto.getCipherInfo(info.nid), info);
}

if (hasSm4Xts) {
  const info = crypto.getCipherInfo('sm4-xts');
  assert(info);
  assert.strictEqual(info.name, 'sm4-xts');
  assert.strictEqual(info.nid, 1290);
  assert.strictEqual(info.mode, 'xts');
  assert.strictEqual(info.keyLength, 32);
  assert.deepStrictEqual(crypto.getCipherInfo(info.nid), info);
}

// Test vectors from RFC 8998, appendix A.
const kKey = Buffer.from('0123456789ABCDEFFEDCBA9876543210', 'hex');
const kIv = Buffer.from('00001234567800000000ABCD', 'hex');
const kAad = Buffer.from('FEEDFACEDEADBEEFFEEDFACEDEADBEEFABADDAD2', 'hex');
const kPlaintext = Buffer.from(
  'AAAAAAAAAAAAAAAABBBBBBBBBBBBBBBB' +
    'CCCCCCCCCCCCCCCCDDDDDDDDDDDDDDDD' +
    'EEEEEEEEEEEEEEEEFFFFFFFFFFFFFFFF' +
    'EEEEEEEEEEEEEEEEAAAAAAAAAAAAAAAA',
  'hex',
);

// RFC 8998, appendix A.1.
{
  const kCiphertext = Buffer.from(
    '17F399F08C67D5EE19D0DC9969C4BB7D' +
      '5FD46FD3756489069157B282BB200735' +
      'D82710CA5C22F0CCFA7CBF93D496AC15' +
      'A56834CBCF98C397B4024A2691233B8D',
    'hex',
  );
  const kAuthTag = Buffer.from('83DE3541E4C2B58177E065A9BF7B62EC', 'hex');

  const cipher = crypto.createCipheriv('sm4-gcm', kKey, kIv);
  cipher.setAAD(kAad);
  const ciphertext = Buffer.concat([cipher.update(kPlaintext), cipher.final()]);
  assert.deepStrictEqual(ciphertext, kCiphertext);
  assert.deepStrictEqual(cipher.getAuthTag(), kAuthTag);

  const decipher = crypto.createDecipheriv('sm4-gcm', kKey, kIv);
  decipher.setAAD(kAad);
  decipher.setAuthTag(kAuthTag);
  const plaintext = Buffer.concat([
    decipher.update(kCiphertext),
    decipher.final(),
  ]);
  assert.deepStrictEqual(plaintext, kPlaintext);

  // A tampered authentication tag must be rejected.
  const badTag = Buffer.from(kAuthTag);
  badTag[0] ^= 1;
  const failing = crypto.createDecipheriv('sm4-gcm', kKey, kIv);
  failing.setAAD(kAad);
  failing.setAuthTag(badTag);
  failing.update(kCiphertext);
  assert.throws(() => failing.final(), {
    message: /Unsupported state or unable to authenticate data/,
  });
}

// RFC 8998, appendix A.2.
{
  const kCiphertext = Buffer.from(
    '48AF93501FA62ADBCD414CCE6034D895' +
      'DDA1BF8F132F042098661572E7483094' +
      'FD12E518CE062C98ACEE28D95DF4416B' +
      'ED31A2F04476C18BB40C84A74B97DC5B',
    'hex',
  );
  const kAuthTag = Buffer.from('16842D4FA186F56AB33256971FA110F4', 'hex');

  const cipher = crypto.createCipheriv('sm4-ccm', kKey, kIv, {
    authTagLength: 16,
  });
  cipher.setAAD(kAad, { plaintextLength: kPlaintext.length });
  const ciphertext = Buffer.concat([cipher.update(kPlaintext), cipher.final()]);
  assert.deepStrictEqual(ciphertext, kCiphertext);
  assert.deepStrictEqual(cipher.getAuthTag(), kAuthTag);

  const decipher = crypto.createDecipheriv('sm4-ccm', kKey, kIv, {
    authTagLength: 16,
  });
  decipher.setAuthTag(kAuthTag);
  decipher.setAAD(kAad, { plaintextLength: kCiphertext.length });
  const plaintext = Buffer.concat([
    decipher.update(kCiphertext),
    decipher.final(),
  ]);
  assert.deepStrictEqual(plaintext, kPlaintext);
}

// There are no official SM4-XTS test vectors; do a round-trip instead.
if (hasSm4Xts) {
  const key = Buffer.from(
    '00112233445566778899AABBCCDDEEFFFFEEDDCCBBAA99887766554433221100',
    'hex',
  );
  const iv = Buffer.from('000102030405060708090A0B0C0D0E0F', 'hex');

  const cipher = crypto.createCipheriv('sm4-xts', key, iv);
  const ciphertext = Buffer.concat([cipher.update(kPlaintext), cipher.final()]);
  assert.strictEqual(ciphertext.length, kPlaintext.length);
  assert.notDeepStrictEqual(ciphertext, kPlaintext);

  const decipher = crypto.createDecipheriv('sm4-xts', key, iv);
  const plaintext = Buffer.concat([
    decipher.update(ciphertext),
    decipher.final(),
  ]);
  assert.deepStrictEqual(plaintext, kPlaintext);
}

// Cipher name lookup is case-insensitive, including for fetched
// provider-only ciphers.
{
  const cipher = crypto.createCipheriv('SM4-GCM', kKey, kIv);
  cipher.setAAD(kAad);
  cipher.update(kPlaintext);
  cipher.final();
}

// The EVP_CIPHER_fetch() fallback must not make unknown algorithms resolve.
{
  const unknown = 'sm4-gcm-not-a-real-cipher';

  // Repeated, so that a failed lookup is not cached as a success.
  for (let i = 0; i < 2; i++) {
    assert.strictEqual(crypto.getCipherInfo(unknown), undefined);
    assert.throws(() => crypto.createCipheriv(unknown, kKey, kIv), {
      code: 'ERR_CRYPTO_UNKNOWN_CIPHER',
      message: 'Unknown cipher',
    });
  }

  // Unknown names must not leave anything behind on the OpenSSL error queue
  // for the next operation to trip over.
  const cipher = crypto.createCipheriv('sm4-gcm', kKey, kIv);
  cipher.setAAD(kAad);
  assert.strictEqual(
    Buffer.concat([cipher.update(kPlaintext), cipher.final()]).length,
    kPlaintext.length,
  );
}

// Cipher::FromNid() now falls back to a name lookup. A nid that is a valid
// object identifier but not a cipher must still resolve to nothing.
{
  assert.strictEqual(crypto.getCipherInfo(672), undefined); // NID_sha256
  assert.strictEqual(crypto.getCipherInfo(0), undefined); // NID_undef
  assert.strictEqual(crypto.getCipherInfo(-1), undefined);
}
