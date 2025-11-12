// Copy from test-heapsnapshot-near-heap-limit-worker.js
'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const fs = require('fs');

const env = {
  ...process.env,
  NODE_DEBUG_NATIVE: 'diagnostics'
};

{
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    fixtures.path('workload', 'grow-worker-and-set-near-heap-limit.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      TEST_SNAPSHOTS: 1,
      TEST_OLD_SPACE_SIZE: 50,
      ...env
    }
  });
  console.log(child.stdout.toString());
  const stderr = child.stderr.toString();
  console.log(stderr);
  const risky = /Not generating snapshots because it's too risky/.test(stderr);
  if (!risky) {
    // There should be one snapshot taken and then after the
    // snapshot heap limit callback is popped, the OOM callback
    // becomes effective.
    assert(stderr.includes('ERR_WORKER_OUT_OF_MEMORY'));
    const list = fs.readdirSync(tmpdir.path)
      .filter((file) => file.endsWith('.heapsnapshot'));
    assert.strictEqual(list.length, 1);
  }
}
