// Flags: --expose-gc
'use strict';

require('../common');
const assert = require('assert');
const v8 = require('v8');

// Helper: find an externalBytes entry whose labels match a predicate.
function findExternal(profile, predicate) {
  if (!Array.isArray(profile.externalBytes)) return undefined;
  return profile.externalBytes.find(predicate);
}

// Helper: find an externalBytes entry by a single label key-value pair.
function findByLabel(profile, key, value) {
  return findExternal(profile, (e) => e.labels[key] === value);
}

// Test 1: Buffer.alloc() inside withHeapProfileLabels is attributed to the
// correct label in externalBytes.
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  // Allocate 10MB Buffer inside a labeled context.
  const buf = v8.withHeapProfileLabels({ route: '/heavy' }, () => {
    const b = Buffer.alloc(10 * 1024 * 1024);
    // Keep buf alive.
    assert.strictEqual(b.length, 10 * 1024 * 1024);
    return b;
  });

  const profile = handle.getAllocationProfile();
  assert.ok(profile, 'profile should exist');

  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
    const entry = findByLabel(profile, 'route', '/heavy');
    assert.ok(entry, 'Expected entry for route=/heavy in externalBytes');
    assert.ok(entry.bytes > 0,
      `Expected /heavy external bytes > 0, got ${entry.bytes}`);
    // The 10MB Buffer should show up (allow some tolerance for overhead).
    assert.ok(entry.bytes >= 9 * 1024 * 1024,
      `Expected /heavy >= 9MB, got ${entry.bytes}`);
  }

  // Keep buf alive until after profile is read.
  assert.ok(buf.length > 0);
  handle.stop();
}

// Test 2: Buffer.alloc() outside any label context is not tracked.
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  // Allocate outside any label context.
  const buf = Buffer.alloc(5 * 1024 * 1024);
  assert.strictEqual(buf.length, 5 * 1024 * 1024);

  const profile = handle.getAllocationProfile();
  assert.ok(profile, 'profile should exist');

  // The profiling allocator skips unlabelled allocations, so externalBytes
  // must be absent (undefined). If it is present for some reason, assert it
  // carries zero attributed bytes.
  if (Array.isArray(profile.externalBytes)) {
    const totalLabeled = profile.externalBytes
      .reduce((a, e) => a + e.bytes, 0);
    assert.strictEqual(totalLabeled, 0,
      `Expected 0 labeled external bytes, got ${totalLabeled}`);
  } else {
    // Absence is the expected outcome: unlabelled allocations are not
    // tracked, so the field is omitted from the profile.
    assert.strictEqual(profile.externalBytes, undefined);
  }

  handle.stop();
}

// Test 3: After dropping Buffer references and forcing GC, per-label bytes
// decrease (Free is called).
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  let profile;

  v8.withHeapProfileLabels({ route: '/gc-test' }, () => {
    // Create a Buffer, then let it be GC'd.
    let buf = Buffer.alloc(8 * 1024 * 1024);
    assert.strictEqual(buf.length, 8 * 1024 * 1024);

    profile = handle.getAllocationProfile();
    assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
      // Entry must exist: 8MB was just allocated inside this context.
      const entry = findByLabel(profile, 'route', '/gc-test');
      assert.ok(entry, 'Expected entry for route=/gc-test before GC');
      assert.ok(entry.bytes >= 7 * 1024 * 1024,
        `Expected /gc-test >= 7MB before GC, got ${entry.bytes}`);
    }

    // Drop reference and force GC.
    buf = null;
  });

  global.gc();
  global.gc();

  profile = handle.getAllocationProfile();
  // After GC, externalBytes may be absent if all labeled allocations were freed.
  {
    const entry = Array.isArray(profile.externalBytes)
      ? findByLabel(profile, 'route', '/gc-test') : undefined;
    const afterGC = entry ? entry.bytes : 0;
    // After GC, the buffer should be freed and the count should decrease.
    // It may not go to exactly 0 due to other small allocations.
    assert.ok(afterGC < 8 * 1024 * 1024,
      `Expected /gc-test < 8MB after GC, got ${afterGC}`);
  }

  handle.stop();
}

