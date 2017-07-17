'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const util = require('util');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readSync('agent.key'),
  cert: fixtures.readSync('multi-alice.crt')
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
    assert.deepStrictEqual(
      peerCert.subject.OU,
      ['Information Technology', 'Engineering', 'Marketing']
    );
    server.close();
  }));
  socket.end('Hello');
}));
