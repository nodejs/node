// Copy from test-heapsnapshot-near-heap-limit.js
'use strict';

const common = require('../common');

if (common.isPi) {
  common.skip('Too slow for Raspberry Pi devices');
}

const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const v8 = require('v8');

const invalidValues = [-1, '', {}, NaN, undefined];
for (let i = 0; i < invalidValues.length; i++) {
  assert.throws(() => v8.setHeapSnapshotNearHeapLimit(invalidValues[i]),
                /ERR_INVALID_ARG_TYPE|ERR_OUT_OF_RANGE/);
}

// Set twice
v8.setHeapSnapshotNearHeapLimit(1);
v8.setHeapSnapshotNearHeapLimit(2);

const env = {
  ...process.env,
  NODE_DEBUG_NATIVE: 'diagnostics',
};

{
  console.log('\nTesting set by cmd option and api');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    '--trace-gc',
    '--heapsnapshot-near-heap-limit=1',
    '--max-old-space-size=50',
    fixtures.path('workload', 'grow-and-set-near-heap-limit.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...env,
      limit: 2,
    },
  });
  console.log(child.stdout.toString());
  const stderr = child.stderr.toString();
  console.log(stderr);
  assert(common.nodeProcessAborted(child.status, child.signal),
         'process should have aborted, but did not');
  const list = fs.readdirSync(tmpdir.path)
    .filter((file) => file.endsWith('.heapsnapshot'));
  const risky = [...stderr.matchAll(
    /Not generating snapshots because it's too risky/g)].length;
  assert(list.length + risky > 0 && list.length <= 1,
         `Generated ${list.length} snapshots ` +
                     `and ${risky} was too risky`);
}

{
  console.log('\nTesting limit = 1');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    '--trace-gc',
    '--max-old-space-size=50',
    fixtures.path('workload', 'grow-and-set-near-heap-limit.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...env,
      limit: 1,
    },
  });
  console.log(child.stdout.toString());
  const stderr = child.stderr.toString();
  console.log(stderr);
  assert(common.nodeProcessAborted(child.status, child.signal),
         'process should have aborted, but did not');
  const list = fs.readdirSync(tmpdir.path)
    .filter((file) => file.endsWith('.heapsnapshot'));
  const risky = [...stderr.matchAll(
    /Not generating snapshots because it's too risky/g)].length;
  assert(list.length + risky > 0 && list.length <= 1,
         `Generated ${list.length} snapshots ` +
                     `and ${risky} was too risky`);
}

{
  console.log('\nTesting set limit twice');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    '--trace-gc',
    '--max-old-space-size=50',
    fixtures.path('workload', 'grow-and-set-near-heap-limit.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...env,
      limit: 1,
      limit2: 2,
    },
  });
  console.log(child.stdout.toString());
  const stderr = child.stderr.toString();
  console.log(stderr);
  assert(common.nodeProcessAborted(child.status, child.signal),
         'process should have aborted, but did not');
  const list = fs.readdirSync(tmpdir.path)
    .filter((file) => file.endsWith('.heapsnapshot'));
  const risky = [...stderr.matchAll(
    /Not generating snapshots because it's too risky/g)].length;
  assert(list.length + risky > 0 && list.length <= 1,
         `Generated ${list.length} snapshots ` +
                     `and ${risky} was too risky`);
}

{
  console.log('\nTesting limit = 3');
  tmpdir.refresh();
  const child = spawnSync(process.execPath, [
    '--trace-gc',
    '--max-old-space-size=50',
    fixtures.path('workload', 'grow-and-set-near-heap-limit.js'),
  ], {
    cwd: tmpdir.path,
    env: {
      ...env,
      limit: 3,
    },
  });
  console.log(child.stdout.toString());
  const stderr = child.stderr.toString();
  console.log(stderr);
  assert(common.nodeProcessAborted(child.status, child.signal),
         'process should have aborted, but did not');
  const list = fs.readdirSync(tmpdir.path)
    .filter((file) => file.endsWith('.heapsnapshot'));
  const risky = [...stderr.matchAll(
    /Not generating snapshots because it's too risky/g)].length;
  assert(list.length + risky > 0 && list.length <= 3,
         `Generated ${list.length} snapshots ` +
        `and ${risky} was too risky`);
}
