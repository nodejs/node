'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

// This test verifies that when a server receives an unknownProtocol it will
// not leave the socket open if the client does not close it.

if (!common.hasCrypto)
  common.skip('missing crypto');

const h2 = require('http2');
const tls = require('tls');

const server = h2.createSecureServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
  unknownProtocolTimeout: 500,
  allowHalfOpen: true
});

server.on('connection', (socket) => {
  socket.on('close', common.mustCall(() => {
    server.close();
  }));
});

server.listen(0, function() {
  tls.connect({
    port: server.address().port,
    rejectUnauthorized: false,
    ALPNProtocols: ['bogus']
  });
});
