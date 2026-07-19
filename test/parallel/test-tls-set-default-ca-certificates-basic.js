'use strict';

// This tests the basic functionality of tls.setDefaultCACertificates().

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const tls = require('tls');
const fixtures = require('../common/fixtures');
const { assertEqualCerts } = require('../common/tls');

const originalBundled = tls.getCACertificates('bundled');
const originalSystem = tls.getCACertificates('system');
const fixtureCert = fixtures.readKey('fake-startcom-root-cert.pem');

function testSetCertificates(certs) {
  // Test setting it can be verified with tls.getCACertificates().
  tls.setDefaultCACertificates(certs);
  const result = tls.getCACertificates('default');
  assertEqualCerts(result, certs);

  // Verify that other certificate types are unchanged
  const newBundled = tls.getCACertificates('bundled');
  const newSystem = tls.getCACertificates('system');
  assertEqualCerts(newBundled, originalBundled);
  assertEqualCerts(newSystem, originalSystem);

  // Test implicit defaults.
  const implicitDefaults = tls.getCACertificates();
  assertEqualCerts(implicitDefaults, certs);

  // Test cached results.
  const cachedResult = tls.getCACertificates('default');
  assertEqualCerts(cachedResult, certs);
  const cachedImplicitDefaults = tls.getCACertificates();
  assertEqualCerts(cachedImplicitDefaults, certs);
}

// Test setting with fixture certificate.
testSetCertificates([fixtureCert]);

// Test setting with empty array.
testSetCertificates([]);

// Test setting with bundled certificates
testSetCertificates(originalBundled);

// Test combining bundled and extra certificates.
testSetCertificates([...originalBundled, fixtureCert]);

// Test setting with a subset of bundled certificates
if (originalBundled.length >= 3) {
  testSetCertificates(originalBundled.slice(0, 3));
}

// Test duplicate certificates
tls.setDefaultCACertificates([fixtureCert, fixtureCert, fixtureCert]);
assertEqualCerts(tls.getCACertificates('default'), [fixtureCert]);
