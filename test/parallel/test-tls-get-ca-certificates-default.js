'use strict';

// This tests that tls.getCACertificates() returns the default
// certificates correctly.

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const { assertIsCAArray } = require('../common/tls');

const certs = tls.getCACertificates();
assertIsCAArray(certs);

// It's cached on subsequent accesses.
assert.strictEqual(certs, tls.getCACertificates('default'));
