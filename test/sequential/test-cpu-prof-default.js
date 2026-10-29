'use strict';

// Test --cpu-prof without --cpu-prof-interval. Here we just verify that
// we manage to generate a profile since it's hard to tell whether we
// can sample our target function with the default sampling rate across
// different platforms and machine configurations.

const common = require('../common');
const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const tmpdir = require('../common/tmpdir');
const {
  getCpuProfiles,
  env,
} = require('../common/cpu-prof');

{
  tmpdir.refresh();
  spawnSyncAndExitWithoutError(process.execPath, [
    '--cpu-prof',
    fixtures.path('workload', 'fibonacci.js'),
  ], {
    cwd: tmpdir.path,
    env,
  });
  const profiles = getCpuProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
}
