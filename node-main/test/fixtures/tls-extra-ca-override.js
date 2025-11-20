'use strict';

// Test script for overidding NODE_EXTRA_CA_CERTS with tls.setDefaultCACertificates().

const tls = require('tls');
const assert = require('assert');
const { assertEqualCerts, includesCert } = require('../common/tls');

// Assert that NODE_EXTRA_CA_CERTS is set
assert(process.env.NODE_EXTRA_CA_CERTS, 'NODE_EXTRA_CA_CERTS environment variable should be set');

// Get initial state with extra CA
const initialDefaults = tls.getCACertificates('default');
const systemCerts = tls.getCACertificates('system');
const bundledCerts = tls.getCACertificates('bundled');
const extraCerts = tls.getCACertificates('extra');

// For this test to work the extra certs must not be in bundled certs
assert.notStrictEqual(bundledCerts.length, 0);
for (const cert of extraCerts) {
  assert(!includesCert(bundledCerts, cert));
}

// Test setting it to initial defaults.
tls.setDefaultCACertificates(initialDefaults);
assertEqualCerts(tls.getCACertificates('default'), initialDefaults);
assertEqualCerts(tls.getCACertificates('default'), initialDefaults);

// Test setting it to the bundled certificates.
tls.setDefaultCACertificates(bundledCerts);
assertEqualCerts(tls.getCACertificates('default'), bundledCerts);
assertEqualCerts(tls.getCACertificates('default'), bundledCerts);

// Test setting it to just the extra certificates.
tls.setDefaultCACertificates(extraCerts);
assertEqualCerts(tls.getCACertificates('default'), extraCerts);
assertEqualCerts(tls.getCACertificates('default'), extraCerts);

// Test setting it to an empty array.
tls.setDefaultCACertificates([]);
assert.deepStrictEqual(tls.getCACertificates('default'), []);

// Test bundled and extra certs are unaffected
assertEqualCerts(tls.getCACertificates('bundled'), bundledCerts);
assertEqualCerts(tls.getCACertificates('extra'), extraCerts);

if (systemCerts.length > 0) {
  // Test system certs are unaffected.
  assertEqualCerts(tls.getCACertificates('system'), systemCerts);
}
