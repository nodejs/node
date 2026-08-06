// Flags: --expose-internals
// Verify Node-side teardown and handle identity when the inspector stops the
// underlying V8 sampling session.
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

assert.strictEqual(getProfilingAllocatorActive(), false,
  'allocator must be inactive before any session');

const handle = v8.startHeapProfile({ sampleInterval: 64, labels: true });

assert.strictEqual(getProfilingAllocatorActive(), true,
  'allocator must be active after startHeapProfile with labels:true');

// Allocate some objects so the sampler has work to do.
const arr = [];
for (let i = 0; i < 1000; i++) arr.push(new Array(100).fill(i));

// Steal V8's sampling profiler via the inspector, simulating what any user
// code that opens a plain inspector Session can do.
const session = new inspector.Session();
session.connect();
session.post('HeapProfiler.stopSampling', (err) => {
  assert.strictEqual(err, null, 'inspector stopSampling must not error');

  // handle.stop() must throw because V8 has no profile, but the teardown
  // must run first — that is the invariant this test guards.
  let threw = false;
  try {
    handle.stop();
  } catch (e) {
    threw = true;
    assert.strictEqual(e.code, 'ERR_HEAP_PROFILE_NOT_STARTED',
      'stop() must throw ERR_HEAP_PROFILE_NOT_STARTED');
  }
  assert.ok(threw, 'handle.stop() must throw after inspector steal');

  // The profiling allocator must be released even though stop() threw.
  assert.strictEqual(getProfilingAllocatorActive(), false,
    'allocator must be inactive after stop() following an inspector steal');

  // The handle is now stopped; subsequent stop() returns undefined.
  assert.strictEqual(handle.stop(), undefined,
    'second stop() must return undefined');

  // getAllocationProfile() must return undefined on a stopped handle.
  assert.strictEqual(handle.getAllocationProfile(), undefined,
    'getAllocationProfile() must return undefined after stop');

  // A new labels session must be startable after the stolen stop.
  const handle2 = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  assert.strictEqual(getProfilingAllocatorActive(), true,
    'allocator must be active again after fresh startHeapProfile');
  const profile2 = handle2.getAllocationProfile();
  handle2.stop();
  assert.ok(profile2, 'second session must return a valid profile');
  assert.ok(Array.isArray(profile2.samples),
    'second session profile must have samples array');

  session.disconnect();

  // Starting a labels:false session must discard stale labels:true state.
  const handle3 = v8.startHeapProfile({ sampleInterval: 64, labels: true });
  assert.strictEqual(getProfilingAllocatorActive(), true,
    'allocator must be active after second labels:true start');

  const session2 = new inspector.Session();
  session2.connect();
  session2.post('HeapProfiler.stopSampling', (err2) => {
    assert.strictEqual(err2, null,
      'second inspector stopSampling must not error');

    // handle3 has NOT been stopped. Its heap_profiling_cleanup_ is still
    // set. Starting a labels:false session must discard that stale cleanup
    // (and the allocator it owns) before starting the new V8 session.
    const handle4 = v8.startHeapProfile({ sampleInterval: 64 });

    assert.strictEqual(getProfilingAllocatorActive(), false,
      'allocator must be inactive after labels:false start discards stale ' +
      'cleanup from a stolen labels:true session');

    handle4.stop();

    // handle3 was never stopped and its session was stolen before handle4
    // started.  The generation counter stamps handle4 with a new generation,
    // so handle3's generation no longer matches.  Its stop() must be a no-op
    // (return undefined) rather than disturbing whatever session is current.
    assert.strictEqual(handle3.stop(), undefined,
      'stale handle stop() must be a no-op when a newer session has run');

    session2.disconnect();

    // A stale handle must not read or stop a newer live session.
    const handle5 = v8.startHeapProfile({ sampleInterval: 64, labels: true });
    const arr5 = [];
    for (let i = 0; i < 500; i++) arr5.push(new Array(100).fill(i));

    const session3 = new inspector.Session();
    session3.connect();
    session3.post('HeapProfiler.stopSampling', (err3) => {
      assert.strictEqual(err3, null,
        'third inspector stopSampling must not error');

      // Start a new session (handle6) while handle5 is stale.
      const handle6 = v8.startHeapProfile({ sampleInterval: 64, labels: true });
      const arr6 = [];
      for (let i = 0; i < 500; i++) arr6.push(new Array(100).fill(i));

      // Stale handle5 must not see handle6's samples.
      assert.strictEqual(handle5.getAllocationProfile(), undefined,
        'stale handle getAllocationProfile() must return undefined');

      // Stale handle5 stop() must be a no-op — must not consume handle6.
      assert.strictEqual(handle5.stop(), undefined,
        'stale handle stop() must return undefined');

      // handle6 must still be running and return a valid profile.
      const profile6 = handle6.getAllocationProfile();
      assert.ok(profile6 !== undefined,
        'live handle getAllocationProfile() must return a profile after ' +
        'stale handle.stop()');
      assert.ok(Array.isArray(profile6.samples),
        'live handle profile must have samples array');

      // handle6.stop() must succeed.
      const result6 = handle6.stop();
      assert.ok(typeof result6 === 'string',
        'live handle stop() must return the DevTools JSON string');

      session3.disconnect();
    });
  });
});
