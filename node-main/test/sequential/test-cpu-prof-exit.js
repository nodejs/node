'use strict';

// This tests that --cpu-prof generates CPU profile when
// process.exit(55) exits the process.

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
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
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    fixtures.path('workload', 'fibonacci-exit.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 55) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 55);
  const profiles = getCpuProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci-exit.js');
}
