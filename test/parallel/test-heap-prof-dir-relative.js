'use strict';

// Tests relative --heap-prof-dir works.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');
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
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-dir',
    'prof',
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
  const dir = tmpdir.resolve('prof');
  assert(fs.existsSync(dir));
  const profiles = getHeapProfiles(dir);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'runAllocation');
}
