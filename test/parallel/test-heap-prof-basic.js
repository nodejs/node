'use strict';

// Tests --heap-prof without --heap-prof-interval. Here we just verify that
// we manage to generate a profile.

const common = require('../common');

const fixtures = require('../common/fixtures');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSync } = require('child_process');

const tmpdir = require('../common/tmpdir');

const {
  getHeapProfiles,
  env
} = require('../common/prof');

{
  tmpdir.refresh();
  const output = spawnSync(process.execPath, [
    '--heap-prof',
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
}
