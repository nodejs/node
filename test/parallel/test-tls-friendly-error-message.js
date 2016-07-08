'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

var key = fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem');
var cert = fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem');

tls.createServer({ key: key, cert: cert }, function(conn) {
  conn.end();
  this.close();
}).listen(0, function() {
  var options = { port: this.address().port, rejectUnauthorized: true };
  tls.connect(options).on('error', common.mustCall(function(err) {
    assert.equal(err.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
    assert.equal(err.message, 'unable to verify the first certificate');
    this.destroy();
  }));
});
