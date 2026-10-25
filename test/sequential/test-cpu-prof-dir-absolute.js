'use strict';

// This tests that relative --cpu-prof-dir works.

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const tmpdir = require('../common/tmpdir');
const {
  getCpuProfiles,
  kCpuProfInterval,
  env,
  verifyFrames,
} = require('../common/cpu-prof');

// relative --cpu-prof-dir
{
  tmpdir.refresh();
  const dir = tmpdir.resolve('prof');
  const { child: output } = spawnSyncAndExitWithoutError(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-dir',
    dir,
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env,
  });
  assert(fs.existsSync(dir));
  const profiles = getCpuProfiles(dir);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci.js');
}
