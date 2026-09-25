'use strict';

// Tests --heap-prof-name works.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');

const {
  getHeapProfiles,
  verifyFrames,
  kHeapProfInterval,
  env
} = require('../common/prof');

{
  tmpdir.refresh();
  const file = tmpdir.resolve('test.heapprofile');
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-name',
    'test.heapprofile',
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const profiles = getHeapProfiles(tmpdir.path);
  assert.deepStrictEqual(profiles, [file]);
  verifyFrames(output, file, 'runAllocation');
}
