'use strict';

// Tests --heap-prof generates a heap profile from worker
// when execArgv is set.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

const tmpdir = require('../common/tmpdir');

const {
  getHeapProfiles,
  verifyFrames,
} = require('../common/prof');

{
  tmpdir.refresh();
  const { child: output } = spawnSyncAndExitWithoutError(process.execPath, [
    fixtures.path('workload', 'allocation-worker-argv.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
      HEAP_PROF_INTERVAL: '128'
    }
  });
  const profiles = getHeapProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'runAllocation');
}
