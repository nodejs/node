'use strict';

const common = require('../common');

if (process.config.variables.arm_version === '7') {
  common.skip('Too slow for armv7 bots');
}

const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const env = {
  ...process.env,
  TEST_ALLOCATION: 50000,
  TEST_CHUNK: 1000,
  TEST_CLEAN_INTERVAL: 500,
  NODE_DEBUG_NATIVE: 'diagnostics'
};

{
  console.log('\nTesting limit = 1');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    '--trace-gc',
    '--heapsnapshot-near-heap-limit=1',
    '--max-old-space-size=50',
    fixtures.path('workload', 'bounded.js'),
  ], {
    cwd: tmpdir.path,
    env,
  });
  console.log(child.stdout.toString());
  console.log(child.stderr.toString());
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.status, 0);
  const list = fs.readdirSync(tmpdir.path)
    .filter((file) => file.endsWith('.heapsnapshot'));
  assert.strictEqual(list.length, 0);
}
