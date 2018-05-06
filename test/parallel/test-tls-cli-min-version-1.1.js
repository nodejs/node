// Flags: --tls-v1.1
'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

// Check that node `--tls-v1.1` is supported.

const assert = require('assert');
const tls = require('tls');

assert.strictEqual(tls.DEFAULT_MAX_VERSION, 'TLSv1.2');
assert.strictEqual(tls.DEFAULT_MIN_VERSION, 'TLSv1.1');

// Check the min-max version protocol versions against these CLI settings.
require('./test-tls-min-max-version.js');
