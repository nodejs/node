// Flags: --tls-max-v1.3
'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

if (!require('constants').TLS1_3_VERSION)
  common.skip(`openssl ${process.versions.openssl} does not support TLSv1.3`);

// Confirm that for TLSv1.3, renegotiate() is disallowed.

const {
  assert, connect, keys
} = require(fixtures.path('tls-connect'));

const server = keys.agent10;

connect({
  client: {
    ca: server.ca,
    checkServerIdentity: common.mustCall(),
  },
  server: {
    key: server.key,
    cert: server.cert,
  },
}, function(err, pair, cleanup) {
  assert.ifError(err);

  const client = pair.client.conn;

  assert.strictEqual(client.getProtocol(), 'TLSv1.3');

  const ok = client.renegotiate({}, common.mustCall((err) => {
    assert(err.code, 'ERR_TLS_RENEGOTIATE');
    assert(err.message, 'Attempt to renegotiate TLS session failed');
    cleanup();
  }));

  assert.strictEqual(ok, false);
});
