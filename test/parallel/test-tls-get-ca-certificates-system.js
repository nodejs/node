'use strict';
// Flags: --use-system-ca
// Test that tls.getCACertificates() returns system certificates correctly.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { assertIsCAArray } = require('../common/tls');

const systemCerts = tls.getCACertificates({ type: 'system', format: 'string' });

if (common.isWindows) {
  assertIsCAArray(systemCerts);
} else {
  assert(Array.isArray(systemCerts));
}

const defaultCerts = tls.getCACertificates({ format: 'string' });
assert(defaultCerts.length >= systemCerts.length);
const defaultSet = new Set(defaultCerts);
const systemSet = new Set(systemCerts);
for (const cert of systemSet) {
  assert(defaultSet.has(cert));
}

assert.deepStrictEqual(systemCerts, tls.getCACertificates({ type: 'system', format: 'string' }));
