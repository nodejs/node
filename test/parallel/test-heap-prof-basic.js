'use strict';

// Tests --heap-prof without --heap-prof-interval. Here we just verify that
// we manage to generate a profile.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const tmpdir = require('../common/tmpdir');

const {
  getHeapProfiles,
  env
} = require('../common/prof');

{
  tmpdir.refresh();
  spawnSyncAndExitWithoutError(process.execPath, [
    '--heap-prof',
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  const profiles = getHeapProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
}