// Test 4: Multiple labels — allocate Buffers with different labels, verify
// externalBytes shows correct per-label totals.
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  const bufs = [];
  v8.withHeapProfileLabels({ route: '/api/users' }, () => {
    bufs.push(Buffer.alloc(4 * 1024 * 1024));
  });

  v8.withHeapProfileLabels({ route: '/api/orders' }, () => {
    bufs.push(Buffer.alloc(6 * 1024 * 1024));
  });

  const profile = handle.getAllocationProfile();
  assert.ok(profile, 'profile should exist');

  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
    const usersEntry = findByLabel(profile, 'route', '/api/users');
    const ordersEntry = findByLabel(profile, 'route', '/api/orders');
    const usersBytes = usersEntry ? usersEntry.bytes : 0;
    const ordersBytes = ordersEntry ? ordersEntry.bytes : 0;
    assert.ok(usersBytes >= 3 * 1024 * 1024,
      `Expected /api/users >= 3MB, got ${usersBytes}`);
    assert.ok(ordersBytes >= 5 * 1024 * 1024,
      `Expected /api/orders >= 5MB, got ${ordersBytes}`);
    // Orders should have more external memory than users.
    assert.ok(ordersBytes > usersBytes,
      `Expected /api/orders (${ordersBytes}) > /api/users (${usersBytes})`);
  }

  // Keep bufs alive.
  assert.ok(bufs.length === 2);
  handle.stop();
}

// Test 5: JSON serialization of the profile includes externalBytes.
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  const buf = v8.withHeapProfileLabels({ route: '/json-test' }, () => {
    return Buffer.alloc(2 * 1024 * 1024);
  });

  const profile = handle.getAllocationProfile();
  const json = JSON.stringify(profile);
  const parsed = JSON.parse(json);

  assert.ok(Array.isArray(parsed.samples), 'samples should be an array');
  // externalBytes must be present: 2MB was allocated inside the label context.
  assert.ok(Array.isArray(parsed.externalBytes),
    'externalBytes should survive JSON round-trip');
  {
    const entry = parsed.externalBytes.find(
      (e) => e.labels && e.labels.route === '/json-test'
    );
    assert.ok(entry, 'Expected /json-test in serialized externalBytes');
    assert.ok(entry.bytes > 0,
      `Expected /json-test bytes > 0, got ${entry.bytes}`);
  }

  // Keep buf alive.
  assert.ok(buf.length > 0);
  handle.stop();
}

// Test 6: Multi-label context — both key-value pairs appear in externalBytes.
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  const buf = v8.withHeapProfileLabels(
    { route: '/foo', handler: 'bar' }, () => {
      return Buffer.alloc(3 * 1024 * 1024);
    });

  const profile = handle.getAllocationProfile();
  assert.ok(profile, 'profile should exist');

  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
    const entry = findExternal(profile,
      (e) => e.labels.route === '/foo' && e.labels.handler === 'bar');
    assert.ok(entry,
      'Expected entry with both route=/foo and handler=bar');
    assert.ok(entry.bytes >= 2 * 1024 * 1024,
      `Expected multi-label entry >= 2MB, got ${entry.bytes}`);
    // Verify both keys are present.
    assert.strictEqual(entry.labels.route, '/foo');
    assert.strictEqual(entry.labels.handler, 'bar');
  }

  // Keep buf alive.
  assert.ok(buf.length > 0);
  handle.stop();
}

// Test 7: externalBytes labels match heap sample labels for same context.
{
  const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

  const buf = v8.withHeapProfileLabels({ route: '/match-test' }, () => {
    // Allocate both heap objects and a Buffer in the same label context.
    const arr = [];
    for (let i = 0; i < 1000; i++) {
      arr.push({ data: new Array(100).fill(i) });
    }
    const b = Buffer.alloc(5 * 1024 * 1024);
    // Keep arr alive.
    assert.ok(arr.length > 0);
    return b;
  });

  const profile = handle.getAllocationProfile();

  // Find heap samples with matching labels.
  const labeledSamples = profile.samples.filter(
    (s) => s.labels && s.labels.route === '/match-test'
  );

  // Find externalBytes entry with matching labels.
  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (ProfilingArrayBufferAllocator active)');
  {
    // extEntry must exist: 5MB Buffer was allocated inside the context.
    const extEntry = findByLabel(profile, 'route', '/match-test');
    assert.ok(extEntry, 'Expected /match-test entry in externalBytes');
    // With 64-byte interval and 1000 × ~800-byte heap allocations,
    // labeled heap samples are reliably expected.
    assert.ok(labeledSamples.length > 0,
      'Expected heap samples for /match-test (64-byte interval, ~800KB allocated)');
    const sampleLabelKeys = Object.keys(labeledSamples[0].labels).sort();
    const extLabelKeys = Object.keys(extEntry.labels).sort();
    assert.deepStrictEqual(extLabelKeys, sampleLabelKeys,
      'externalBytes label keys should match heap sample label keys');
  }

  // Keep buf alive.
  assert.ok(buf.length > 0);
  handle.stop();
}

