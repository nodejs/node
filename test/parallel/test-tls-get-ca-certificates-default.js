'use strict';
// Test that tls.getCACertificates() returns the default certificates correctly.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { assertIsCAArray } = require('../common/tls');

const certs = tls.getCACertificates({ format: 'string' });
assertIsCAArray(certs);

const certs2 = tls.getCACertificates({ type: 'default', format: 'string' });
assert.deepStrictEqual(certs, certs2);

assert.deepStrictEqual(certs, tls.getCACertificates({ type: 'default', format: 'string' }));
