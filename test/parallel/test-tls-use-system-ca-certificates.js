'use strict';

// Flags: --no-use-system-ca
// This tests that tls.useSystemCA() works correctly.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

// Get initial state
const initialDefaultCerts = tls.getCACertificates('default');
const systemCerts = tls.getCACertificates('system');

assert(Array.isArray(systemCerts));

// Check that system certs are not included by default (without --use-system-ca)
const initialSystemSet = new Set(systemCerts);
const initialDefaultSet = new Set(initialDefaultCerts);
const initialIntersection = initialDefaultSet.intersection(initialSystemSet);

// The initial default should not contain all system certs
// if there are system certs installed.
if (systemCerts.length > 0) {
  assert(initialIntersection.size < systemCerts.length);
}

// Enable system CA certificates.
tls.useSystemCA();

// Get certificates after enabling system CAs
const newDefaultCerts = tls.getCACertificates('default');
const newSystemCerts = tls.getCACertificates('system');

// System certificates should have the same content
assert.deepStrictEqual(systemCerts, newSystemCerts);

// Default certificates behavior depends on whether system certs exist
if (systemCerts.length > 0) {
  // The new default should be old default plus system certs
  const newDefaultSet = new Set(newDefaultCerts);
  const newSystemSet = new Set(systemCerts);
  assert.deepStrictEqual(newDefaultSet.intersection(initialDefaultSet), initialDefaultSet);
  assert.deepStrictEqual(newDefaultSet.intersection(newSystemSet), newSystemSet);
} else {
  // If no system certs, default certs should remain the same
  assert.deepStrictEqual(initialDefaultCerts, newDefaultCerts);
}

// Calling useSystemCA again should be a no-op
tls.useSystemCA();
const sameDefaultCerts = tls.getCACertificates('default');
assert.deepStrictEqual(newDefaultCerts, sameDefaultCerts);
