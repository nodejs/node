'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

const ctx = vm.createContext({ x: 0 });

// Global properties should be intercepted in strict mode
vm.runInContext('"use strict"; x = 42', ctx);
assert.strictEqual(ctx.x, 42);

// Contextual store should only be intercepted in non-strict mode
vm.runInContext('y = 42', ctx);
assert.strictEqual(ctx.y, 42);

assert.throws(() => vm.runInContext('"use strict"; z = 42', ctx),
              /ReferenceError: z is not defined/);
assert.strictEqual(Object.hasOwn(ctx, 'z'), false);
