'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); }

// Regression test for https://github.com/nodejs/node/issues/64274.
// process.exit() joins the V8 platform worker threads without tearing down
// the isolate, so a background task waiting for a main thread GC hung it.

const { spawnSyncAndExitWithoutError } = require('../common/child_process');

if (process.argv[2] === 'child') {
  const keep = [];
  for (let i = 0; i < 1e5; i++) keep.push({ i });
  setTimeout(() => {
    // uv_library_shutdown() waits for this job, keeping the main thread out
    // of JS while the stress task runs into the heap limit and waits for GC.
    require('crypto').pbkdf2('a', 'b', 5e6, 64, 'sha512', () => {});
    process.exit(0);
  }, 300);
  return;
}

spawnSyncAndExitWithoutError(process.execPath, [
  '--stress-concurrent-allocation',
  __filename,
  'child',
], {
  timeout: common.platformTimeout(30_000),
  killSignal: 'SIGKILL',
});
