'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/ec-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/ec-cert.pem')
};

const server = tls.createServer(options, function(conn) {
  conn.end('ok');
}).listen(0, common.mustCall(function() {
  const c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, common.mustCall(function() {
    c.on('end', common.mustCall(function() {
      c.end();
      server.close();
    }));

    c.on('data', function(data) {
      assert.strictEqual(data.toString(), 'ok');
    });

    const cert = c.getPeerCertificate();
    assert.strictEqual(cert.subject.C, 'US');
  }));
}));
