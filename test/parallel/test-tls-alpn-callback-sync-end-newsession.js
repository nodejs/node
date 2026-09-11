'use strict';

// A shutdown deferred off the SSL library's stack must not run while the
// 'newSession' callback is still outstanding. Make sure it's deferred
// correctly so the connection still cleanly closes.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');
const { SSL_OP_NO_TICKET } = require('crypto').constants;

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  // The only config that consistently fires newSession on both OpenSSL &
  // BoringSSL is TLS v1.2 + session id resumption (tickets disabled):
  minVersion: 'TLSv1.2',
  maxVersion: 'TLSv1.2',
  secureOptions: SSL_OP_NO_TICKET,
  ALPNCallback: common.mustCall(function({ protocols }) {
    this.end();
    return protocols[0];
  }),
});

// Answering asynchronously holds EncOut() while the shutdown is replayed.
server.on('newSession', common.mustCall((id, data, callback) => {
  setImmediate(callback);
}));

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

  client.on('end', common.mustCall());
  client.on('close', common.mustCall((hadError) => {
    assert.strictEqual(hadError, false);
    server.close();
  }));
  client.on('error', common.mustNotCall());
}));
