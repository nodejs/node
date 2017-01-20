'use strict';
// https://github.com/nodejs/node/issues/10223

require('../common');
const assert = require('assert');
const vm = require('vm');

const ctx = vm.createContext();
vm.runInContext('Object.defineProperty(this, "x", { value: 42 })', ctx);
assert.strictEqual(ctx.x, undefined);  // Not copied out by cloneProperty().
assert.strictEqual(vm.runInContext('x', ctx), 42);
vm.runInContext('x = 0', ctx);                      // Does not throw but x...
assert.strictEqual(vm.runInContext('x', ctx), 42);  // ...should be unaltered.
assert.throws(() => vm.runInContext('"use strict"; x = 0', ctx),
              /Cannot assign to read only property 'x'/);
assert.strictEqual(vm.runInContext('x', ctx), 42);
