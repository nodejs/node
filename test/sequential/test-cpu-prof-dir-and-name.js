'use strict';

// This tests that --cpu-prof-dir and --cpu-prof-name works together.

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

{
  tmpdir.refresh();
  const dir = tmpdir.resolve('prof');
  const file = path.join(dir, 'test.cpuprofile');
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-name',
    'test.cpuprofile',
    '--cpu-prof-dir',
    dir,
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  assert(fs.existsSync(dir));
  const profiles = getCpuProfiles(dir);
  assert.deepStrictEqual(profiles, [file]);
  verifyFrames(output, file, 'fibonacci.js');
}
