'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');
if (!hasOpenSSL(3) || isBoringSSL)
  common.skip('requires OpenSSL 3 provider support');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const { createPublicKey } = require('crypto');

const publicKey = createPublicKey(
  fixtures.readKey('rsa_pss_public_2048_sha256_sha256_16.pem'));
const der = publicKey.export({ format: 'der', type: 'spki' });
const saltOffset = der.indexOf(Buffer.from([0xa2, 3, 2, 1, 16]));
assert.notStrictEqual(saltOffset, -1);

const publicDetails = { modulusLength: 2048, publicExponent: 65537n };
const restrictedDetails = {
  ...publicDetails,
  hashAlgorithm: 'sha256',
  mgf1HashAlgorithm: 'sha256',
  saltLength: 16,
};

function assertDetails(encoded, expected) {
  const key = createPublicKey({ key: encoded, format: 'der', type: 'spki' });
  assert.strictEqual(key.asymmetricKeyType, 'rsa-pss');
  assert.deepStrictEqual(key.asymmetricKeyDetails, expected);
}

assertDetails(der, restrictedDetails);

for (const saltLength of [0, 32, 127, -1, -128]) {
  const encoded = Buffer.from(der);
  encoded.writeInt8(saltLength, saltOffset + 4);
  assertDetails(encoded, saltLength < 0 ? publicDetails : {
    ...restrictedDetails,
    saltLength,
  });
}
