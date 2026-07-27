'use strict';

const common = require('../common');

if (common.isPi()) {
  common.skip('Too slow for Raspberry Pi devices');
}

const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const fs = require('fs');

tmpdir.refresh();
spawnSync(process.execPath, [
  '--max-old-space-size=50',
  '--heap-prof',
  `--heap-prof-dir=${tmpdir.path}`,
  `--diagnostic-dir=${tmpdir.path}`,
  fixtures.path('workload', 'heap-profile-near-heap-limit-with-heap-prof.js'),
], {
  cwd: tmpdir.path,
});

const profiles = fs.readdirSync(tmpdir.path)
  .filter((file) => file.endsWith('.heapprofile'));
assert.strictEqual(profiles.length, 1);

const profile = JSON.parse(
  fs.readFileSync(tmpdir.resolve(profiles[0]), 'utf8'));
assert(profile.head);
assert(profile.samples.length > 0);
