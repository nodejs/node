'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

common.expectWarning('ExperimentalWarning',
                     'vm.measureMemory is an experimental feature. ' +
                     'This feature could change at any time');

// The formats could change when V8 is updated, then the tests should be
// updated accordingly.
function assertSummaryShape(result) {
  assert.strictEqual(typeof result, 'object');
  assert.strictEqual(typeof result.total, 'object');
  assert.strictEqual(typeof result.total.jsMemoryEstimate, 'number');
  assert(Array.isArray(result.total.jsMemoryRange));
  assert.strictEqual(typeof result.total.jsMemoryRange[0], 'number');
  assert.strictEqual(typeof result.total.jsMemoryRange[1], 'number');
}

function assertDetailedShape(result) {
  // For now, the detailed shape is the same as the summary shape. This
  // should change in future versions of V8.
  return assertSummaryShape(result);
}

// Test measuring memory of the current context
{
  vm.measureMemory()
    .then(assertSummaryShape);

  vm.measureMemory({})
    .then(assertSummaryShape);

  vm.measureMemory({ mode: 'summary' })
    .then(assertSummaryShape);

  vm.measureMemory({ mode: 'detailed' })
    .then(assertDetailedShape);

  assert.throws(() => vm.measureMemory(null), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => vm.measureMemory('summary'), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => vm.measureMemory({ mode: 'random' }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
}

// Test measuring memory of the sandbox
{
  const context = vm.createContext();
  vm.measureMemory({ context })
    .then(assertSummaryShape);

  vm.measureMemory({ mode: 'summary', context },)
    .then(assertSummaryShape);

  vm.measureMemory({ mode: 'detailed', context })
    .then(assertDetailedShape);

  assert.throws(() => vm.measureMemory({ mode: 'summary', context: null }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => vm.measureMemory({ mode: 'summary', context: {} }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
}
