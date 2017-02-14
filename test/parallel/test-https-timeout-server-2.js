'use strict';

const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const tls = require('tls');
const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
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
