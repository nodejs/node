'use strict';
require('../common');
const assert = require('assert');

const vm = require('vm');

// https://github.com/nodejs/node/issues/10223
const ctx = vm.createContext();

// Define x with writable = false.
vm.runInContext('Object.defineProperty(this, "x", { value: 42 })', ctx);
assert.strictEqual(ctx.x, 42);
assert.strictEqual(vm.runInContext('x', ctx), 42);

vm.runInContext('x = 0', ctx);                      // Does not throw but x...
assert.strictEqual(vm.runInContext('x', ctx), 42);  // ...should be unaltered.

assert.throws(() => vm.runInContext('"use strict"; x = 0', ctx),
              /Cannot assign to read only property 'x'/);
assert.strictEqual(vm.runInContext('x', ctx), 42);
