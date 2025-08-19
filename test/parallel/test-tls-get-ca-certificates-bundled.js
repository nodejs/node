'use strict';
// Test that tls.getCACertificates() returns the bundled certificates correctly.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { assertIsCAArray } = require('../common/tls');

const certs = tls.getCACertificates({ type: 'bundled', format: 'string' });
assertIsCAArray(certs);

assert.deepStrictEqual(certs, tls.rootCertificates);

assert.deepStrictEqual(certs, tls.getCACertificates({ type: 'bundled', format: 'string' }));
