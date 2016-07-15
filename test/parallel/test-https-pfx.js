'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var pfx = fs.readFileSync(common.fixturesDir + '/test_cert.pfx');

var options = {
  host: '127.0.0.1',
  port: undefined,
  path: '/',
  pfx: pfx,
  passphrase: 'sample',
  requestCert: true,
  rejectUnauthorized: false
};

var server = https.createServer(options, function(req, res) {
  assert.equal(req.socket.authorized, false); // not a client cert
  assert.equal(req.socket.authorizationError, 'DEPTH_ZERO_SELF_SIGNED_CERT');
  res.writeHead(200);
  res.end('OK');
});

server.listen(0, options.host, common.mustCall(function() {
  options.port = this.address().port;

  https.get(options, common.mustCall(function(res) {
    var data = '';

    res.on('data', function(data_) { data += data_; });
    res.on('end', common.mustCall(function() {
      assert.strictEqual(data, 'OK');
      server.close();
    }));
  }));
}));
