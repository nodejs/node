'use strict';

require('../common');
const assert = require('assert');

// Verify that eval is allowed by default.
assert.strictEqual(eval('"eval"'), 'eval');
