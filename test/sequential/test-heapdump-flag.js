'use strict';
const common = require('../common');

if (common.isWindows)
  common.skip('test not supported on Windows');

const assert = require('assert');

if (process.argv[2] === 'child') {
  const fs = require('fs');

  assert.strictEqual(process.listenerCount('SIGUSR2'), 1);
  process.kill(process.pid, 'SIGUSR2');
  process.kill(process.pid, 'SIGUSR2');

  // Asynchronously wait for the snapshot. Use an async loop to be a bit more
  // robust in case platform or machine differences throw off the timing.
  (function validate() {
    const files = fs.readdirSync(process.cwd());

    if (files.length === 0)
      return setImmediate(validate);

    assert.strictEqual(files.length, 2);

    for (let i = 0; i < files.length; i++) {
      assert.match(files[i], /^Heap\..+\.heapsnapshot$/);
      JSON.parse(fs.readFileSync(files[i]));
    }
  })();
} else {
  const { spawnSync } = require('child_process');
  const tmpdir = require('../common/tmpdir');

  tmpdir.refresh();
  const args = ['--heapsnapshot-signal', 'SIGUSR2', __filename, 'child'];
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });

  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);
}
