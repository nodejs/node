// Flags: --icu-data-dir=test/fixtures/empty/
'use strict';
require('../common');
const assert = require('assert');
const config = process.binding('config');

// No-op when ICU case mappings are unavailable.
assert.strictEqual('ç'.toLocaleUpperCase('el'), 'ç');
assert.strictEqual(config.icuDataDir, 'test/fixtures/empty/');
