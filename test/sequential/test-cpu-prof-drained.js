'use strict';

// This tests that --cpu-prof generates CPU profile when event
// loop is drained.
// TODO(joyeecheung): share the fixtures with v8 coverage tests

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const tmpdir = require('../common/tmpdir');
const {
  getCpuProfiles,
  kCpuProfInterval,
  env,
  verifyFrames,
} = require('../common/cpu-prof');

{
  tmpdir.refresh();
  const { child: output } = spawnSyncAndExitWithoutError(process.execPath, [
    '--cpu-prof',
    '--cpu-prof-interval',
    kCpuProfInterval,
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env,
  });
  const profiles = getCpuProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'fibonacci.js');
}
