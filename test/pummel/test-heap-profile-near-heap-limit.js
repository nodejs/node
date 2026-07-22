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
const path = require('path');
const env = {
  ...process.env,
  NODE_DEBUG_NATIVE: 'diagnostics',
};

tmpdir.refresh();
const child = spawnSync(process.execPath, [
  '--max-old-space-size=50',
  fixtures.path('workload', 'heap-profile-near-heap-limit.js'),
], {
  cwd: tmpdir.path,
  env,
});
// Surface the V8 abort trace when the assertions below fail.
console.log(child.stdout.toString());
console.log(child.stderr.toString());
assert.strictEqual(child.status, 0);

const profile = JSON.parse(
  fs.readFileSync(path.join(tmpdir.path, 'oom.heapprofile'), 'utf8'));
assert(profile.head);
assert(profile.samples.length > 0);
