'use strict';

// Tests that histogram objects report the memory of their native histogram to
// V8, so that creating many short-lived histograms triggers garbage
// collection.

const common = require('../common');
const assert = require('assert');
const { setImmediate: setImmediatePromise } = require('timers/promises');
const {
  PerformanceObserver,
  constants: { NODE_PERFORMANCE_GC_FLAGS_ALL_EXTERNAL_MEMORY },
  createHistogram,
} = require('perf_hooks');

let externalMemoryGCs = 0;
const observer = new PerformanceObserver((list) => {
  for (const entry of list.getEntries()) {
    if (entry.detail.flags & NODE_PERFORMANCE_GC_FLAGS_ALL_EXTERNAL_MEMORY)
      externalMemoryGCs++;
  }
});
observer.observe({ entryTypes: ['gc'] });

// With the default options, a histogram holds 45,056 64-bit counts, about
// 352 KiB. V8 starts a garbage collection once external memory has grown by
// 64 MiB, so 400 unreferenced snapshots (about 137 MiB) must trigger one.
const histogram = createHistogram();
for (let i = 0; i < 400; i++) histogram.snapshot();

(async () => {
  // Performance entries for garbage collections are delivered asynchronously.
  for (let i = 0; i < 10 && externalMemoryGCs === 0; i++)
    await setImmediatePromise();
  observer.disconnect();
  assert.ok(externalMemoryGCs > 0,
            'Expected a garbage collection caused by external memory');
})().then(common.mustCall());
