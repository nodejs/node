'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const path = require('path');

const key = fs.readFileSync(path.join(common.fixturesDir, 'pass-key.pem'));
const cert = fs.readFileSync(path.join(common.fixturesDir, 'pass-cert.pem'));

const server = tls.Server({
  key: key,
  passphrase: 'passphrase',
  cert: cert,
  ca: [cert],
  requestCert: true,
  rejectUnauthorized: true
}, function(s) {
  s.end();
});

server.listen(0, common.mustCall(function() {
  const c = tls.connect({
    port: this.address().port,
    key: key,
    passphrase: 'passphrase',
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));
  c.on('end', function() {
    server.close();
  });
}));

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: key,
    passphrase: 'invalid',
    cert: cert,
    rejectUnauthorized: false
  });
});
