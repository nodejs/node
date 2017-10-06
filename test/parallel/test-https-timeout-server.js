'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const https = require('https');

const net = require('net');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  handshakeTimeout: 50
};

const server = https.createServer(options, common.mustNotCall());

server.on('clientError', common.mustCall(function(err, conn) {
  // Don't hesitate to update the asserts if the internal structure of
  // the cleartext object ever changes. We're checking that the https.Server
  // has closed the client connection.
  assert.strictEqual(conn._secureEstablished, false);
  server.close();
  conn.destroy();
}));

server.listen(0, function() {
  net.connect({ host: '127.0.0.1', port: this.address().port });
});
