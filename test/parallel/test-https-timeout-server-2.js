'use strict';

var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var net = require('net');
var tls = require('tls');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = https.createServer(options, assert.fail);

server.on('secureConnection', function(cleartext) {
  var s = cleartext.setTimeout(50, function() {
    cleartext.destroy();
    server.close();
  });
  assert.ok(s instanceof tls.TLSSocket);
});

server.listen(common.PORT, function() {
  tls.connect({
    host: '127.0.0.1',
    port: common.PORT,
    rejectUnauthorized: false
  });
});
