'use strict';
// Flags: --use-system-ca
// This tests that tls.getCACertificates() returns the system
// certificates correctly.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { assertIsCAArray } = require('../common/tls');

const systemCerts = tls.getCACertificates('system');
assertIsCAArray(systemCerts);

// When --use-system-ca is true, default is a superset of system
// certificates.
const defaultCerts = tls.getCACertificates('default');
assert(defaultCerts.length >= systemCerts.length);
const defaultSet = new Set(defaultCerts);
const systemSet = new Set(systemCerts);
assert.deepStrictEqual(defaultSet.intersection(systemSet), systemSet);

// It's cached on subsequent accesses.
assert.strictEqual(systemCerts, tls.getCACertificates('system'));
