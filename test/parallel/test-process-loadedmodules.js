'use strict';

require('../common');
const assert = require('assert');


for (const mod of process.loadedModules) {
  assert.strictEqual(typeof mod, 'string');
}

assert.strictEqual(
  new Set(process.loadedModules).size,
  process.loadedModules.length
);

assert.ok(!process.loadedModules.includes('cluster'));
require('cluster');
assert.ok(process.loadedModules.includes('cluster'));

assert.ok(Array.isArray(process.loadedModules));
assert.throws(() => process.loadedModules = 'foo', /^TypeError: Cannot set property loadedModules of #<process> which has only a getter$/);
