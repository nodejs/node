'use strict';

// Benchmark: cost of getAllocationProfile() label resolution.
//
// Builds up live samples under withHeapProfileLabels() across N unique
// label sets, then measures wall-clock time to call getAllocationProfile()
// repeatedly. Varying samples_per_unique_label_set exposes how resolution
// cost scales with sample count vs. unique label sets — this is the
// signal a per-call resolution cache (see PR #62649) should improve.
//
// Run standalone:
//   node benchmark/v8/heap-profiler-labels-resolution.js
//
// Run with compare.js for statistical analysis:
//   node benchmark/compare.js --old ./node-baseline --new ./node-cached \
//     --filter heap-profiler-labels-resolution
//
// Memory: each retained sample corresponds to ~sampleInterval bytes of
// live heap. With the 512 KiB default and a target of 5,000 samples the
// workload retains ~4 GiB after compensating for the V8 sampler dropping
// ~37% of one-sampleInterval-sized allocations. Hence
// --max-old-space-size=6144.

const common = require('../common.js');
const v8 = require('v8');

const SAMPLE_INTERVAL = 512 * 1024; // V8 default
const RETAINED_SAMPLES_TARGET = 5000;
// V8's sampler picks each allocation of size A with probability
// 1 - exp(-A / sampleInterval). For A = sampleInterval that's ~63%, so
// allocate ~1.6x as many chunks as the desired sample count.
const SAMPLE_PROBABILITY = 1 - Math.exp(-1);

const bench = common.createBenchmark(main, {
  samples_per_unique_label_set: [1, 10, 100, 1000],
  n: [20],
}, {
  flags: ['--max-old-space-size=6144'],
});

function main({ samples_per_unique_label_set: samplesPerLabel, n }) {
  const uniqueLabelSets = Math.max(
    1, Math.ceil(RETAINED_SAMPLES_TARGET / samplesPerLabel),
  );
  const chunksPerLabel = Math.max(
    1, Math.ceil(samplesPerLabel / SAMPLE_PROBABILITY),
  );
  // Each chunk is one sampleInterval of JS heap (a JSArray of Smi slots).
  // JSArray over plain strings here because String.repeat() of a single
  // ASCII char appears to bypass the sampler's allocation observers in
  // large-object-space, while typed JSArray allocations are reliably
  // sampled.
  const SLOTS_PER_CHUNK = SAMPLE_INTERVAL / 8;

  v8.startSamplingHeapProfiler(SAMPLE_INTERVAL);

  // The sampling profiler tracks each sample with a weak global; once
  // the underlying object is GC'd the sample is dropped. Holding strong
  // references in this retainer keeps samples live throughout the
  // measurement loop below.
  const retainer = [];
  for (let i = 0; i < uniqueLabelSets; i++) {
    const labels = { route: `/route-${i}`, method: 'GET' };
    v8.withHeapProfileLabels(labels, () => {
      for (let j = 0; j < chunksPerLabel; j++) {
        retainer.push(new Array(SLOTS_PER_CHUNK).fill(0));
      }
    });
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    const profile = v8.getAllocationProfile();
    // Defensive use of the result so the call is not eliminated.
    if (!profile) throw new Error('profile missing');
  }
  bench.end(n);

  v8.stopSamplingHeapProfiler();
  // Touch retainer post-bench so it stays live across the measurement.
  if (retainer.length === 0) throw new Error('retainer leaked');
}
