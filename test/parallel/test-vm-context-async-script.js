'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

const sandbox = { setTimeout };

const ctx = vm.createContext(sandbox);

vm.runInContext('setTimeout(function() { x = 3; }, 0);', ctx);
setTimeout(common.mustCall(() => {
  assert.strictEqual(sandbox.x, 3);
  assert.strictEqual(ctx.x, 3);
}), 1);
