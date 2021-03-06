'use strict';

// Test asynchronous SNI+OCSP on TLSSocket created with `server` set to
// `net.Server` instead of `tls.Server`

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

const server = net.createServer(common.mustCall((s) => {
  const tlsSocket = new tls.TLSSocket(s, {
    isServer: true,
    server: server,

    secureContext: tls.createSecureContext({ key, cert }),

    SNICallback: common.mustCall((hostname, callback) => {
      assert.strictEqual(hostname, 'test.test');

      callback(null, null);
    })
  });

  tlsSocket.on('secure', common.mustCall(() => {
    tlsSocket.end();
    server.close();
  }));
})).listen(0, () => {
  const opts = {
    servername: 'test.test',
    port: server.address().port,
    rejectUnauthorized: false,
    requestOCSP: true
  };

  tls.connect(opts, function() {
    this.end();
  });
});
