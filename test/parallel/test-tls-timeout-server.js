'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const net = require('net');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
  handshakeTimeout: 50
};

const server = tls.createServer(options, common.mustNotCall());

server.on('tlsClientError', common.mustCall(function(err, conn) {
  conn.destroy();
  server.close();
}));

server.listen(0, common.mustCall(function() {
  net.connect({ host: '127.0.0.1', port: this.address().port });
}));
