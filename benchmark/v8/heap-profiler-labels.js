'use strict';

// Benchmark: overhead of V8 sampling heap profiler with and without labels.
//
// Measures per-allocation cost across three modes:
// - none: no profiler running (baseline)
// - sampling: profiler active, no labels callback
// - sampling-with-labels: profiler active with labels via withHeapProfileLabels
//
// Run standalone:
//   node benchmark/v8/heap-profiler-labels.js
//
// Run with compare.js for statistical analysis:
//   node benchmark/compare.js --old ./node-baseline --new ./node-with-labels \
//     --filter heap-profiler-labels

const common = require('../common.js');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  mode: ['none', 'sampling', 'sampling-with-labels'],
  n: [1e6],
});

function main({ mode, n }) {
  const interval = 512 * 1024; // 512KB — V8 default, production-realistic.

  if (mode === 'sampling') {
    v8.startSamplingHeapProfiler(interval);
  } else if (mode === 'sampling-with-labels') {
    v8.startSamplingHeapProfiler(interval);
  }

  if (mode === 'sampling-with-labels') {
    v8.withHeapProfileLabels({ route: '/bench' }, () => {
      runWorkload(n);
    });
  } else {
    runWorkload(n);
  }

  if (mode !== 'none') {
    v8.stopSamplingHeapProfiler();
  }
}

function runWorkload(n) {
  const arr = [];
  bench.start();
  for (let i = 0; i < n; i++) {
    // Allocate objects with string properties — representative of JSON API
    // workloads. Each object is ~100-200 bytes on the V8 heap.
    arr.push({ id: i, name: `item-${i}`, value: Math.random() });
    // Prevent unbounded growth — keep last 1000 to maintain GC pressure
    // without running out of memory.
    if (arr.length > 1000) arr.shift();
  }
  bench.end(n);
}
