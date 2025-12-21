'use strict';

// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const vm = require('vm');

const { internalBinding } = require('internal/test/binding');
const { isInsideNodeModules } = internalBinding('util');

const script = new vm.Script(`
  const runInsideNodeModules = (cb) => {
    return cb();
  };

  runInsideNodeModules;
`, {
  filename: '/workspace/node_modules/test.js',
});
const runInsideNodeModules = script.runInThisContext();

// Test when called directly inside node_modules
assert.strictEqual(runInsideNodeModules(isInsideNodeModules), true);

// Test when called inside a user callback from node_modules
runInsideNodeModules(common.mustCall(() => {
  function nonNodeModulesFunction() {
    assert.strictEqual(isInsideNodeModules(), false);
  }

  nonNodeModulesFunction();
}));

// Test when called outside node_modules
assert.strictEqual(isInsideNodeModules(), false);
