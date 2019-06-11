'use strict';

// This tests that --cpu-prof-name can be used to specify the
// name of the generated CPU profile.

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const path = require('path');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');
const {
  getCpuProfiles,
  kCpuProfInterval,
  env,
  verifyFrames
} = require('../common/cpu-prof');

// --cpu-prof-name
{
  tmpdir.refresh();
  const file = path.join(tmpdir.path, 'test.cpuprofile');
  const output = spawnSync(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    '--cpu-prof-name',
    'test.cpuprofile',
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
  }
  assert.strictEqual(output.status, 0);
  const profiles = getCpuProfiles(tmpdir.path);
  assert.deepStrictEqual(profiles, [file]);
  verifyFrames(output, file, 'fibonacci.js');
}
