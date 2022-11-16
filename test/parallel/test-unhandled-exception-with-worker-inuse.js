'use strict';
const common = require('../common');

// https://github.com/nodejs/node/issues/45421
//
// Check that node will not call v8::Isolate::SetIdle() when exiting
// due to an unhandled exception, otherwise the assertion(enabled in
// debug build only) in the SetIdle() will fail.
//
// The root cause of this issue is that before PerIsolateMessageListener()
// is invoked by v8, v8 preserves the vm state, which is JS. However,
// SetIdle() requires the vm state is either EXTERNAL or IDLE when embedder
// calling it.

if (process.argv[2] === 'child') {
  const { Worker } = require('worker_threads');
  new Worker('', { eval: true });
  throw new Error('xxx');
} else {
  const assert = require('assert');
  const { spawnSync } = require('child_process');
  const result = spawnSync(process.execPath, [__filename, 'child']);

  const stderr = result.stderr.toString().trim();
  // Expect error message to be preserved
  assert.match(stderr, /xxx/);
  // Expect no crash
  assert.ok(!common.nodeProcessAborted(result.status, result.signal), stderr);
}
