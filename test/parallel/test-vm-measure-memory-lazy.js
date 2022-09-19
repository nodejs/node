// Flags: --expose-gc

'use strict';
const common = require('../common');
const {
  assertSummaryShape,
  expectExperimentalWarning
} = require('../common/measure-memory');
const vm = require('vm');

expectExperimentalWarning();

// Test lazy memory measurement - we will need to global.gc()
// or otherwise these may not resolve.
{
  vm.measureMemory()
    .then(common.mustCall(assertSummaryShape));
  global.gc();
}

{
  vm.measureMemory({})
    .then(common.mustCall(assertSummaryShape));
  global.gc();
}

{
  vm.measureMemory({ mode: 'summary' })
    .then(common.mustCall(assertSummaryShape));
  global.gc();
}

{
  vm.measureMemory({ mode: 'detailed' })
    .then(common.mustCall(assertSummaryShape));
  global.gc();
}
