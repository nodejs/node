// Flags: --expose-gc
// Heap profile labels require async-context-frame (on by default).
'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');

// Test: labels API functions are exported
assert.strictEqual(typeof v8.startHeapProfile, 'function');
assert.strictEqual(typeof v8.withHeapProfileLabels, 'function');
assert.strictEqual(typeof v8.setHeapProfileLabels, 'function');

// A labels:false session must ignore labels set before session start.
{
  v8.setHeapProfileLabels({ route: '/gate-false' });
  const handle = v8.startHeapProfile({ sampleInterval: 64 }); // labels:false
  const arr = [];
  for (let i = 0; i < 2000; i++) arr.push(new Array(200).fill(i));
  const profile = handle.getAllocationProfile();
  handle.stop();

  assert.ok(profile);
  const labeled = profile.samples.filter(
    (s) => Object.keys(s.labels).length > 0,
  );
  assert.strictEqual(labeled.length, 0,
    'labels:false session must emit zero labelled samples');

  // Reset the ALS so the /gate-false label does not contaminate later tests.
  // setHeapProfileLabels uses enterWith, which persists in the async context.
  v8.setHeapProfileLabels({});
}

// Test: handle has getAllocationProfile method
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  assert.strictEqual(typeof handle.getAllocationProfile, 'function');
  handle.stop();
}

// Test: getAllocationProfile returns undefined after stop
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  handle.stop();
  assert.strictEqual(handle.getAllocationProfile(), undefined);
}

// Test: basic profiling without labels
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  const arr = [];
  for (let i = 0; i < 1000; i++) arr.push({ x: i });
  const profile = handle.getAllocationProfile();
  handle.stop();

  assert.ok(profile);
  assert.ok(Array.isArray(profile.samples));
  assert.ok(profile.samples.length > 0);

  // Every sample should have a labels field (empty object when unlabeled)
  for (const sample of profile.samples) {
    assert.strictEqual(typeof sample.nodeId, 'number');
    assert.strictEqual(typeof sample.size, 'number');
    assert.strictEqual(typeof sample.count, 'number');
    assert.strictEqual(typeof sample.sampleId, 'number');
    assert.strictEqual(typeof sample.labels, 'object');
    assert.ok(sample.labels !== null);
  }
}

// Test: withHeapProfileLabels captures labels on samples
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  v8.withHeapProfileLabels({ route: '/test' }, () => {
    const arr = [];
    for (let i = 0; i < 5000; i++) arr.push({ data: i });
  });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/test'
  );
  assert.ok(labeled.length > 0, 'Should have samples labeled with /test');
}

// Test: distinct labels are attributed correctly
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  v8.withHeapProfileLabels({ route: '/heavy' }, () => {
    const arr = [];
    for (let i = 0; i < 10000; i++) arr.push(new Array(100));
  });

  v8.withHeapProfileLabels({ route: '/light' }, () => {
    const arr = [];
    for (let i = 0; i < 100; i++) arr.push({ x: i });
  });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const heavy = profile.samples.filter((s) => s.labels.route === '/heavy');
  const light = profile.samples.filter((s) => s.labels.route === '/light');

  // Attribution-correctness: every labeled sample must carry one of the
  // two expected routes. A misattribution bug would produce samples with an
  // unexpected route value.
  const expectedRoutes = new Set(['/heavy', '/light']);
  const allLabeled = profile.samples.filter(
    (s) => s.labels.route !== undefined
  );
  for (const sample of allLabeled) {
    assert.ok(expectedRoutes.has(sample.labels.route),
      `Sample carries unexpected route: ${sample.labels.route}`);
  }
  // /heavy allocates 10× more than /light: samples are reliably expected.
  assert.ok(heavy.length > 0, 'Should have /heavy samples');
  // /light may have zero samples due to its low allocation volume.
}

// Test: multi-key labels
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  v8.withHeapProfileLabels({ route: '/api', method: 'GET' }, () => {
    const arr = [];
    for (let i = 0; i < 5000; i++) arr.push({ data: i });
  });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/api' && s.labels.method === 'GET'
  );
  assert.ok(labeled.length > 0, 'Should have multi-key labeled samples');
}

// Test: JSON.stringify round-trip
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  v8.withHeapProfileLabels({ route: '/json' }, () => {
    const arr = [];
    for (let i = 0; i < 5000; i++) arr.push({ data: i });
  });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const json = JSON.stringify(profile);
  const parsed = JSON.parse(json);
  assert.ok(Array.isArray(parsed.samples));
  const labeled = parsed.samples.filter((s) => s.labels.route === '/json');
  assert.ok(labeled.length > 0, 'Labels survive JSON round-trip');
}

