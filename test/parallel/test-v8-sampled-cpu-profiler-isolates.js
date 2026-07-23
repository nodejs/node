'use strict';

// Verifies that v8.startCpuProfile() with { withContext: true } maintains
// per-isolate state: a profile in the main isolate and a profile in a Worker
// isolate run concurrently without their contexts crossing over.

const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, parentPort, workerData } =
    require('worker_threads');
const v8 = require('v8');

function burnCpu(ms) {
  const end = Date.now() + ms;
  // eslint-disable-next-line no-empty
  while (Date.now() < end) {}
}

function collectContexts(node) {
  const out = [];
  if (Array.isArray(node.contexts)) {
    for (const c of node.contexts) out.push(c.context);
  }
  for (const child of node.children || []) {
    out.push(...collectContexts(child));
  }
  return out;
}

if (!isMainThread) {
  // Worker side: profile in this isolate with our own marker.
  const handle = v8.startCpuProfile({
    withContext: true,
    intervalMicros: 500,
  });
  const marker = workerData.marker;
  handle.runWithContext(marker, () => burnCpu(120));
  const profile = handle.stopAndCapture();
  const ctxs = collectContexts(profile.topDownRoot);
  parentPort.postMessage({
    sampleCount: ctxs.length,
    distinctMarkers: [...new Set(ctxs)],
  });
  return;
}

// Main thread: profile concurrently in this isolate.
const w = new Worker(__filename, { workerData: { marker: 'worker-marker' } });

const handle = v8.startCpuProfile({
  withContext: true,
  intervalMicros: 500,
});
const mainMarker = 'main-marker';
handle.runWithContext(mainMarker, () => burnCpu(120));

w.on('message', common.mustCall((workerResult) => {
  const profile = handle.stopAndCapture();
  const mainCtxs = collectContexts(profile.topDownRoot);

  assert.ok(mainCtxs.length > 0,
            'main isolate profile should have context-bearing samples');
  for (const ctx of mainCtxs) {
    assert.strictEqual(ctx, mainMarker,
                       `main isolate leaked: ${ctx} (expected ${mainMarker})`);
  }

  assert.ok(workerResult.sampleCount > 0,
            'worker isolate profile should have context-bearing samples');
  assert.deepStrictEqual(workerResult.distinctMarkers, ['worker-marker'],
                         `worker isolate leaked: ${workerResult.distinctMarkers}`);
}));
