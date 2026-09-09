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

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  // TLSv1.3 issues its session tickets after the handshake, which is what puts
  // the 'newSession' callback and the deferred shutdown in the same window.
  minVersion: 'TLSv1.3',
  maxVersion: 'TLSv1.3',
  ALPNCallback: common.mustCall(function({ protocols }) {
    this.end();
    return protocols[0];
  }),
});

// Answering asynchronously holds EncOut() while the shutdown is replayed.
server.on('newSession', common.mustCallAtLeast((id, data, callback) => {
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
    minVersion: 'TLSv1.3',
    maxVersion: 'TLSv1.3',
  }, common.mustCall(() => {
    assert.strictEqual(client.alpnProtocol, 'a');
  }));

  // The tickets have to survive the shutdown, not be cut off by the FIN.
  client.on('session', common.mustCallAtLeast());
  client.on('close', common.mustCall((hadError) => {
    assert.strictEqual(hadError, false);
    server.close();
  }));
  client.on('error', common.mustNotCall());
}));
