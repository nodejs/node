'use strict';

// Flags: --no-use-system-ca

// This tests that tls.useSystemCA() works correctly
// when the certificates from the README are installed on the system.
// To run this test, install the certificates as described in README.md

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { assertIsCAArray } = require('../common/tls');
const fixtures = require('../common/fixtures');

// Read the expected certificates that should be installed
const startcomRootCert = fixtures.readKey('fake-startcom-root-cert.pem', 'utf8');

// Get initial state.
const initialDefaultCerts = tls.getCACertificates('default');
const systemCerts = tls.getCACertificates('system');

// System certs should be a valid CA array and should contain our test certificates
assertIsCAArray(systemCerts);
assert(systemCerts.length > 0, 'System certificates should be available when test certs are installed');

assert(systemCerts.includes(startcomRootCert));
assert(!initialDefaultCerts.includes(startcomRootCert));

// Check that system certs are not included by default (without --use-system-ca)
const initialSystemSet = new Set(systemCerts);
const initialDefaultSet = new Set(initialDefaultCerts);
const initialIntersection = initialDefaultSet.intersection(initialSystemSet);

// The initial default should not contain all system certs
assert(initialIntersection.size < systemCerts.length, 'Default certs should not include all system certs initially');

// Enable system CA certificates
tls.useSystemCA();

// Get certificates after enabling system CAs
const newDefaultCerts = tls.getCACertificates('default');
const newSystemCerts = tls.getCACertificates('system');

// System certificates should have the same content
assert.deepStrictEqual(systemCerts, newSystemCerts);

// Default certificates should now include the system certificates
assert.notStrictEqual(initialDefaultCerts, newDefaultCerts, 'Default certs should change after enabling system CAs');

// The new default should be a superset of system certificates
assert(newDefaultCerts.length >= systemCerts.length, 'New default should include all system certs');
const newDefaultSet = new Set(newDefaultCerts);
const newSystemSet = new Set(systemCerts);
assert.deepStrictEqual(newDefaultSet.intersection(newSystemSet), newSystemSet);

// Verify that our test certificates are now in the default certs
assert(newDefaultCerts.includes(startcomRootCert));

// Calling useSystemCA again should be a no-op
tls.useSystemCA();
const sameDefaultCerts = tls.getCACertificates('default');
assert.deepStrictEqual(newDefaultCerts, sameDefaultCerts);
