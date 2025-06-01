'use strict';
const common = require('../common');
const {
  assertSummaryShape,
  assertSingleDetailedShape,
} = require('../common/measure-memory');
const assert = require('assert');
const vm = require('vm');

// Test eager memory measurement
{
  vm.measureMemory({ execution: 'eager' })
    .then(common.mustCall(assertSummaryShape));

  if (!common.isWindows) {
    vm.measureMemory({ mode: 'detailed', execution: 'eager' })
      .then(common.mustCall(assertSingleDetailedShape));
  }

  vm.measureMemory({ mode: 'summary', execution: 'eager' })
    .then(common.mustCall(assertSummaryShape));

  assert.throws(() => vm.measureMemory(null), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => vm.measureMemory('summary'), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => vm.measureMemory({ mode: 'random' }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
  assert.throws(() => vm.measureMemory({ execution: 'random' }), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
}
