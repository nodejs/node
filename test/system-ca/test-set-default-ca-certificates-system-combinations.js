// Flags: --use-system-ca

// This tests various combinations of CA certificates with
// tls.setDefaultCACertificates().

'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const tls = require('tls');
const { assertEqualCerts } = require('../common/tls');
const fixtures = require('../common/fixtures');

const fixtureCert = fixtures.readKey('fake-startcom-root-cert.pem');
const originalBundled = tls.getCACertificates('bundled');
const originalSystem = tls.getCACertificates('system');

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

  // Test system CA certificates are not affected.
  const systemCerts = tls.getCACertificates('system');
  assertEqualCerts(systemCerts, originalSystem);
}

// Test setting with fixture certificate.
testSetCertificates([fixtureCert]);

// Test setting with empty array.
testSetCertificates([]);

// Test setting with bundled certificates
testSetCertificates(originalBundled);

// Test setting with a subset of bundled certificates
if (originalBundled.length >= 3) {
  testSetCertificates(originalBundled.slice(0, 3));
}
