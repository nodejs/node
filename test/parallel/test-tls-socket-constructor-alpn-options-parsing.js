'use strict';

// Test that TLSSocket can take arrays of strings for ALPNProtocols.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

new tls.TLSSocket(null, {
  ALPNProtocols: ['http/1.1'],
});

const assert = require('assert');
const net = require('net');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

const server = net.createServer(common.mustCall((s) => {
  const tlsSocket = new tls.TLSSocket(s, {
    isServer: true,
    server,
    key,
    cert,
    ALPNProtocols: ['http/1.1'],
  });

  tlsSocket.on('secure', common.mustCall(() => {
    assert.strictEqual(tlsSocket.alpnProtocol, 'http/1.1');
    tlsSocket.end();
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const alpnOpts = {
    port: server.address().port,
    rejectUnauthorized: false,
    ALPNProtocols: ['h2', 'http/1.1']
  };

  tls.connect(alpnOpts, common.mustCall(function() {
    this.end();
  }));
}));
