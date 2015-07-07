'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

var hadTimeout = false;

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = tls.Server(options, function(socket) {
  var s = socket.setTimeout(100);
  assert.ok(s instanceof tls.TLSSocket);

  socket.on('timeout', function(err) {
    hadTimeout = true;
    socket.end();
    server.close();
  });
});

server.listen(common.PORT, function() {
  var socket = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  });
});

process.on('exit', function() {
  assert.ok(hadTimeout);
});
