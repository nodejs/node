'use strict';

// Tests that --heap-prof outputs heap profile when event loop is drained.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');

const {
  getHeapProfiles,
  verifyFrames,
  kHeapProfInterval,
  env,
} = require('../common/prof');

{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof',
    '--heap-prof-interval',
    kHeapProfInterval,
    fixtures.path('workload', 'allocation.js'),
  ], {
    cwd: tmpdir.path,
    env
  });
  if (output.status !== 0) {
    console.log(output.stderr.toString());
    console.log(output);
  }
  assert.strictEqual(output.status, 0);
  const profiles = getHeapProfiles(tmpdir.path);
  assert.strictEqual(profiles.length, 1);
  verifyFrames(output, profiles[0], 'runAllocation');
}
