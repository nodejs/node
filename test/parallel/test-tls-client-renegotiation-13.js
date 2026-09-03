'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const { hasOpenSSL, isBoringSSL } = require('../common/crypto');

const fixtures = require('../common/fixtures');

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
}, common.mustSucceed((pair, cleanup) => {
  const client = pair.client.conn;

  assert.strictEqual(client.getProtocol(), 'TLSv1.3');

  const ok = client.renegotiate({}, common.mustCall((err) => {
    if (isBoringSSL) {
      assert.throws(() => { throw err; }, {
        message: 'TLS session renegotiation is unsupported by this TLS ' +
                 'implementation',
        code: 'ERR_TLS_RENEGOTIATION_UNSUPPORTED',
      });
    } else {
      assert.throws(() => { throw err; }, {
        message: hasOpenSSL(3) ?
          'error:0A00010A:SSL routines::wrong ssl version' :
          'error:1420410A:SSL routines:SSL_renegotiate:wrong ssl version',
        code: 'ERR_SSL_WRONG_SSL_VERSION',
        library: 'SSL routines',
        reason: 'wrong ssl version',
      });
    }
    cleanup();
  }));

  assert.strictEqual(ok, false);
}));
