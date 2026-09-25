'use strict';
// https://github.com/nodejs/node/issues/12300

require('../common');
const assert = require('assert');
const vm = require('vm');

const ctx = vm.createContext({ x: 42 });

// This might look as if x has not been declared, but x is defined on the
// sandbox and the assignment should not throw.
vm.runInContext('"use strict"; x = 1', ctx);

assert.strictEqual(ctx.x, 1);
