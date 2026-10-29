'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { isBoringSSL } = require('../common/crypto');

if (isBoringSSL) {
  common.skip('OpenSSL legacy failures are not testable with BoringSSL');
}

const fixtures = require('../common/fixtures');

const {
  assert, connect, keys
} = require(fixtures.path('tls-connect'));

const legacyPfx = fixtures.readKey('legacy.pfx');

connect({
  client: {
    pfx: legacyPfx,
    passphrase: 'legacy',
    rejectUnauthorized: false
  },
  server: keys.agent1
}, common.mustCall((e, pair, cleanup) => {
  assert.strictEqual(e.code, 'ERR_CRYPTO_UNSUPPORTED_OPERATION');
  assert.strictEqual(e.message, 'Unsupported PKCS12 PFX data');
  cleanup();
}));
