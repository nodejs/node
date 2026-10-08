'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { createPrivateKey, createPublicKey, sign, verify } = require('node:crypto');
const fixtures = require('../common/fixtures');
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');

const privateKey = createPrivateKey(fixtures.readKey('rsa_private_2048.pem'));
const publicKey = createPublicKey(privateKey);
const der = publicKey.export({ format: 'der', type: 'pkcs1' });
const pem = publicKey.export({ format: 'pem', type: 'pkcs1' });
const data = Buffer.from('PKCS#1 public key import');
const signature = sign('sha256', data, privateKey);

for (const [key, format] of [
  [der, 'der'],
  [Buffer.concat([der, Buffer.from('trailing data')]), 'der'],
  [pem, 'pem'],
  [`leading data\n${pem}trailing data\n`, 'pem'],
]) {
  const imported = createPublicKey({ key, format, type: 'pkcs1' });
  assert.strictEqual(imported.type, 'public');
  assert.strictEqual(imported.asymmetricKeyType, 'rsa');
  assert.deepStrictEqual(imported.asymmetricKeyDetails,
                         publicKey.asymmetricKeyDetails);
  assert.deepStrictEqual(imported.export({ format: 'der', type: 'pkcs1' }), der);
  assert.strictEqual(imported.export({ format: 'pem', type: 'pkcs1' }), pem);
  assert(verify('sha256', data, imported, signature));
}

// The public PKCS#1 decoder must reject truncated keys and other DER structures.
for (const invalid of [
  der.subarray(0, der.length - 1),
  publicKey.export({ format: 'der', type: 'spki' }),
]) {
  assert.throws(() => createPublicKey({
    key: invalid, format: 'der', type: 'pkcs1',
  }), { name: 'Error' });
  assert.throws(() => createPublicKey(
    `-----BEGIN RSA PUBLIC KEY-----\n${invalid.toString('base64')}\n` +
    '-----END RSA PUBLIC KEY-----\n',
  ), { name: 'Error' });
}

// Public-key creation continues to recognize PKCS#1 private keys separately.
const privateDer = privateKey.export({ format: 'der', type: 'pkcs1' });
assert.deepStrictEqual(createPublicKey({
  key: privateDer, format: 'der', type: 'pkcs1',
}).export({ format: 'der', type: 'pkcs1' }), der);

// Preserve the ASN.1 forms accepted by the legacy RSA BIGNUM decoder. These
// tiny keys exercise parsing only, without performing RSA operations.
if (!isBoringSSL) {
  for (const hex of [
    '30800201110201030000',  // Indefinite-length BER SEQUENCE.
    '300702020011020103',  // Redundant modulus padding.
    '300702810111020103',  // Non-minimal INTEGER length encoding.
    '30800201110201030000ffff',  // BER with trailing data.
  ]) {
    const imported = createPublicKey({
      key: Buffer.from(hex, 'hex'), format: 'der', type: 'pkcs1',
    });
    assert.strictEqual(imported.type, 'public');
    assert.strictEqual(imported.asymmetricKeyType, 'rsa');
  }

  // OpenSSL 4 rejects empty INTEGERs in both legacy and provider decoders.
  const emptyExponent = {
    key: Buffer.from('30050201110200', 'hex'), format: 'der', type: 'pkcs1',
  };
  if (hasOpenSSL(4)) {
    assert.throws(() => createPublicKey(emptyExponent), { name: 'Error' });
  } else {
    const imported = createPublicKey(emptyExponent);
    assert.strictEqual(imported.type, 'public');
    assert.strictEqual(imported.asymmetricKeyType, 'rsa');
  }
}

for (const hex of [
  '3080020111020103',  // Missing BER end-of-contents marker.
  '30800201110201030201010000',  // Third INTEGER inside BER SEQUENCE.
  '3006220111020103',  // Constructed INTEGER.
  '3006020111040103',  // OCTET STRING in place of the exponent.
  '3009020111020103020101',  // Third INTEGER inside DER SEQUENCE.
]) {
  assert.throws(() => createPublicKey({
    key: Buffer.from(hex, 'hex'), format: 'der', type: 'pkcs1',
  }), { name: 'Error' });
}
