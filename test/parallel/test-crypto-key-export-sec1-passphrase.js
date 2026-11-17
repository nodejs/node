'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasFIPS, isBoringSSL } = require('../common/crypto');

if (isBoringSSL)
  common.skip('BoringSSL does not use OpenSSL provider encoders');
if (hasFIPS(3))
  common.skip('SEC1 private key encoding is not supported in FIPS mode');

const assert = require('assert');
const {
  createPrivateKey,
  generateKeyPairSync,
} = require('crypto');

const { privateKey } = generateKeyPairSync('ec', {
  namedCurve: 'prime256v1',
});

for (const passphrase of [
  Buffer.alloc(0),
  Buffer.from([0x62, 0x69, 0x00, 0x6e, 0x61, 0x72, 0x79]),
]) {
  const pem = privateKey.export({
    type: 'sec1',
    format: 'pem',
    cipher: 'aes-128-cbc',
    passphrase,
  });
  assert.match(pem.toString(), /^-----BEGIN EC PRIVATE KEY-----\n/);
  assert.strictEqual(createPrivateKey({ key: pem, passphrase })
    .asymmetricKeyType, 'ec');
  // A wrong CBC password can occasionally yield valid padding, in which case
  // ASN.1 decoding fails instead.
  assert.throws(() => createPrivateKey({
    key: pem,
    passphrase: Buffer.from('wrong'),
  }), { code: /^ERR_OSSL_(BAD_DECRYPT|UNSUPPORTED)$/ });
  assert.throws(() => createPrivateKey({ key: pem }), {
    code: 'ERR_OSSL_CRYPTO_INTERRUPTED_OR_CANCELLED',
  });
}
