// Flags: --tls-min-v1.3
'use strict';
const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

// Check that node `--tls-min-v1.3` is supported.

const assert = require('assert');
const tls = require('tls');

assert.strictEqual(tls.DEFAULT_MAX_VERSION, 'TLSv1.3');
assert.strictEqual(tls.DEFAULT_MIN_VERSION, 'TLSv1.3');

// Check the min-max version protocol versions against these CLI settings.
require('./test-tls-min-max-version.js');
