'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

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
  return s.endsWith('\n-----END CERTIFICATE-----');
}));
