'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');

const {
  assert, connect, keys
} = require(fixtures.path('tls-connect'));

const invalidPfx = fixtures.readKey('cert-without-key.pfx');

connect({
  client: {
    pfx: invalidPfx,
    passphrase: 'test',
    rejectUnauthorized: false
  },
  server: keys.agent1
}, common.mustCall((e, pair, cleanup) => {
  assert.strictEqual(e.message, 'Unable to load private key from PFX data');
  cleanup();
}));
