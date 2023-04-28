'use strict';

// This tests that relative --cpu-prof-dir works.

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');
const {
  getCpuProfiles,
  kCpuProfInterval,
  env,
  verifyFrames
} = require('../common/cpu-prof');

// relative --cpu-prof-dir
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-dir',
    'prof',
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const dir = path.join(tmpdir.path, 'prof');
  assert(fs.existsSync(dir));
  const profiles = getCpuProfiles(dir);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci.js');
}
