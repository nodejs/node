// Flags: --expose-gc

'use strict';
const common = require('../common');
const {
  assertSummaryShape,
  expectExperimentalWarning
} = require('../common/measure-memory');
const vm = require('vm');

expectExperimentalWarning();

// Test lazy memory measurement - we will need to globalThis.gc()
// or otherwise these may not resolve.
{
  vm.measureMemory()
    .then(assertSummaryShape).then(common.mustCall());
  globalThis.gc();
}

{
  vm.measureMemory({})
    .then(assertSummaryShape).then(common.mustCall());
  globalThis.gc();
}

{
  vm.measureMemory({ mode: 'summary' })
    .then(assertSummaryShape).then(common.mustCall());
  globalThis.gc();
}

{
  vm.measureMemory({ mode: 'detailed' })
    .then(assertSummaryShape).then(common.mustCall());
  globalThis.gc();
}
