// Heap profile labels require async-context-frame (on by default).
//
// End-to-end attribution accuracy: the other label tests check that samples
// carry the labels they should, which catches label bleed but would still
// pass if the bytes attributed to each label were meaningless. These tests
// allocate a known ratio of memory under interleaved async tasks and check
// that the profile reproduces that ratio.
//
// Tolerances are wide on purpose. Sampling is statistical, so the point is to
// catch attribution that is broken (roughly equal shares, or everything
// landing on one label), not to pin down the sampler's precision.
'use strict';
const common = require('../common');
const assert = require('assert');
const v8 = require('v8');

// A heavy and a light task, interleaved through await boundaries so that the
// async context is torn down and restored repeatedly while both are in
// flight. Ground truth is 10:1 by allocation count.
async function testHeapAttribution() {
  const HEAVY_N = 10000;
  const LIGHT_N = 1000;
  const ROUNDS = 20;
  const sink = [];

  const task = async (route, n) => {
    await v8.withHeapProfileLabels({ route }, async () => {
      for (let round = 0; round < 5; round++) {
        await null;
        const arr = new Array(n);
        for (let i = 0; i < n; i++) arr[i] = { route, i, pad: 'x'.repeat(16) };
        sink.push(arr[0]);
      }
    });
  };

  const handle = v8.startHeapProfile({ sampleInterval: 4096, labels: true });
  for (let r = 0; r < ROUNDS; r++) {
    await Promise.all([
      task('/heavy', HEAVY_N),
      task('/light', LIGHT_N),
      task('/heavy', HEAVY_N),
      task('/light', LIGHT_N),
    ]);
  }
  const profile = handle.getAllocationProfile();
  handle.stop();

  let heavy = 0;
  let light = 0;
  let unlabeled = 0;
  for (const sample of profile.samples) {
    const bytes = sample.size * sample.count;
    if (sample.labels.route === '/heavy') heavy += bytes;
    else if (sample.labels.route === '/light') light += bytes;
    else if (sample.labels.route === undefined) unlabeled += bytes;
    else assert.fail(`Unexpected route: ${sample.labels.route}`);
  }

  assert.ok(heavy > 0 && light > 0,
            `Both tasks should be attributed, got heavy=${heavy} light=${light}`);

  // Observed 9.3 to 11.3 across runs against a ground truth of 10. A bug that
  // attributed allocations to whichever context happened to be current would
  // land near 1.
  const ratio = heavy / light;
  assert.ok(ratio > 5 && ratio < 20,
            `heavy:light byte ratio ${ratio.toFixed(2)} is not near the ` +
            'expected 10:1; attribution looks wrong');

  // Almost everything allocated during the profile happens inside a labeled
  // task, so unlabeled bytes should be a small remainder.
  const labeledShare = (heavy + light) / (heavy + light + unlabeled);
  assert.ok(labeledShare > 0.9,
            `Only ${(labeledShare * 100).toFixed(1)}% of sampled bytes were ` +
            'attributed to a label');
}

// Off-heap attribution goes through ProfilingArrayBufferAllocator rather than
// the sampler, so it is exact rather than statistical. Buffers must be larger
// than Buffer.poolSize / 2 to get their own BackingStore; pooled allocations
// are attributed to whichever label triggered the pool refill.
async function testExternalAttribution() {
  const BIG_KB = 64;
  const SMALL_KB = 40;
  const COUNT = 20;
  const ROUNDS = 5;
  const live = [];

  const task = async (route, sizeKB) => {
    await v8.withHeapProfileLabels({ route }, async () => {
      for (let i = 0; i < COUNT; i++) {
        await null;
        live.push(Buffer.allocUnsafe(sizeKB * 1024));
      }
    });
  };

  const handle = v8.startHeapProfile({ sampleInterval: 4096, labels: true });
  for (let r = 0; r < ROUNDS; r++) {
    await Promise.all([task('/big', BIG_KB), task('/small', SMALL_KB)]);
  }
  const profile = handle.getAllocationProfile();
  handle.stop();

  assert.ok(Array.isArray(profile.externalBytes),
            'externalBytes should be present when buffers were allocated');

  let big = 0;
  let small = 0;
  for (const entry of profile.externalBytes) {
    if (entry.labels.route === '/big') big += entry.bytes;
    else if (entry.labels.route === '/small') small += entry.bytes;
  }

  const expectedBig = BIG_KB * 1024 * COUNT * ROUNDS;
  assert.ok(big >= expectedBig * 0.9,
            `/big external bytes ${big} well below the ${expectedBig} ` +
            'actually allocated');
  const ratio = big / small;
  const expectedRatio = BIG_KB / SMALL_KB;
  assert.ok(ratio > expectedRatio * 0.6 && ratio < expectedRatio * 1.6,
            `big:small external byte ratio ${ratio.toFixed(2)} is not near ` +
            `the expected ${expectedRatio.toFixed(2)}`);
}

async function main() {
  await testHeapAttribution();
  await testExternalAttribution();
}

main().then(common.mustCall());
