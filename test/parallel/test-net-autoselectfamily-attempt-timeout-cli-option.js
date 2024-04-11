'use strict';

// Flags: --network-family-autoselection-attempt-timeout=123

require('../common');

const assert = require('assert');
const { getDefaultAutoSelectFamilyAttemptTimeout } = require('net');

// Note that in test/common/index the default value is multiplied by 10.
assert.strictEqual(getDefaultAutoSelectFamilyAttemptTimeout(), 1230);
