'use strict';

const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');

const server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
  rejectUnauthorized: true
}, function(c) {
}).listen(0, common.mustCall(function() {
  const c = tls.connect({
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
