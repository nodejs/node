'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const net = require('net');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent2-key.pem');
const cert = fixtures.readKey('agent2-cert.pem');

let serverTlsSocket;
const tlsServer = tls.createServer({ cert, key }, (socket) => {
  serverTlsSocket = socket;
});

// A plain net server, that manually passes connections to the TLS
// server to be upgraded
let netSocket;
const netServer = net.createServer((socket) => {
  tlsServer.emit('connection', socket);

  netSocket = socket;
}).listen(0, common.mustCall(function() {
  connectClient(netServer);
}));

// A client that connects, sends one message, and closes the raw connection:
function connectClient(server) {
  const clientTlsSocket = tls.connect({
    host: 'localhost',
    port: server.address().port,
    rejectUnauthorized: false
  });

  clientTlsSocket.write('foo', 'utf8', common.mustCall(() => {
    assert(netSocket);
    netSocket.setTimeout(common.platformTimeout(10), common.mustCall(() => {
      assert(serverTlsSocket);

      netSocket.destroy();

      setImmediate(() => {
        assert.strictEqual(netSocket.destroyed, true);

        setImmediate(() => {
          assert.strictEqual(clientTlsSocket.destroyed, true);
          assert.strictEqual(serverTlsSocket.destroyed, true);

          tlsServer.close();
          netServer.close();
        });
      });
    }));
  }));
}
