'use strict';

// Writing to a server TLSSocket synchronously from inside an ALPNCallback,
// which the TLS library invokes on its own stack mid-handshake, must not break
// the connection; the data must be delivered once the handshake ends.

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
    // The write cannot complete until the handshake does, but it must be
    // accepted and eventually flushed rather than dropped or encrypted into
    // the middle of the handshake.
    this.write('from-mid-handshake', common.mustCall());
    return protocols[0];
  }),
});

server.on('tlsClientError', common.mustNotCall());
server.on('secureConnection', common.mustCall((socket) => {
  assert.strictEqual(socket.alpnProtocol, 'a');
  socket.on('error', common.mustNotCall());
}));

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    ALPNProtocols: ['a', 'b'],
    rejectUnauthorized: false,
  }, common.mustCall(() => {
    assert.strictEqual(client.alpnProtocol, 'a');

    client.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'from-mid-handshake');
      client.end();
      server.close();
    }));
  }));
  client.on('error', common.mustNotCall());
}));
