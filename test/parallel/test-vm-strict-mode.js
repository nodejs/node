'use strict';
// https://github.com/nodejs/node/issues/12300

require('../common');
const assert = require('assert');
const vm = require('vm');

const ctx = new vm.Context();
ctx.global.x = 42;

// This might look as if x has not been declared, but x is defined on the
// sandbox and the assignment should not throw.
vm.runInContext('"use strict"; x = 1', ctx);

assert.strictEqual(ctx.global.x, 1);