// Test: startHeapProfile({ sampleInterval: 0 }) throws RangeError
assert.throws(() => v8.startHeapProfile({ sampleInterval: 0 }), {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
});

// Test: withHeapProfileLabels validates arguments
assert.throws(() => v8.withHeapProfileLabels('bad', () => {}), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => v8.withHeapProfileLabels({}, 'bad'), {
  code: 'ERR_INVALID_ARG_TYPE',
});
// Non-string label VALUES must be rejected (validateString in labelsToFlat).
// Only non-object labels are covered by the tests above; this pins the value path.
assert.throws(() => v8.withHeapProfileLabels({ route: 42 }, () => {}), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => v8.withHeapProfileLabels({ route: null }, () => {}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Test: setHeapProfileLabels validates arguments
assert.throws(() => v8.setHeapProfileLabels('bad'), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Test: repeated start/stop cycles work
{
  for (let cycle = 0; cycle < 3; cycle++) {
    const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });
    v8.withHeapProfileLabels({ route: `/cycle${cycle}` }, () => {
      const arr = [];
      for (let i = 0; i < 1000; i++) arr.push({ x: i });
    });
    const profile = handle.getAllocationProfile();
    handle.stop();
    assert.ok(profile);
    assert.ok(profile.samples.length > 0);
  }
}

// Test: samples are retained with includeObjectsCollectedByMajorGC and
// includeObjectsCollectedByMinorGC (sample entries themselves survive GC of
// their underlying allocation). Labels for retained samples are kept alive by
// the LabelInternTable refcount — both live and dead-but-retained samples carry
// their labels. The underlying ALS JSArray is unpinned only when ALL samples
// (live and retained) sharing it are gone, i.e. at profiler teardown for
// retained samples. See test-v8-heap-profile-labels-include-collected.js for
// the dedup + label-visibility regression test.
{
  const handle = v8.startHeapProfile({
    sampleInterval: 64,
    stackDepth: 16,
    includeObjectsCollectedByMajorGC: true,
    includeObjectsCollectedByMinorGC: true,
    labels: true,
  });

  // Keep some allocations alive so their samples retain labels through
  // GC: label_id is only released when the underlying object's weak
  // callback fires.
  const heavyAlive = [];
  v8.withHeapProfileLabels({ route: '/heavy-gc' }, () => {
    for (let i = 0; i < 500; i++) {
      // Half the arrays become garbage immediately; half are kept alive.
      const a = new Array(25000).fill(i);
      if (i % 2 === 0) heavyAlive.push(a);
    }
  });

  // Force garbage collection to retire the dead-from-birth allocations.
  global.gc();

  const profile = handle.getAllocationProfile();
  handle.stop();

  // Profile must have samples (retention works at all).
  assert.ok(profile.samples.length > 0);

  // Live-allocation samples in the heavy block keep their labels because
  // their weak callback hasn't fired (the underlying object is still
  // reachable via heavyAlive[]).
  const heavyAliveLabeled =
    profile.samples.filter((s) => s.labels.route === '/heavy-gc');
  assert.ok(heavyAliveLabeled.length > 0,
            'Live-allocation samples should retain their labels');
  // Touch heavyAlive after the assertion to keep it reachable through GC.
  assert.strictEqual(heavyAlive.length, 250);
}

// Test: GC'd samples are removed without includeObjectsCollected* (default)
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  v8.withHeapProfileLabels({ route: '/gc-default' }, () => {
    for (let i = 0; i < 500; i++) {
      // Allocate ~100KB arrays that become garbage immediately
      new Array(25000).fill(i);
    }
  });

  // Force garbage collection — without includeObjectsCollected*, samples are
  // removed from the profile via V8's OnWeakCallback
  global.gc();

  const profile = handle.getAllocationProfile();
  handle.stop();

  const samples = profile.samples.filter(
    (s) => s.labels.route === '/gc-default'
  );
  const totalBytes = samples.reduce((sum, s) => sum + s.size * s.count, 0);
  // After GC, most or all samples should be gone. The total bytes retained
  // should be much less than what was allocated (~50MB).
  assert.ok(
    totalBytes < 5 * 1024 * 1024,
    `Without includeObjectsCollected*, GC'd samples should mostly be removed ` +
    `(got ${(totalBytes / 1024 / 1024).toFixed(1)}MB)`
  );
}

