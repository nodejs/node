'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { X509Certificate } = require('crypto');

function testX509Option() {
  const certs = tls.getCACertificates({ type: 'default', as: 'x509' });

  assert.ok(Array.isArray(certs), 'should return an array');
  assert.ok(certs.length > 0, 'should return non-empty array');
  assert.ok(certs[0] instanceof X509Certificate,
            'each cert should be instance of X509Certificate');

  assert.match(certs[0].serialNumber, /^[0-9A-F]+$/i, 'serialNumber should be hex string');
}

testX509Option();

console.log('Test passed: getCACertificates with as: "x509"');
