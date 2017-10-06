'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const https = require('https');
const tls = require('tls');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = https.createServer(options, common.mustNotCall());

server.on('secureConnection', function(cleartext) {
  const s = cleartext.setTimeout(50, function() {
    cleartext.destroy();
    server.close();
  });
  assert.ok(s instanceof tls.TLSSocket);
});

server.listen(0, function() {
  tls.connect({
    host: '127.0.0.1',
    port: this.address().port,
    rejectUnauthorized: false
  });
});
