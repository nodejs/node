// Flags: --icu-data-dir=test/fixtures/empty/
'use strict';
require('../common');
const assert = require('assert');
const config = process.binding('config');

assert.deepStrictEqual(Intl.NumberFormat.supportedLocalesOf('en'), []);
assert.strictEqual(config.icuDataDir, 'test/fixtures/empty/');
