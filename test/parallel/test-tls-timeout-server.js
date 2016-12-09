'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const net = require('net');
const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
  handshakeTimeout: 50
};

const server = tls.createServer(options, common.fail);

server.on('tlsClientError', common.mustCall(function(err, conn) {
  conn.destroy();
  server.close();
}));

server.listen(0, common.mustCall(function() {
  net.connect({ host: '127.0.0.1', port: this.address().port });
}));
