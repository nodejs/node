'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const net = require('net');
const tls = require('tls');
const h2 = require('http2');

// This test sets up an H2 proxy server, and tunnels a request over one of its streams
// back to itself, via TLS, and then closes the TLS connection. On some Node versions
// (v18 & v20 up to 20.5.1) the resulting JS Stream Socket fails to shutdown correctly
// in this case, and crashes due to a null pointer in finishShutdown.

const tlsOptions = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  ALPNProtocols: ['h2']
};

const netServer = net.createServer((socket) => {
  socket.allowHalfOpen = false;
  // ^ This allows us to trigger this reliably, but it's not strictly required
  // for the bug and crash to happen, skipping this just fails elsewhere later.

  h2Server.emit('connection', socket);
});

const h2Server = h2.createSecureServer(tlsOptions, (req, res) => {
  res.writeHead(200);
  res.end();
});

h2Server.on('connect', (req, res) => {
  res.writeHead(200, {});
  netServer.emit('connection', res.stream);
});

netServer.listen(0, common.mustCall(() => {
  const proxyClient = h2.connect(`https://localhost:${netServer.address().port}`, {
    rejectUnauthorized: false
  });

  const proxyReq = proxyClient.request({
    ':method': 'CONNECT',
    ':authority': 'example.com:443'
  });

  proxyReq.on('response', common.mustCall((response) => {
    assert.strictEqual(response[':status'], 200);

    // Create a TLS socket within the tunnel, and start sending a request:
    const tlsSocket = tls.connect({
      socket: proxyReq,
      ALPNProtocols: ['h2'],
      rejectUnauthorized: false
    });

    proxyReq.on('close', common.mustCall(() => {
      proxyClient.close();
      netServer.close();
    }));

    // Forcibly kill the TLS socket
    tlsSocket.destroy();

    // This results in an async error in affected Node versions, before the 'close' event
  }));
}));
