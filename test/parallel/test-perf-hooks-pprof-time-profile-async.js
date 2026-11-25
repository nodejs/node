'use strict';

// Checks how `time.profile()` starts and stops the time profiler of
// node:perf_hooks/pprof.

const common = require('../common');
const assert = require('node:assert');
const { decodeGzippedProfile } = require('../common/pprof');
const { time, encodeSync } = require('node:perf_hooks/pprof');

(async () => {
  // The profiler runs while the returned promise is pending, and the profile
  // covers the whole duration.
  {
    const durationMillis = 100;
    const promise = time.profile({ durationMillis });
    assert.strictEqual(time.isStarted(), true);
    await assert.rejects(time.profile(), { code: 'ERR_INVALID_STATE' });
    assert.throws(() => time.start(), { code: 'ERR_INVALID_STATE' });
    const profile = await promise;
    assert.strictEqual(time.isStarted(), false);
    const { profile: decoded } = decodeGzippedProfile(encodeSync(profile));
    assert.ok(decoded.durationNanos >= durationMillis * 1e6 * 0.9,
              `${decoded.durationNanos}`);
  }

  // Invalid options reject without starting the profiler.
  await assert.rejects(time.profile(null), { code: 'ERR_INVALID_ARG_TYPE' });
  await assert.rejects(time.profile({ intervalMicros: 0 }), {
    code: 'ERR_OUT_OF_RANGE',
  });
  assert.strictEqual(time.isStarted(), false);

  // Aborting stops the profiler, and rejects without a profile.
  {
    const controller = new AbortController();
    const promise = time.profile({
      durationMillis: 60_000,
      signal: controller.signal,
    });
    assert.strictEqual(time.isStarted(), true);
    controller.abort();
    await assert.rejects(promise, { name: 'AbortError' });
    assert.strictEqual(time.isStarted(), false);
  }

  // A signal that is already aborted stops the profiler that was just
  // started.
  await assert.rejects(time.profile({ signal: AbortSignal.abort() }), {
    name: 'AbortError',
  });
  assert.strictEqual(time.isStarted(), false);

  // Aborting does not stop a profiler that was started by another caller
  // after the profiler of `time.profile()` was stopped.
  {
    const controller = new AbortController();
    const promise = time.profile({
      durationMillis: 60_000,
      signal: controller.signal,
    });
    time.stop();
    time.start();
    controller.abort();
    await assert.rejects(promise, { name: 'AbortError' });
    assert.strictEqual(time.isStarted(), true);
    time.stop();
  }

  // When the profiler of `time.profile()` is stopped before the duration
  // elapses, the promise rejects, and does not stop a profiler that was
  // started by another caller in the meantime.
  {
    const promise = time.profile({ durationMillis: 50 });
    time.stop();
    await assert.rejects(promise, { code: 'ERR_INVALID_STATE' });
    assert.strictEqual(time.isStarted(), false);
  }
  {
    const promise = time.profile({ durationMillis: 50 });
    time.stop();
    time.start();
    await assert.rejects(promise, { code: 'ERR_INVALID_STATE' });
    assert.strictEqual(time.isStarted(), true);
    time.stop();
  }

  // Restarting with `time.stop(true)` keeps the profiler of `time.profile()`,
  // which still resolves.
  {
    const promise = time.profile({ durationMillis: 50 });
    time.stop(true);
    await promise;
    assert.strictEqual(time.isStarted(), false);
  }
})().then(common.mustCall());
