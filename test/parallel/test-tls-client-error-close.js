'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
}, common.mustNotCall());

server.on('tlsClientError', common.mustCall((err, socket) => {
  assert.match(err.code, /^ERR_SSL_/);
  assert(socket.destroyed);
  // Connection details must remain available while handling the error.
  assert.strictEqual(socket.remoteAddress, '127.0.0.1');
  assert.strictEqual(socket.remoteFamily, 'IPv4');
  assert(socket.remotePort > 0);
  socket.on('close', common.mustCall(() => {
    // The SSL state has been freed before close listeners run.
    assert.strictEqual(socket.getProtocol(), null);
    server.close(common.mustCall());
  }));
}));

server.listen(0, '127.0.0.1', common.mustCall(() => {
  const client = net.connect(server.address().port, '127.0.0.1');
  client.on('connect', common.mustCall(() => {
    // Reject the handshake without a TLS client.
    client.write(Buffer.alloc(64, 42));
  }));
  client.on('error', common.mustCallAtLeast((err) => {
    assert.strictEqual(err.code, 'ECONNRESET');
  }, 0));
  client.on('close', common.mustCall());
  client.resume();
}));
