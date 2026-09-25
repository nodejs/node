'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the 'net.client.socket' diagnostics channel publishes
// a message when tls.connect() is used to create a socket connection.

const assert = require('assert');
const dc = require('diagnostics_channel');
const fixtures = require('../common/fixtures');
const tls = require('tls');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  rejectUnauthorized: false,
};

dc.subscribe('net.client.socket', common.mustCall(({ socket }) => {
  assert.strictEqual(socket instanceof tls.TLSSocket, true);
}));

const server = tls.createServer(options, common.mustCall((socket) => {
  socket.destroy();
  server.close();
}));

server.listen({ port: 0 }, common.mustCall(() => {
  const { port } = server.address();
  tls.connect(port, options);
}));