// Test 8: every externalBytes entry has at least one own label key.
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  const buf = v8.withHeapProfileLabels({ route: '/invariant-test' }, () => {
    return Buffer.alloc(2 * 1024 * 1024);
  });

  const profile = handle.getAllocationProfile();

  // externalBytes must be present: 2MB was allocated inside the label context.
  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes should be an array (labelled Buffer allocated)');
  for (const entry of profile.externalBytes) {
    assert.ok(
      Object.keys(entry.labels).length > 0,
      'Every externalBytes entry must have at least one own key on its ' +
      `labels object; got ${JSON.stringify(entry.labels)}`
    );
  }

  // Keep buf alive.
  assert.ok(buf.length > 0);
  handle.stop();
}

// Test 9: externalBytes is absent (undefined, not an empty array) when no
// labelled backing stores are live. The profiling allocator skips untagged
// allocations, so a Buffer allocated outside any label context produces no
// tracked entry and the field must be omitted from the profile entirely.
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  const buf = Buffer.alloc(2 * 1024 * 1024);
  assert.ok(buf.length > 0);

  const profile = handle.getAllocationProfile();
  handle.stop();

  assert.strictEqual(profile.externalBytes, undefined,
    'externalBytes must be absent (not an empty array) when no labelled ' +
    'backing stores are live');
}

// Test 10: content-based merging — two distinct label contexts with identical
// label content merge into a single externalBytes entry whose bytes are the sum.
// The two contexts have different label_ids (separate flat arrays, separate
// intern-table entries) but the same serialised content, so GetAllocationProfile
// deduplicates them by content before building the output array.
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  const buf1 = v8.withHeapProfileLabels({ route: '/merge-test' }, () => {
    return Buffer.alloc(3 * 1024 * 1024);
  });
  const buf2 = v8.withHeapProfileLabels({ route: '/merge-test' }, () => {
    return Buffer.alloc(4 * 1024 * 1024);
  });

  const profile = handle.getAllocationProfile();

  // Keep both Buffers alive through the profile read.
  assert.ok(buf1.length > 0);
  assert.ok(buf2.length > 0);

  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes must be present (7MB of labelled Buffers allocated)');
  const merged = profile.externalBytes.filter(
    (e) => e.labels.route === '/merge-test'
  );
  assert.strictEqual(merged.length, 1,
    'Two distinct label contexts with identical content must merge into one ' +
    'externalBytes entry');
  assert.ok(merged[0].bytes >= 6 * 1024 * 1024,
    `Merged entry must sum both allocations (>= 6 MB), got ${merged[0].bytes}`);

  handle.stop();
}

// Test 11: label serialisation keeps NUL-containing values distinct.
{
  const handle = v8.startHeapProfile({ sampleInterval: 512 * 1024, labels: true });

  const buf1 = v8.withHeapProfileLabels({ route: 'a' }, () => {
    return Buffer.alloc(3 * 1024 * 1024);
  });
  const buf2 = v8.withHeapProfileLabels({ route: 'a\0x' }, () => {
    return Buffer.alloc(3 * 1024 * 1024);
  });

  const profile = handle.getAllocationProfile();

  assert.ok(buf1.length > 0);
  assert.ok(buf2.length > 0);

  assert.ok(Array.isArray(profile.externalBytes),
    'externalBytes must be present (two labelled Buffers allocated)');

  const routeA = profile.externalBytes.filter((e) => e.labels.route === 'a');
  const routeNul = profile.externalBytes.filter(
    (e) => e.labels.route === 'a\0x');

  assert.strictEqual(routeA.length, 1,
    'Expected exactly one entry for route="a"');
  assert.strictEqual(routeNul.length, 1,
    'Expected exactly one entry for route="a\\0x"; ' +
    'NUL bytes in labels must not collide with shorter values');

  handle.stop();
}

console.log('All external memory tracking tests passed.');