// Test: includeObjectsCollected* retains samples, omitting it does not.
// Labels on retained-but-collected samples are released by the intern
// table refcount, so we compare total profile bytes (not labeled
// bytes) — the retained sample entries themselves are what matters.
{
  // Start WITH includeObjectsCollected*
  const handleWith = v8.startHeapProfile({
    sampleInterval: 64,
    stackDepth: 16,
    includeObjectsCollectedByMajorGC: true,
    includeObjectsCollectedByMinorGC: true,
    labels: true,
  });
  v8.withHeapProfileLabels({ route: '/retained' }, () => {
    for (let i = 0; i < 200; i++) new Array(25000).fill(i);
  });
  global.gc();
  const withProfile = handleWith.getAllocationProfile();
  handleWith.stop();

  const withBytes = withProfile.samples.reduce(
    (sum, s) => sum + s.size * s.count, 0
  );

  // Start WITHOUT includeObjectsCollected*
  const handleWithout = v8.startHeapProfile({
    sampleInterval: 64,
    labels: true,
  });
  v8.withHeapProfileLabels({ route: '/removed' }, () => {
    for (let i = 0; i < 200; i++) new Array(25000).fill(i);
  });
  global.gc();
  const withoutProfile = handleWithout.getAllocationProfile();
  handleWithout.stop();

  const withoutBytes = withoutProfile.samples.reduce(
    (sum, s) => sum + s.size * s.count, 0
  );

  // With includeObjectsCollected* should retain significantly more bytes.
  assert.ok(withBytes > 0,
    `includeObjectsCollected* should retain samples: withBytes=${withBytes}`);
  assert.ok(
    withBytes > withoutBytes * 5,
    `includeObjectsCollected* should retain more samples: ` +
    `with=${(withBytes / 1024).toFixed(0)}KB, ` +
    `without=${(withoutBytes / 1024).toFixed(0)}KB`
  );
}

// Test: setHeapProfileLabels doesn't leak entries when called repeatedly.
// Each call replaces the current ALS store via enterWith. Use two profiler
// sessions: one to exercise the label rotation with profiling active, a fresh
// one to capture only post-loop allocations, so we can assert that ONLY the
// final label appears (no stale routes leaking through).
{
  // Session 1: run the label rotation with profiling active to exercise cleanup.
  const handle1 = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  for (let i = 0; i < 100; i++) {
    v8.setHeapProfileLabels({ route: `/iter${i}` });
  }
  handle1.stop();

  // Session 2: capture only post-loop allocations.
  const handle2 = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  const arr = [];
  for (let i = 0; i < 5000; i++) arr.push({ data: i });
  const profile = handle2.getAllocationProfile();
  handle2.stop();

  // Only the final label (/iter99) must appear: stale route labels must not
  // outlive the loop.
  const finalLabeled = profile.samples.filter(
    (s) => s.labels.route === '/iter99'
  );
  assert.ok(finalLabeled.length > 0,
    'Should have samples labeled with final /iter99');

  const staleLabeled = profile.samples.filter(
    (s) => s.labels.route && s.labels.route !== '/iter99'
  );
  assert.strictEqual(staleLabeled.length, 0,
    `Old label routes must not appear after loop; found: ` +
    JSON.stringify([...new Set(staleLabeled.map((s) => s.labels.route))]));
}

// Labels survive when another ALS store changes the shared
// AsyncContextFrame Map identity.
// withHeapProfileLabels. The CPED-storage approach stores the full CPED value
// on each sample at allocation time and resolves labels at profile-read time
// via Map lookup, so a change in the CPED Map identity does not drop the
// labels.
{
  const { AsyncLocalStorage } = require('async_hooks');
  const otherALS = new AsyncLocalStorage();

  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  v8.withHeapProfileLabels({ route: '/cped-identity' }, () => {
    // Allocate before changing other ALS (CPED address is X)
    const before = [];
    for (let i = 0; i < 2000; i++) before.push({ pre: i });

    // Change a DIFFERENT ALS store — this creates a new AsyncContextFrame,
    // changing the CPED address to Y. The heap profile labels ALS store is
    // still set (it was inherited into the new frame).
    otherALS.enterWith({ unrelated: 'data' });

    // Allocate after the other ALS change (CPED address is now Y, not X)
    const after = [];
    for (let i = 0; i < 2000; i++) after.push({ post: i });
  });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/cped-identity'
  );
  assert.ok(
    labeled.length > 0,
    'Labels must survive when another ALS store changes the CPED address'
  );
}

// Test: labels object is frozen — Object.isFrozen is true and mutation throws
// in strict mode. This is a documented public API guarantee.
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  v8.withHeapProfileLabels({ route: '/frozen-test' }, () => {
    const arr = [];
    for (let i = 0; i < 5000; i++) arr.push({ data: i });
  });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/frozen-test'
  );
  assert.ok(labeled.length > 0, 'Expected labeled samples for /frozen-test');

  for (const sample of labeled) {
    assert.ok(Object.isFrozen(sample.labels),
      'sample.labels must be frozen (documented guarantee)');
    // In strict mode (this file has "use strict") assigning to a frozen object
    // throws TypeError.
    assert.throws(
      () => { sample.labels.newKey = 'value'; },
      TypeError,
      'Mutating a frozen labels object must throw in strict mode'
    );
  }
}

