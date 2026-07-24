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
  fixtures.path('workload', 'heap-profile-and-snapshot-near-heap-limit.js'),
], {
  cwd: tmpdir.path,
  env,
});
console.log(child.stdout.toString());
const stderr = child.stderr.toString();
console.log(stderr);
assert(common.nodeProcessAborted(child.status, child.signal),
       'process should have aborted, but did not');

const snapshots = fs.readdirSync(tmpdir.path)
  .filter((file) => file.endsWith('.heapsnapshot'));
const risky = [...stderr.matchAll(
  /Not generating snapshots because it's too risky/g)].length;
assert(snapshots.length + risky > 0 && snapshots.length <= 1,
       `Generated ${snapshots.length} snapshots and ${risky} was too risky`);

const profile = JSON.parse(
  fs.readFileSync(path.join(tmpdir.path, 'oom.heapprofile'), 'utf8'));
assert(profile.head);
assert(profile.samples.length > 0);
