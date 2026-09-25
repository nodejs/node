'use strict';

// Writing to a server TLSSocket synchronously from inside an SNICallback,
// while the handshake is still waiting on the certificate callback, must not
// break the connection; the data must be delivered once the handshake ends.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const net = require('net');
const tls = require('tls');

const secureContext = tls.createSecureContext({
  key: fixtures.readKey('rsa_private.pem'),
  cert: fixtures.readKey('rsa_cert.crt'),
});

let serverSocket;
const server = net.createServer(common.mustCall((raw) => {
  serverSocket = new tls.TLSSocket(raw, {
    isServer: true,
    secureContext,
    SNICallback: common.mustCall((servername, callback) => {
      assert.strictEqual(servername, 'localhost');
      serverSocket.write('from-mid-handshake');
      callback(null, null);
    }),
  });
  serverSocket.on('error', common.mustNotCall());
}));

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    servername: 'localhost',
    rejectUnauthorized: false,
  }, common.mustCall(() => {
    client.on('data', common.mustCall((data) => {
      assert.strictEqual(data.toString(), 'from-mid-handshake');
      client.end();
      server.close();
    }));
  }));
  client.on('error', common.mustNotCall());
}));
