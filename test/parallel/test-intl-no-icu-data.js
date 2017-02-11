// Flags: --icu-data-dir=test/fixtures/empty/
'use strict';
require('../common');
const assert = require('assert');

// No-op when ICU case mappings are unavailable.
assert.strictEqual('ç'.toLocaleUpperCase('el'), 'ç');
