'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const fixtures = require('../common/fixtures');

const privateKey = crypto.createPrivateKey(fixtures.readKey('rsa_private.pem', 'ascii'));
const publicKey = crypto.createPublicKey(fixtures.readKey('rsa_public.pem', 'ascii'));

const data = crypto.randomBytes(32);

for (const digest of ['sha256', 'sha384', 'sha512']) {
  const hLen = crypto.hash(digest, data, 'buffer').byteLength;
  const maxSaltLength =
      privateKey.asymmetricKeyDetails.modulusLength / 8 - hLen - 2;

  const sig = crypto.sign(digest, data, {
    key: privateKey,
    padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
    // No "saltLength" provided, documented default RSA_PSS_SALTLEN_MAX_SIGN expected
  });

  assert.strictEqual(crypto.verify(
    digest,
    data,
    {
      key: publicKey,
      padding: crypto.constants.RSA_PKCS1_PSS_PADDING,
      saltLength: maxSaltLength,
    },
    sig
  ), true);
}
