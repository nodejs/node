'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

const sandbox = { x: 3 };

const ctx = vm.createContext(sandbox);

assert.strictEqual(vm.runInContext('x;', ctx), 3);
vm.runInContext('y = 4;', ctx);
assert.strictEqual(sandbox.y, 4);
assert.strictEqual(ctx.y, 4);
