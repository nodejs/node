'use strict';
// https://github.com/nodejs/node/issues/12300

require('../common');
const assert = require('assert');
const vm = require('vm');

const ctx = vm.createContext({ x: 42 });

// The following line wrongly throws an
// error because GlobalPropertySetterCallback()
// does not check if the property exists
// on the sandbox. It should just set x to 1
// instead of throwing an error.
vm.runInContext('"use strict"; x = 1', ctx);

assert.strictEqual(ctx.x, 1);
