'use strict';

require('../common');

const { getAssetKeys } = require('node:sea');
const assert = require('node:assert');

// Test that getAssetKeys throws when not in SEA
assert.throws(() => getAssetKeys(), {
  code: 'ERR_NOT_IN_SINGLE_EXECUTABLE_APPLICATION'
});
