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

const passKey = fs.readFileSync(path.join(common.fixturesDir, 'pass-key.pem'));
const rawKey = fs.readFileSync(path.join(common.fixturesDir, 'raw-key.pem'));
const cert = fs.readFileSync(path.join(common.fixturesDir, 'pass-cert.pem'));

assert(Buffer.isBuffer(passKey));
assert(Buffer.isBuffer(cert));
assert.strictEqual(typeof passKey.toString(), 'string');
assert.strictEqual(typeof cert.toString(), 'string');

const server = tls.Server({
  key: passKey,
  passphrase: 'passphrase',
  cert: cert,
  ca: [cert],
  requestCert: true,
  rejectUnauthorized: true
}, function(s) {
  s.end();
});

server.listen(0, common.mustCall(function() {
  // Buffer
  tls.connect({
    port: this.address().port,
    key: passKey,
    passphrase: 'passphrase',
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: rawKey,
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: rawKey,
    passphrase: 'passphrase', // Ignored.
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  // Buffer[]
  /* XXX(sam) Should work, but its unimplemented ATM.
  tls.connect({
    port: this.address().port,
    key: [passKey],
    passphrase: 'passphrase',
    cert: [cert],
    rejectUnauthorized: false
  }, common.mustCall(function() {}));
  */

  tls.connect({
    port: this.address().port,
    key: [rawKey],
    cert: [cert],
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: [rawKey],
    passphrase: 'passphrase', // Ignored.
    cert: [cert],
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  // string
  tls.connect({
    port: this.address().port,
    key: passKey.toString(),
    passphrase: 'passphrase',
    cert: cert.toString(),
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: rawKey.toString(),
    cert: cert.toString(),
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: rawKey.toString(),
    passphrase: 'passphrase', // Ignored.
    cert: cert.toString(),
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  // String[]
  /* XXX(sam) Should work, but its unimplemented ATM.
  tls.connect({
    port: this.address().port,
    key: [passKey.toString()],
    passphrase: 'passphrase',
    cert: [cert.toString()],
    rejectUnauthorized: false
  }, common.mustCall(function() {}));
  */

  tls.connect({
    port: this.address().port,
    key: [rawKey.toString()],
    cert: [cert.toString()],
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: [rawKey.toString()],
    passphrase: 'passphrase', // Ignored.
    cert: [cert.toString()],
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  // Object[]
  tls.connect({
    port: this.address().port,
    key: [{pem: passKey, passphrase: 'passphrase'}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: [{pem: passKey.toString(), passphrase: 'passphrase'}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey, passphrase: 'passphrase'}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey.toString(), passphrase: 'passphrase'}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  /* XXX(sam) Should work, but unimplemented ATM
  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey}],
    passphrase: 'passphrase',
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey.toString()}],
    passphrase: 'passphrase',
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey.toString()}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall(function() {}));
  */
})).unref();

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: passKey,
    passphrase: 'invalid',
    cert: cert,
    rejectUnauthorized: false
  });
}, /bad decrypt/);
