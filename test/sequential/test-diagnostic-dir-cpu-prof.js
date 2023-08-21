'use strict';

// This test is to ensure that --diagnostic-dir does not change the directory
// for --cpu-prof when --cpu-prof-dir is specified

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const fs = require('fs');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');
const {
  getCpuProfiles,
  kCpuProfInterval,
  env,
  verifyFrames
} = require('../common/cpu-prof');

// Test --diagnostic-dir changes the default for --cpu-prof

{
  tmpdir.refresh();
  const dir = tmpdir.resolve('prof');
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--diagnostic-dir',
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
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci.js');
}

// Test --cpu-prof-dir overwrites --diagnostic-dir

{
  tmpdir.refresh();
  const dir = tmpdir.resolve('diag');
  const dir2 = tmpdir.resolve('prof');
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--diagnostic-dir',
    dir,
    '--cpu-prof-dir',
    dir2,
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  assert(fs.existsSync(dir2));
  const profiles = getCpuProfiles(dir2);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci.js');
}
