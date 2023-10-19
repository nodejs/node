'use strict';

// Tests --heap-prof-dir and --heap-prof-name work together.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');
const path = require('path');
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
  const dir = tmpdir.resolve('prof');
  const file = path.join(dir, 'test.heapprofile');
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-name',
    'test.heapprofile',
    '--heap-prof-dir',
    dir,
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
  assert(fs.existsSync(dir));
  const profiles = getHeapProfiles(dir);
  assert.deepStrictEqual(profiles, [file]);
  verifyFrames(output, file, 'runAllocation');
}
