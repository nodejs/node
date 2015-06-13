'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var net = require('net');
var fs = require('fs');

var clientErrors = 0;

process.on('exit', function() {
  assert.equal(clientErrors, 1);
});

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
  handshakeTimeout: 50
};

var server = tls.createServer(options, assert.fail);

server.on('clientError', function(err, conn) {
  conn.destroy();
  server.close();
  clientErrors++;
});

server.listen(common.PORT, function() {
  net.connect({ host: '127.0.0.1', port: common.PORT });
});
