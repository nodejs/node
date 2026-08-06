// Flags: --expose-internals
// An inspector-owned sampling session must not inherit the labels key from a
// Node-owned session that the inspector stopped.
'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');
const inspector = require('inspector');
const { internalBinding } = require('internal/test/binding');
const { getProfilingAllocatorActive } = internalBinding('v8');

if (typeof getProfilingAllocatorActive !== 'function') {
  // Build does not have V8_HEAP_PROFILER_SAMPLE_LABELS; nothing to test.
  process.exit(0);
}

// Step 1: start a labels:true session via Node.
const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

// Warm up the ALS by running a labelled callback before the steal, so the
// ALS key is definitely set in V8 when the out-of-band stop happens.
// Keep a reference so GC does not collect samples before getAllocationProfile.
let warmUp = [];
for (let i = 0; i < 200; i++) warmUp.push(new Array(50).fill(i));

// Step 2: steal the session with an out-of-band inspector stop.
const session = new inspector.Session();
session.connect();
session.post('HeapProfiler.stopSampling', (stopErr) => {
  assert.strictEqual(stopErr, null, 'inspector stopSampling must not error');

  // Step 3: start a new V8 sampling session via the inspector. This session
  // never requested labels and must not receive any.
  session.post('HeapProfiler.startSampling', {}, (startErr) => {
    assert.strictEqual(startErr, null, 'inspector startSampling must not error');

    // Keep the labelled allocations live until the inspector profile is read.
    let leaked = [];
    v8.withHeapProfileLabels({ route: '/leak-test' }, () => {
      for (let i = 0; i < 3000; i++) leaked.push(new Array(200).fill(i));
    });

    // Read the active inspector-owned profile.
    const profile = handle.getAllocationProfile();
    assert.ok(profile, 'getAllocationProfile must return a profile');
    assert.ok(Array.isArray(profile.samples),
      'profile.samples must be an array');
    assert.ok(profile.samples.length > 0,
      'inspector session must have captured at least one sample');

    // The inspector-owned session did not opt in to labels.
    const labelled = profile.samples.filter(
      (s) => Object.keys(s.labels).length > 0,
    );
    assert.strictEqual(labelled.length, 0,
      `inspector session must have 0 labelled samples; got ${labelled.length} ` +
      `out of ${profile.samples.length} total`);

    // Release references before stopping.
    warmUp = null;
    leaked = null;
    session.post('HeapProfiler.stopSampling', () => {
      session.disconnect();
    });
  });
});
