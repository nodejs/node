'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { connect, createServer } = require('tls');
const { createMockedLookup } = require('../common/dns');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');
const ca = fixtures.readKey('ca1-cert.pem');
const servername = 'agent1';

for (const autoSelectFamily of [false, true]) {
  const server = createServer({ key, cert }, common.mustCall((peer) => {
    assert.strictEqual(peer.servername, servername);
    peer.end();
    server.close();
  }));

  server.listen(0, '127.0.0.1', common.mustCall(() => {
    // The server only listens on IPv4. Falling back from IPv6 replaces the
    // client's TCP handle, which must preserve the TLS server name.
    const lookup = autoSelectFamily ?
      createMockedLookup('::1', '127.0.0.1') : createMockedLookup('127.0.0.1');
    const socket = connect({
      host: servername,
      port: server.address().port,
      servername,
      ca,
      autoSelectFamily,
      lookup,
    }, common.mustCall(() => socket.end()));
  }));
}
