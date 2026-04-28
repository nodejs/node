'use strict';
const common = require('../common');
const {
  assertDetailedShape,
  expectExperimentalWarning
} = require('../common/measure-memory');
const vm = require('vm');
const assert = require('assert');

expectExperimentalWarning();
{
  const arr = [];
  const count = 10;
  for (let i = 0; i < count; ++i) {
    const context = vm.createContext({
      test: new Array(100).fill('foo')
    });
    arr.push(context);
  }
  // Check that one more context shows up in the result
  vm.measureMemory({ mode: 'detailed', execution: 'eager' })
    .then(common.mustCall((result) => {
      // We must hold on to the contexts here so that they
      // don't get GC'ed until the measurement is complete
      assert.strictEqual(arr.length, count);
      assertDetailedShape(result, count + common.isWindows);
    }));
}
