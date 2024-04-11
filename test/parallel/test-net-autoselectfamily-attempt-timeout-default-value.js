'use strict';

require('../common');

const assert = require('assert');
const { getDefaultAutoSelectFamilyAttemptTimeout } = require('net');

// Note that in test/common/index the default value is multiplied by 10.
assert.strictEqual(getDefaultAutoSelectFamilyAttemptTimeout(), 2500);
