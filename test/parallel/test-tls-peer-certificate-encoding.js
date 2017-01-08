'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const util = require('util');
const join = require('path').join;

const options = {
  key: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent5-key.pem')),
  cert: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent5-cert.pem')),
  ca: [ fs.readFileSync(join(common.fixturesDir, 'keys', 'ca2-cert.pem')) ]
};

const server = tls.createServer(options, function(cleartext) {
  cleartext.end('World');
});
server.listen(0, common.mustCall(function() {
  const socket = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    const peerCert = socket.getPeerCertificate();

    console.error(util.inspect(peerCert));
    assert.strictEqual(peerCert.subject.CN, 'Ádám Lippai');
    server.close();
  }));
  socket.end('Hello');
}));
