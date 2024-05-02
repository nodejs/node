'use strict';
const common = require('../common');


if (common.isWindows)
  common.skip('test not supported on Windows');

const assert = require('assert');

const validateHeapSnapshotFile = () => {
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

      // Check the file is parsable as JSON
      JSON.parse(fs.readFileSync(files[i]));
    }
  })();
};

if (process.argv[2] === 'child') {
  validateHeapSnapshotFile();
} else {
  // Modify the timezone. So we can check the file date string still returning correctly.
  process.env.TZ = 'America/New_York';
  const { spawnSync } = require('child_process');
  const tmpdir = require('../common/tmpdir');

  tmpdir.refresh();
  const args = ['--heapsnapshot-signal', 'SIGUSR2', '--diagnostic-dir', tmpdir.path, __filename, 'child'];
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });

  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);
}
