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
const child = spawnSync(process.execPath, [
  '--max-old-space-size=50',
  fixtures.path('workload', 'heap-profile-and-snapshot-near-heap-limit.js'),
], {
  cwd: tmpdir.path,
});

const stderr = child.stderr.toString();
assert(common.nodeProcessAborted(child.status, child.signal),
       'process should have aborted, but did not');

const files = fs.readdirSync(tmpdir.path);
const snapshots = files.filter((f) => f.endsWith('.heapsnapshot'));
const profiles = files.filter((f) => f.endsWith('.heapprofile'));

assert(snapshots.length === 1 ||
       stderr.includes('Not generating snapshots because it\'s too risky'));
assert.strictEqual(profiles.length, 1);
