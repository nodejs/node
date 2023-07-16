'use strict';
const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { Worker, parentPort } = require('worker_threads');

if (process.env.TEST_CHILD_PROCESS === '1') {
  // Do not use isMainThread so that this test itself can be run inside a Worker.
  if (!process.env.HAS_STARTED_WORKER) {
    process.env.HAS_STARTED_WORKER = 1;
    const m = new globalThis.SharedArray(16);

    const worker = new Worker(__filename);
    worker.once('message', common.mustCall((message) => {
      assert.strictEqual(message, m);
    }));

    worker.postMessage(m);
  } else {
    parentPort.once('message', common.mustCall((message) => {
      // Simple echo.
      parentPort.postMessage(message);
    }));
  }
} else {
  if (process.config.variables.v8_enable_pointer_compression === 1) {
    common.skip('--harmony-struct cannot be used with pointer compression');
  }

  const args = ['--harmony-struct', __filename];
  const options = { env: { TEST_CHILD_PROCESS: '1', ...process.env } };
  const child = spawnSync(process.execPath, args, options);

  assert.strictEqual(child.stderr.toString().trim(), '');
  assert.strictEqual(child.stdout.toString().trim(), '');
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);
}
