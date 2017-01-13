'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');
const ctx = vm.createContext();

// Test strict mode inside a vm script, i.e., using an undefined variable
// throws a ReferenceError. Also check that variables
// that are not successfully set in the vm, must not be set
// on the sandboxed context.

vm.runInContext('w = 1;', ctx);
assert.strictEqual(1, ctx.w);

assert.throws(function() { vm.runInContext('"use strict"; x = 1;', ctx); },
              /ReferenceError: x is not defined/);
assert.strictEqual(undefined, ctx.x);

vm.runInContext('"use strict"; var y = 1;', ctx);
assert.strictEqual(1, ctx.y);

vm.runInContext('"use strict"; this.z = 1;', ctx);
assert.strictEqual(1, ctx.z);

// w has been defined
vm.runInContext('"use strict"; w = 2;', ctx);
assert.strictEqual(2, ctx.w);
