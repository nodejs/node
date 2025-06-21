'use strict';

// This tests that --cpu-prof-dir works for workers.

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
  getFrames
} = require('../common/cpu-prof');

// --cpu-prof-dir with worker
{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-dir',
    'prof',
    '--cpu-prof',
    fixtures.path('workload', 'fibonacci-worker.js'),
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
  const profiles = getCpuProfiles(dir);
  assert.strictEqual(profiles.length, 2);
  const profile1 = getFrames(profiles[0], 'fibonacci.js');
  const profile2 = getFrames(profiles[1], 'fibonacci.js');
  if (profile1.frames.length === 0 && profile2.frames.length === 0) {
    // Show native debug output and the profile for debugging.
    console.log(output.stderr.toString());
    console.log('CPU path: ', profiles[0]);
    console.log(profile1.nodes);
    console.log('CPU path: ', profiles[1]);
    console.log(profile2.nodes);
  }
  assert(profile1.frames.length > 0 || profile2.frames.length > 0);
}
