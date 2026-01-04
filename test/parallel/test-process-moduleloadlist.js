'use strict';

require('../common');
const assert = require('assert');

assert.ok(Array.isArray(process.moduleLoadList));
assert.throws(() => process.moduleLoadList = 'foo', /^TypeError: Cannot set property moduleLoadList of #<process> which has only a getter$/);
