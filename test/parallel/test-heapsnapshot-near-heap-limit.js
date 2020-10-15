'use strict';

const common = require('../common');
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
    '--heapsnapshot-near-heap-limit=-15',
    '--max-old-space-size=50',
    fixtures.path('workload', 'grow.js')
  ], {
    cwd: tmpdir.path,
    env,
  });
  assert.strictEqual(child.status, 9);
}

{
  console.log('\nTesting limit = 0');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    '--trace-gc',
    '--heapsnapshot-near-heap-limit=0',
    '--max-old-space-size=50',
    fixtures.path('workload', 'grow.js')
  ], {
    cwd: tmpdir.path,
    env,
  });
  console.log(child.stdout.toString());
  console.log(child.stderr.toString());
  assert(common.nodeProcessAborted(child.status, child.signal),
         'process should have aborted, but did not');
  const list = fs.readdirSync(tmpdir.path)
    .filter((file) => file.endsWith('.heapsnapshot'));
  assert.strictEqual(list.length, 0);
}

{
  console.log('\nTesting limit = 1');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    '--trace-gc',
    '--heapsnapshot-near-heap-limit=1',
    '--max-old-space-size=50',
    fixtures.path('workload', 'grow.js')
  ], {
    cwd: tmpdir.path,
    env,
  });
  console.log(child.stdout.toString());
  console.log(child.stderr.toString());
  assert(common.nodeProcessAborted(child.status, child.signal),
         'process should have aborted, but did not');
  const list = fs.readdirSync(tmpdir.path)
    .filter((file) => file.endsWith('.heapsnapshot'));
  assert.strictEqual(list.length, 1);
}

{
  console.log('\nTesting limit = 3');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    '--trace-gc',
    '--heapsnapshot-near-heap-limit=3',
    '--max-old-space-size=50',
    fixtures.path('workload', 'grow.js')
  ], {
    cwd: tmpdir.path,
    env,
  });
  console.log(child.stdout.toString());
  console.log(child.stderr.toString());
  assert(common.nodeProcessAborted(child.status, child.signal),
         'process should have aborted, but did not');
  const list = fs.readdirSync(tmpdir.path)
    .filter((file) => file.endsWith('.heapsnapshot'));
  assert(list.length > 0 && list.length <= 3);
}


{
  console.log('\nTesting worker');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    fixtures.path('workload', 'grow-worker.js')
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
  // There should be one snapshot taken and then after the
  // snapshot heap limit callback is popped, the OOM callback
  // becomes effective.
  assert(stderr.includes('ERR_WORKER_OUT_OF_MEMORY'));
  const list = fs.readdirSync(tmpdir.path)
    .filter((file) => file.endsWith('.heapsnapshot'));
  assert.strictEqual(list.length, 1);
}