// Test: samples captured under the same label context share the identical (===)
// labels object. This identity guarantee is what makes it safe to freeze and
// share a single object rather than copying it per sample.
// Per the corrected doc: identity is only guaranteed within one context — two
// separate withHeapProfileLabels calls with equal content produce distinct
// frozen objects.
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  v8.withHeapProfileLabels({ route: '/shared-test' }, () => {
    const arr = [];
    for (let i = 0; i < 10000; i++) arr.push({ data: i });
  });

  const profile = handle.getAllocationProfile();
  handle.stop();

  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/shared-test'
  );
  // At 64-byte interval, 10000 × ~30-byte objects yields ~4000+ samples.
  assert.ok(labeled.length > 1,
    'Expected multiple samples from the same context to test sharing');

  const firstLabels = labeled[0].labels;
  for (const sample of labeled) {
    assert.strictEqual(sample.labels, firstLabels,
      'All samples from the same label context must share the identical (===) ' +
      'labels object');
  }
}

// A second startHeapProfile call must throw without stopping the active
// session.
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  const arr = [];
  for (let i = 0; i < 1000; i++) arr.push({ x: i });
  // First session should be active.
  assert.ok(handle.getAllocationProfile(), 'First session must be active');

  // Second start must throw, not silently kill the first session.
  assert.throws(
    () => v8.startHeapProfile({ sampleInterval: 64, labels: true }),
    { code: 'ERR_HEAP_PROFILE_HAVE_BEEN_STARTED' }
  );

  // After the failed second start the first session is still running.
  assert.ok(handle.getAllocationProfile(),
    'First session must still be active after failed second start');

  handle.stop();
}

// A second labels:true start must not steal the active session.
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  assert.throws(
    () => v8.startHeapProfile({ sampleInterval: 64, labels: true }),
    { code: 'ERR_HEAP_PROFILE_HAVE_BEEN_STARTED' }
  );

  // The first session is still alive; stop() must return a profile string.
  const profile = handle.stop();
  assert.ok(typeof profile === 'string' && profile.length > 0,
    'handle.stop() must return a profile string when session was not stolen');
}

// A labels:false start must not steal an active labels:true session.
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  assert.throws(() => v8.startHeapProfile(), {
    code: 'ERR_HEAP_PROFILE_HAVE_BEEN_STARTED',
  });

  handle.stop();
}

// A later labels:true session must reinstall the labels key.
{
  // First session with labels (clears the V8 key on stop via DoCleanup).
  {
    const h = v8.startHeapProfile({ sampleInterval: 64, labels: true });
    v8.setHeapProfileLabels({ route: '/first' });
    const arr = [];
    for (let i = 0; i < 2000; i++) arr.push(new Array(200).fill(i));
    h.stop();
  }

  // Second session with labels after the first ended.
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  v8.withHeapProfileLabels({ route: '/second' }, () => {
    const arr = [];
    for (let i = 0; i < 5000; i++) arr.push(new Array(200).fill(i));
  });
  const profile = handle.getAllocationProfile();
  handle.stop();

  assert.ok(profile);
  const labeled = profile.samples.filter(
    (s) => s.labels.route === '/second',
  );
  assert.ok(labeled.length > 0,
    'labels:true session after a prior labels session must emit labelled samples');
}

// A labels:false session following labels:true must emit no labels.
{
  // First session with labels: arm the key, allocate, then stop.
  {
    const h = v8.startHeapProfile({ sampleInterval: 64, labels: true });
    v8.withHeapProfileLabels({ route: '/prior-true' }, () => {
      const arr = [];
      for (let i = 0; i < 2000; i++) arr.push(new Array(200).fill(i));
    });
    h.stop();
  }

  // Second session without labels: must emit zero labelled samples even
  // though a labels:true session ran immediately before.
  const handle = v8.startHeapProfile({ sampleInterval: 64 });
  v8.setHeapProfileLabels({ route: '/after-prior-true' });
  const arr = [];
  for (let i = 0; i < 2000; i++) arr.push(new Array(200).fill(i));
  const profile = handle.getAllocationProfile();
  handle.stop();

  assert.ok(profile);
  assert.ok(profile.samples.length > 0, 'expected samples from labels:false session');
  const labeled = profile.samples.filter(
    (s) => Object.keys(s.labels).length > 0,
  );
  assert.strictEqual(labeled.length, 0,
    'labels:false session after a prior labels:true session must emit ' +
    'zero labelled samples');

  // Reset the ALS so the label does not contaminate later tests.
  v8.setHeapProfileLabels({});
}
