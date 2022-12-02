'use strict';
require('../common');
const fixtures = require('../common/fixtures.js');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');
const { MemoryProfileWatchdog } = require('v8');

const invalidateList = [
  undefined,
  {},
  {
    interval: 1000,
    maxRss: 1024 * 1024,
  },
  {
    maxRss: 1024 * 1024,
    filename: 'dummy.heapsnapshot',
  },
  {
    interval: 1000,
    filename: 'dummy.heapsnapshot',
  },
];
invalidateList.forEach((arg) => {
  assert.throws(() => new MemoryProfileWatchdog(arg), /ERR_INVALID_ARG_TYPE|ERR_INVALID_ARG_VALUE|ERR_OUT_OF_RANGE/);
});

tmpdir.refresh();
const filepath = path.join(tmpdir.path, `${process.pid}.heapsnapshot`);
const fileList = [
  {

    filename: fixtures.path('/profile-watchdog/stop-after-start.js'),
  },
  {

    filename: fixtures.path('/profile-watchdog/stop-before-timeout.js'),
  },
  {

    filename: fixtures.path('/profile-watchdog/stop-after-timeout.js'),
  },
  {

    filename: fixtures.path('/profile-watchdog/take-snapshot.js'),
    args: {
      env: {
        ...process.env,
        filepath,
      },
    },
  },
];

fileList.forEach(({ filename, args, check }) => {
  const proc = cp.spawnSync(process.execPath, [ filename ], { ...args });
  if (proc.status !== 0) {
    console.log(proc.stderr.toString());
  }
  assert.ok(proc.status === 0);
});
