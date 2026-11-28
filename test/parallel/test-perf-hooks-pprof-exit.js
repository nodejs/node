'use strict';

// Checks that processes exit cleanly while the time profiler runs.

const common = require('../common');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const collectCpuTime = !common.isWindows;

for (const exit of [
  // The event loop drains.
  '',
  // process.exit() is called.
  'process.exit(0);',
  // process.exit() is called from a timer, while samples are taken.
  'setTimeout(() => process.exit(0), 50);',
]) {
  for (const workaroundV8Bug of [true, false]) {
    const script = `
      const { time } = require('node:perf_hooks/pprof');
      time.start(${JSON.stringify({ collectCpuTime, workaroundV8Bug })});
      const end = Date.now() + 100;
      while (Date.now() < end);
      ${exit}
    `;
    spawnSyncAndExitWithoutError(process.execPath, [
      // A profiler that is still started at exit is not a leak. Debug builds
      // always verify this, and release builds only with this flag.
      '--verify-base-objects',
      '--no-warnings',
      '-e',
      script,
    ]);
  }
}
