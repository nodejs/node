'use strict';

// Test that TLSSocket can take arrays of strings for ALPNProtocols and
// NPNProtocols.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

new tls.TLSSocket(null, {
  ALPNProtocols: ['http/1.1'],
  NPNProtocols: ['http/1.1']
});

if (!process.features.tls_npn)
  common.skip('node compiled without NPN feature of OpenSSL');

if (!process.features.tls_alpn)
  common.skip('node compiled without ALPN feature of OpenSSL');

const assert = require('assert');
const net = require('net');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

const protocols = [];

const server = net.createServer(common.mustCall((s) => {
  const tlsSocket = new tls.TLSSocket(s, {
    isServer: true,
    server,
    key,
    cert,
    ALPNProtocols: ['http/1.1'],
    NPNProtocols: ['http/1.1']
  });

  tlsSocket.on('secure', common.mustCall(() => {
    protocols.push({
      alpnProtocol: tlsSocket.alpnProtocol,
      npnProtocol: tlsSocket.npnProtocol
    });
    tlsSocket.end();
  }));
}, 2));

server.listen(0, common.mustCall(() => {
  const alpnOpts = {
    port: server.address().port,
    rejectUnauthorized: false,
    ALPNProtocols: ['h2', 'http/1.1']
  };
  const npnOpts = {
    port: server.address().port,
    rejectUnauthorized: false,
    NPNProtocols: ['h2', 'http/1.1']
  };

  tls.connect(alpnOpts, function() {
    this.end();

    tls.connect(npnOpts, function() {
      this.end();

      server.close();

      assert.deepStrictEqual(protocols, [
        { alpnProtocol: 'http/1.1', npnProtocol: false },
        { alpnProtocol: false, npnProtocol: 'http/1.1' }
      ]);
    });
  });
}));
