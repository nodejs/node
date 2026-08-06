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
  const interval = 512 * 1024; // 512 KiB, V8's default sampling interval.

  let handle;
  if (mode === 'sampling') {
    handle = v8.startHeapProfile({ sampleInterval: interval });
  } else if (mode === 'sampling-with-labels') {
    handle = v8.startHeapProfile({ labels: true, sampleInterval: interval });
  }

  if (mode === 'sampling-with-labels') {
    v8.withHeapProfileLabels({ route: '/bench' }, () => {
      runWorkload(n);
    });
  } else {
    runWorkload(n);
  }

  if (handle) {
    handle.stop();
  }
}

function runWorkload(n) {
  const arr = [];
  bench.start();
  for (let i = 0; i < n; i++) {
    // Allocate objects with string properties. Each object is ~100-200
    // bytes on the V8 heap.
    arr.push({ id: i, name: `item-${i}`, value: Math.random() });
    // Retain the last 1000 objects to keep steady-state GC pressure
    // without unbounded growth.
    if (arr.length > 1000) arr.shift();
  }
  bench.end(n);
}
