'use strict';
// Flags: --use-system-ca

// This tests that tls.useSystemCA() is a no-op when
// --use-system-ca flag is already set.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

// Get initial state when --use-system-ca is already set
const initialDefaultCerts = tls.getCACertificates('default');
const systemCerts = tls.getCACertificates('system');

assert(Array.isArray(systemCerts));

// With --use-system-ca, default should already include system certs
const initialSystemSet = new Set(systemCerts);
const initialDefaultSet = new Set(initialDefaultCerts);
assert.deepStrictEqual(initialDefaultSet.intersection(initialSystemSet), initialSystemSet);

// Enable system CA certificates (should be a no-op)
tls.useSystemCA();

// Get certificates after calling useSystemCA
const newDefaultCerts = tls.getCACertificates('default');
const newSystemCerts = tls.getCACertificates('system');

// Everything should have the same content.
assert.deepStrictEqual(initialDefaultCerts, newDefaultCerts);
assert.deepStrictEqual(systemCerts, newSystemCerts);

// Multiple calls should still be no-ops
tls.useSystemCA();
tls.useSystemCA();
const stillSameDefaultCerts = tls.getCACertificates('default');
assert.deepStrictEqual(newDefaultCerts, stillSameDefaultCerts);
