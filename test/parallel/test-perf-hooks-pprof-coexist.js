// Flags: --allow-natives-syntax
'use strict';

// Checks that the time profiler works alongside the other CPU profilers:
// v8.startCpuProfile() and the inspector's Profiler domain.

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('node:assert');
const inspector = require('node:inspector');
const v8 = require('node:v8');
const {
  decodeGzippedProfile,
  pprofTestBusyLoop,
} = require('../common/pprof');
const { time, encodeSync } = require('node:perf_hooks/pprof');

// Profiles lack inlined frames of optimized code that was compiled before the
// profiler started: see pprofTestBusyLoop() in test/common/pprof.js.
eval('%NeverOptimizeFunction(pprofTestBusyLoop)');

/**
 * Asserts that a pprof profile contains samples of `pprofTestBusyLoop`.
 * @param {object} profile A profile returned by `time.stop()`.
 * @throws {AssertionError} If the profile has no samples of the function.
 */
function checkPprof(profile) {
  const { samples } = decodeGzippedProfile(encodeSync(profile));
  assert.ok(samples.some(({ stack }) =>
    stack.some(({ name }) => name === 'pprofTestBusyLoop')));
}

/**
 * Asserts that a V8 CPU profile contains a node of `pprofTestBusyLoop`.
 * @param {object} cpuProfile A profile in the V8 CPU profile format.
 * @throws {AssertionError} If the profile has no node of the function.
 */
function checkCpuProfile(cpuProfile) {
  assert.ok(cpuProfile.nodes.some(({ callFrame }) =>
    callFrame.functionName === 'pprofTestBusyLoop'));
}

const collectCpuTime = !common.isWindows;

// v8.startCpuProfile() started before, and stopped before.
{
  const handle = v8.startCpuProfile();
  time.start({ collectCpuTime });
  pprofTestBusyLoop(200_000);
  checkCpuProfile(JSON.parse(handle.stop()));
  pprofTestBusyLoop(200_000);
  checkPprof(time.stop());
}

// v8.startCpuProfile() started after, and stopped after.
{
  time.start({ collectCpuTime });
  const handle = v8.startCpuProfile();
  pprofTestBusyLoop(200_000);
  checkPprof(time.stop(true));
  pprofTestBusyLoop(200_000);
  checkPprof(time.stop());
  checkCpuProfile(JSON.parse(handle.stop()));
}

// The inspector's profiler.
{
  const session = new inspector.Session();
  session.connect();
  session.post('Profiler.enable');
  session.post('Profiler.start', common.mustSucceed(() => {
    time.start({ collectCpuTime });
    pprofTestBusyLoop(200_000);
    session.post('Profiler.stop', common.mustSucceed(({ profile }) => {
      checkCpuProfile(profile);
      pprofTestBusyLoop(200_000);
      checkPprof(time.stop());
      session.disconnect();
    }));
  }));
}
