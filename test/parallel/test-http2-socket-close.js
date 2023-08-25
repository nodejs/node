'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const net = require('net');
const h2 = require('http2');

const tlsOptions = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  ALPNProtocols: ['h2']
};

// Create a net server that upgrades sockets to HTTP/2 manually, but with two
// different shutdown timeouts: a short socket timeout, and a longer H2 session timeout.
// Since the only request is complete, the session should shutdown cleanly when the
// socket shuts down (and should _not_ segfault, as it does in Node v20.5.1)

const netServer = net.createServer((socket) => {
  setTimeout(() => {
    socket.destroy();
  }, 10);

  h2Server.emit('connection', socket);
});

const h2Server = h2.createSecureServer(tlsOptions, (req, res) => {
  res.writeHead(200);
  res.end();
});

h2Server.on('session', (session) => {
  setTimeout(() => {
    session.close();
  }, 20);
});

netServer.listen(0, common.mustCall(() => {
  const proxyClient = h2.connect(`https://localhost:${netServer.address().port}`, {
    rejectUnauthorized: false
  });

  proxyClient.on('close', common.mustCall(() => {
    netServer.close();
  }));

  const req = proxyClient.request({
    ':method': 'GET',
    ':path': '/'
  });

  req.on('response', common.mustCall((response) => {
    assert.strictEqual(response[':status'], 200);
  }));
}));
