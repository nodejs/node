'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = tls.createServer(options, common.mustCall(function(cleartext) {
  const s = cleartext.setTimeout(50, function() {
    cleartext.destroy();
    server.close();
  });
  assert.ok(s instanceof tls.TLSSocket);
}));

server.listen(0, common.mustCall(function() {
  tls.connect({
    host: '127.0.0.1',
    port: this.address().port,
    rejectUnauthorized: false
  });
}));
