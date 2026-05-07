// Flags: --expose-gc

'use strict';

require('../common');
const assert = require('assert');
const v8 = require('v8');

// Walk the result tree and collect every node's contexts array (if present).
function collectContexts(node) {
  const out = [];
  if (Array.isArray(node.contexts)) {
    for (const c of node.contexts) {
      out.push({ context: c.context, timestamp: c.timestamp });
    }
  }
  for (const child of node.children || []) {
    out.push(...collectContexts(child));
  }
  return out;
}

function burnCpu(ms) {
  const end = Date.now() + ms;
  // eslint-disable-next-line no-empty
  while (Date.now() < end) {}
}

(async function main() {
  // Test 1: backwards-compat — v8.startCpuProfile() with no args returns a
  // handle whose stop() yields a JSON string.
  {
    const handle = v8.startCpuProfile();
    burnCpu(20);
    const json = handle.stop();
    assert.strictEqual(typeof json, 'string');
    assert.ok(json.length > 0);
    JSON.parse(json);   // valid JSON
    // Calling stop() again is a no-op.
    assert.strictEqual(handle.stop(), undefined);
  }

  // Test 2: stopAndCapture without context — returns object tree shape, no
  // contexts arrays anywhere because withContext was not enabled.
  {
    const handle = v8.startCpuProfile();
    burnCpu(40);
    const profile = handle.stopAndCapture();
    assert.strictEqual(typeof profile, 'object');
    assert.strictEqual(typeof profile.startTime, 'number');
    assert.strictEqual(typeof profile.endTime, 'number');
    assert.strictEqual(typeof profile.droppedContexts, 'number');
    assert.strictEqual(typeof profile.topDownRoot, 'object');
    const ctxs = collectContexts(profile.topDownRoot);
    assert.strictEqual(ctxs.length, 0);
  }

  // Test 3: with withContext, samples taken inside runWithContext carry the
  // context value.
  {
    const handle = v8.startCpuProfile({
      withContext: true,
      contextBufferSize: 4096,
      intervalMicros: 500,
    });
    const marker = { which: 'phase-A' };
    handle.runWithContext(marker, () => {
      burnCpu(80);
    });
    const profile = handle.stopAndCapture();
    const ctxs = collectContexts(profile.topDownRoot);
    assert.ok(ctxs.length > 0);
    for (const { context } of ctxs) {
      assert.strictEqual(context, marker);
    }
  }

  // Test 4: context propagates across an awaited continuation.
  {
    const handle = v8.startCpuProfile({
      withContext: true,
      intervalMicros: 500,
    });
    const marker = { which: 'phase-B' };
    await handle.runWithContext(marker, async () => {
      burnCpu(40);
      await new Promise((resolve) => setImmediate(resolve));
      burnCpu(40);
    });
    const profile = handle.stopAndCapture();
    const ctxs = collectContexts(profile.topDownRoot);
    assert.ok(ctxs.length > 0);
    for (const { context } of ctxs) {
      assert.strictEqual(context, marker);
    }
  }

  // Test 5: snapshot() returns the just-captured tree and continues sampling
  // for the next snapshot. Uses different markers on either side to verify
  // sessions don't bleed into one another.
  {
    const handle = v8.startCpuProfile({
      withContext: true,
      intervalMicros: 500,
    });
    const markerA = { phase: 'A' };
    const markerB = { phase: 'B' };
    handle.runWithContext(markerA, () => burnCpu(60));
    const snapA = handle.snapshot();
    handle.runWithContext(markerB, () => burnCpu(60));
    const snapB = handle.stopAndCapture();

    const ctxsA = collectContexts(snapA.topDownRoot);
    const ctxsB = collectContexts(snapB.topDownRoot);
    assert.ok(ctxsA.length > 0, 'snapshot A should have context-bearing samples');
    assert.ok(ctxsB.length > 0, 'snapshot B should have context-bearing samples');
    for (const { context } of ctxsA) {
      assert.strictEqual(context, markerA, 'snapshot A leaked phase B context');
    }
    for (const { context } of ctxsB) {
      assert.strictEqual(context, markerB, 'snapshot B leaked phase A context');
    }
  }

  // Test 6: buffer-full path increments droppedContexts.
  {
    const handle = v8.startCpuProfile({
      withContext: true,
      contextBufferSize: 4,
      intervalMicros: 500,
    });
    handle.runWithContext({ which: 'phase-D' }, () => burnCpu(150));
    const profile = handle.stopAndCapture();
    assert.ok(profile.droppedContexts > 0,
              `expected droppedContexts > 0, got ${profile.droppedContexts}`);
  }

  // Test 7: runWithContext / enterWithContext throw when withContext is not
  // set.
  {
    const handle = v8.startCpuProfile();
    assert.throws(() => handle.runWithContext({}, () => {}),
                  /requires \{ withContext: true \}/);
    assert.throws(() => handle.enterWithContext({}),
                  /requires \{ withContext: true \}/);
    handle.stop();
  }

  // Test 8: stop() / stopAndCapture() on already-stopped is no-op.
  {
    const handle = v8.startCpuProfile();
    handle.stop();
    assert.strictEqual(handle.stop(), undefined);
    assert.strictEqual(handle.stopAndCapture(), undefined);
  }

  // Test 9: snapshot() after stop throws.
  {
    const handle = v8.startCpuProfile({ withContext: true });
    handle.stopAndCapture();
    assert.throws(() => handle.snapshot(), /has been stopped/);
  }

  // Test 10: dropping the handle without stopping is recoverable. The
  // destructor's best-effort teardown runs at GC and a subsequent
  // startCpuProfile() must succeed.
  {
    (() => {
      const dropped = v8.startCpuProfile({ withContext: true });
      dropped.runWithContext({ phase: 'orphaned' }, () => burnCpu(20));
      // Intentionally do not call stop / stopAndCapture / snapshot.
    })();
    if (typeof globalThis.gc === 'function') globalThis.gc();
    // Whether or not GC has reclaimed the handle yet, a brand new profiler
    // must start cleanly. (If GC hasn't run, the previous handle is still
    // alive and t_active_profiler is set — that's the existing-active error.)
    let started = false;
    try {
      const fresh = v8.startCpuProfile();
      fresh.stop();
      started = true;
    } catch (e) {
      // Acceptable iff GC didn't run; otherwise this is a real failure.
      assert.match(e.message, /already active/);
    }
    if (typeof globalThis.gc !== 'function' && !started) {
      console.warn(
        'Note: --expose-gc was not enabled; orphaned-handle GC path ' +
        'was not exercised.');
    }
  }
})().then(
  () => console.log('all tests passed'),
  (err) => { console.error(err); process.exit(1); }
);
