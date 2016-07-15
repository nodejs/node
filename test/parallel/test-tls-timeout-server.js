'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var net = require('net');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
  handshakeTimeout: 50
};

var server = tls.createServer(options, common.fail);

server.on('tlsClientError', common.mustCall(function(err, conn) {
  conn.destroy();
  server.close();
}));

server.listen(0, function() {
  net.connect({ host: '127.0.0.1', port: this.address().port });
});
