'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var https = require('https');

var pfx = fs.readFileSync(common.fixturesDir + '/test_cert.pfx');

var options = {
  host: '127.0.0.1',
  port: common.PORT,
  path: '/',
  pfx: pfx,
  passphrase: 'sample',
  requestCert: true,
  rejectUnauthorized: false
};

var options1 = {
  host: '127.0.0.1',
  port: common.PORT,
  path: '/',
  pfx: pfx,
  passphrase: 'sample',
  requestCert: true
};

var server = https.createServer(options, function(req, res) {
  assert.equal(req.socket.authorized, false); // not a client cert
  assert.equal(req.socket.authorizationError, 'DEPTH_ZERO_SELF_SIGNED_CERT');
  res.writeHead(200);
  res.end('OK');
});

assert.doesNotThrow(() => https.createServer(options1, assert.fail),
 'cert is a required parameter for Server.createServer');

assert.doesNotThrow(() => https.createServer(options1, assert.fail),
  'key is a required parameter for Server.createServer');

server.listen(options.port, options.host, function() {
  var data = '';

  https.get(options, function(res) {
    res.on('data', function(data_) { data += data_; });
    res.on('end', function() { server.close(); });
  });

  process.on('exit', function() {
    assert.equal(data, 'OK');
  });
});
