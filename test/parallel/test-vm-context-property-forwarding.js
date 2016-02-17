'use strict';
require('../common');
var assert = require('assert');
var vm = require('vm');

var sandbox = { x: 3 };

var ctx = vm.createContext(sandbox);

assert.strictEqual(vm.runInContext('x;', ctx), 3);
vm.runInContext('y = 4;', ctx);
assert.strictEqual(sandbox.y, 4);
assert.strictEqual(ctx.y, 4);
