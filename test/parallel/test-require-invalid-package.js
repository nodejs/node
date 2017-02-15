'use strict';

const common = require('../common');
const assert = require('assert');

// Should be an invalid package path.
assert.throws(() => require('package.json'),
              common.expectsError('MODULE_NOT_FOUND')
);
