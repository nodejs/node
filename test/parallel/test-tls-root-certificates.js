'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const tls = require('tls');
const { fork } = require('child_process');

if (process.argv[2] !== 'child') {
  // Parent
  const NODE_EXTRA_CA_CERTS = fixtures.path('keys', 'ca1-cert.pem');

  fork(
    __filename,
    ['child'],
    { env: { ...process.env, NODE_EXTRA_CA_CERTS } }
  ).on('exit', common.mustCall(function(status) {
    assert.strictEqual(status, 0);
  }));
} else {
  // Child
  assert(Array.isArray(tls.rootCertificates));
  assert(tls.rootCertificates.length > 0);

  // Getter should return the same object.
  assert.strictEqual(tls.rootCertificates, tls.rootCertificates);

  // Array is immutable...
  assert.throws(() => tls.rootCertificates[0] = 0, /TypeError/);
  assert.throws(() => tls.rootCertificates.sort(), /TypeError/);

  // ...and so is the property.
  assert.throws(() => tls.rootCertificates = 0, /TypeError/);

  // Does not contain duplicates.
  assert.strictEqual(tls.rootCertificates.length,
                     new Set(tls.rootCertificates).size);

  assert(tls.rootCertificates.every((s) => {
    return s.startsWith('-----BEGIN CERTIFICATE-----\n');
  }));

  assert(tls.rootCertificates.every((s) => {
    return s.endsWith('\n-----END CERTIFICATE-----\n');
  }));

  const extraCert = fixtures.readKey('ca1-cert.pem', 'utf8');
  assert(tls.rootCertificates.includes(extraCert));
}
