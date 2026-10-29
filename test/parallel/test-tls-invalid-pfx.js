'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');

const {
  assert, connect, keys
} = require(fixtures.path('tls-connect'));
const { hasFIPS } = require('../common/crypto');

const invalidPfx = fixtures.readKey('cert-without-key.pfx');

connect({
  client: {
    pfx: invalidPfx,
    passphrase: 'test',
    rejectUnauthorized: false
  },
  server: keys.agent1
}, common.mustCall((e, pair, cleanup) => {
  if (hasFIPS(3)) {
    assert.strictEqual(e.code, 'ERR_CRYPTO_UNSUPPORTED_OPERATION');
  } else {
    assert.strictEqual(e.message, 'Unable to load private key from PFX data');
  }
  cleanup();
}));
