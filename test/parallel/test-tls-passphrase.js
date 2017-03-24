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
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: rawKey,
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: rawKey,
    passphrase: 'ignored',
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  // Buffer[]
  tls.connect({
    port: this.address().port,
    key: [passKey],
    passphrase: 'passphrase',
    cert: [cert],
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [rawKey],
    cert: [cert],
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [rawKey],
    passphrase: 'ignored',
    cert: [cert],
    rejectUnauthorized: false
  }, common.mustCall());

  // string
  tls.connect({
    port: this.address().port,
    key: passKey.toString(),
    passphrase: 'passphrase',
    cert: cert.toString(),
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: rawKey.toString(),
    cert: cert.toString(),
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: rawKey.toString(),
    passphrase: 'ignored',
    cert: cert.toString(),
    rejectUnauthorized: false
  }, common.mustCall());

  // String[]
  tls.connect({
    port: this.address().port,
    key: [passKey.toString()],
    passphrase: 'passphrase',
    cert: [cert.toString()],
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [rawKey.toString()],
    cert: [cert.toString()],
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [rawKey.toString()],
    passphrase: 'ignored',
    cert: [cert.toString()],
    rejectUnauthorized: false
  }, common.mustCall());

  // Object[]
  tls.connect({
    port: this.address().port,
    key: [{pem: passKey, passphrase: 'passphrase'}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [{pem: passKey, passphrase: 'passphrase'}],
    passphrase: 'ignored',
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [{pem: passKey}],
    passphrase: 'passphrase',
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [{pem: passKey.toString(), passphrase: 'passphrase'}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey, passphrase: 'ignored'}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey.toString(), passphrase: 'ignored'}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey}],
    passphrase: 'ignored',
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey.toString()}],
    passphrase: 'ignored',
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());

  tls.connect({
    port: this.address().port,
    key: [{pem: rawKey.toString()}],
    cert: cert,
    rejectUnauthorized: false
  }, common.mustCall());
})).unref();

const errMessagePassword = /bad password read/;

// Missing passphrase
assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: passKey,
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessagePassword);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [passKey],
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessagePassword);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [{pem: passKey}],
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessagePassword);

const errMessageDecrypt = /bad decrypt/;

// Invalid passphrase
assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: passKey,
    passphrase: 'invalid',
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessageDecrypt);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [passKey],
    passphrase: 'invalid',
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessageDecrypt);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [{pem: passKey}],
    passphrase: 'invalid',
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessageDecrypt);

assert.throws(function() {
  tls.connect({
    port: server.address().port,
    key: [{pem: passKey, passphrase: 'invalid'}],
    passphrase: 'passphrase', // Valid but unused
    cert: cert,
    rejectUnauthorized: false
  });
}, errMessageDecrypt);
