'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { hasOpenSSL3 } = require('../common/crypto');

if (hasOpenSSL3 && !process.features.openssl_is_boringssl) {
  common.skip('this test requires a build without EVP_MAC support');
}

const assert = require('node:assert');
const crypto = require('node:crypto');

const algorithm = 'hmac';
const options = { digest: 'sha256' };
const key = Buffer.from('key');

assert.strictEqual(typeof crypto.createMac, 'function');
assert.strictEqual(typeof crypto.getMacs, 'function');
assert.strictEqual(crypto.Mac, undefined);
assert.deepStrictEqual(crypto.getMacs(), []);
assert.throws(() => crypto.createMac(algorithm, key, options), {
  code: 'ERR_CRYPTO_MAC_NOT_SUPPORTED',
});
(async () => {
  const esmCrypto = await import('node:crypto');
  assert.strictEqual(esmCrypto.createMac, crypto.createMac);
  assert.strictEqual(esmCrypto.getMacs, crypto.getMacs);
  assert.strictEqual(esmCrypto.Mac, undefined);
})().then(common.mustCall());
