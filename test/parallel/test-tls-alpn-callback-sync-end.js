'use strict';

// Ending a server TLSSocket synchronously from inside an ALPNCallback must
// finish the handshake and then shut the connection down cleanly, rather than
// dropping the underlying socket part way through it.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  ALPNCallback: common.mustCall(function({ protocols }) {
    this.end();
    return protocols[0];
  }),
});

server.on('tlsClientError', common.mustNotCall());
server.on('secureConnection', common.mustCall((socket) => {
  socket.on('error', common.mustNotCall());
}));

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    ALPNProtocols: ['a'],
    rejectUnauthorized: false,
  }, common.mustCall(() => {
    assert.strictEqual(client.alpnProtocol, 'a');
  }));

  // A clean close_notify, not a truncated connection.
  client.on('end', common.mustCall());
  client.on('close', common.mustCall((hadError) => {
    assert.strictEqual(hadError, false);
    server.close();
  }));
  client.on('error', common.mustNotCall());
}));
