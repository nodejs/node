'use strict';

require('../common');
const assert = require('assert');

assert.ok(Array.isArray(process.loadedModules));
assert.throws(() => process.loadedModules = 'foo', /^TypeError: Cannot set property loadedModules of #<process> which has only a getter$/);
