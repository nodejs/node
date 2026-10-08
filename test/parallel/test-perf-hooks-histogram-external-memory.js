'use strict';

// Tests that histogram objects report the memory of their native histogram to
// V8, so that creating many short-lived histograms triggers garbage
// collection.

const common = require('../common');
const assert = require('assert');
const { setImmediate: setImmediatePromise } = require('timers/promises');
const {
  PerformanceObserver,
  constants: { NODE_PERFORMANCE_GC_MAJOR },
  createHistogram,
} = require('perf_hooks');

let majorGCs = 0;
const observer = new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) {
    if (entry.detail.kind === NODE_PERFORMANCE_GC_MAJOR) majorGCs++;
  }
});
observer.observe({ entryTypes: ['gc'] });

// With the default options, a histogram holds 45,056 64-bit counts, about
// 352 KiB, while its JavaScript wrapper is small. It should only
// triggers a major GC if that memory is reported. The GC trigger and finalization
// can vary depending on V8 internals, so only check the occurrence of a major GC here.
const histogram = createHistogram();
for (let i = 0; i < 400; i++) histogram.snapshot();

(async () => {
  // Performance entries for garbage collections are delivered asynchronously.
  for (let i = 0; i < 10 && majorGCs === 0; i++)
    await setImmediatePromise();
  observer.disconnect();
  assert.ok(majorGCs > 0,
            'Expected a major garbage collection caused by histogram memory');
})().then(common.mustCall());
