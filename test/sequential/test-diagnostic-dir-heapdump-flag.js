'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');

if (common.isWindows)
  common.skip('test not supported on Windows');

const assert = require('assert');

if (process.argv[2] === 'child') {
  const fs = require('fs');
  const path = require('path');

  assert.strictEqual(process.listenerCount('SIGUSR2'), 1);
  process.kill(process.pid, 'SIGUSR2');
  process.kill(process.pid, 'SIGUSR2');

  // Asynchronously wait for the snapshot. Use an async loop to be a bit more
  // robust in case platform or machine differences throw off the timing.
  (function validate() {
    const files = fs.readdirSync(tmpdir.path);

    if (files.length === 0)
      return setImmediate(validate);

    assert.strictEqual(files.length, 2);

    for (let i = 0; i < files.length; i++) {
      assert.match(files[i], /^Heap\..+\.heapsnapshot$/);
      JSON.parse(fs.readFileSync(path.join(tmpdir.path, files[i])));
    }
  })();
} else {
  const { spawnSync } = require('child_process');

  tmpdir.refresh();
  const args = [
    '--heapsnapshot-signal',
    'SIGUSR2',
    '--diagnostic-dir',
    tmpdir.path,
    __filename,
    'child',
  ];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);
}
