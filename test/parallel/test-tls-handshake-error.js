'use strict';

const common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
  rejectUnauthorized: true
}, function(c) {
}).listen(0, common.mustCall(function() {
  var c = tls.connect({
    port: this.address().port,
    ciphers: 'RC4'
  }, function() {
    assert(false, 'should not be called');
  });

  c.on('error', common.mustCall(function(err) {
    assert.notEqual(err.code, 'ECONNRESET');
  }));

  c.on('close', common.mustCall(function(err) {
    assert.ok(err);
    server.close();
  }));
}));
