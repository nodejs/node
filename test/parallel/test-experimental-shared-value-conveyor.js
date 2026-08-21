'use strict';
const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { MessageChannel } = require('worker_threads');

if (process.env.TEST_CHILD_PROCESS === '1') {
  // --harmony-struct implies --shared-string-table. V8 currently does not
  // support JSON.parse in worker isolates with that flag, and Node workers
  // parse process.config during bootstrap.
  const m = new globalThis.SharedArray(16);
  const { port1, port2 } = new MessageChannel();

  port1.once('message', common.mustCall((message) => {
    assert.strictEqual(message, m);
    port1.close();
    port2.close();
  }));

  port2.postMessage(m);
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
