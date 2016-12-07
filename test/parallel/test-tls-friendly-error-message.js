'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');

const key = fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem');
const cert = fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem');

tls.createServer({ key: key, cert: cert }, common.mustCall(function(conn) {
  conn.end();
  this.close();
})).listen(0, common.mustCall(function() {
  var options = { port: this.address().port, rejectUnauthorized: true };
  tls.connect(options).on('error', common.mustCall(function(err) {
    assert.strictEqual(err.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
    assert.strictEqual(err.message, 'unable to verify the first certificate');
    this.destroy();
  }));
}));
