'use strict';

require('../common');
const assert = require('assert');

// Should be an invalid package path.
assert.throws(() => require('package.json'),
              { code: 'MODULE_NOT_FOUND' }
);
