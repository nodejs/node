'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('/ec-key.pem'),
  cert: fixtures.readKey('ec-cert.pem')
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
