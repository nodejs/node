'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const util = require('util');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent5-key.pem'),
  cert: fixtures.readKey('agent5-cert.pem'),
  ca: [ fixtures.readKey('ca2-cert.pem') ]
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
