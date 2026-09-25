'use strict';

const common = require('../common');
const assert = require('assert');
const {
  createJSHeapSnapshot,
  validateByRetainingPathFromNodes,
} = require('../common/heap');
const {
  createHistogram,
  createSlidingWindowHistogram,
} = require('perf_hooks');

(async () => {
  const uncached = createHistogram();
  const cached = createHistogram();
  cached.record(1);
  cached.record(1000);
  await cached.qrde({ cache: true });

  const sliding = createSlidingWindowHistogram({
    chunks: 2,
    recordsPerChunk: 1,
  });

  const nodes = createJSHeapSnapshot();
  const snapshots = validateByRetainingPathFromNodes(
    nodes,
    'Node / Histogram',
    [{ node_name: 'Node / qrde_snapshot', edge_name: 'qrde_snapshot' }],
  );
  assert.strictEqual(snapshots.length, 1);
  assert.ok(snapshots[0].self_size > 0);

  const windows = validateByRetainingPathFromNodes(
    nodes,
    'Node / SlidingWindowHistogram',
    [],
  );
  for (const [edgeName, nodeName] of [
    ['chunks', 'Node / chunks'],
    ['generations', 'Node / generations'],
    ['spare', 'Node / Histogram'],
  ]) {
    validateByRetainingPathFromNodes(windows, 'Node / SlidingWindowHistogram', [
      { node_name: nodeName, edge_name: edgeName },
    ]);
  }

  // Keep all three wrappers live through snapshot generation.
  assert.strictEqual(uncached.count, 0);
  assert.strictEqual(cached.count, 2);
  assert.strictEqual(sliding.snapshot().count, 0);
})().then(common.mustCall());
