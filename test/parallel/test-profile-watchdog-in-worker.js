'use strict';
require('../common');
const fixtures = require('../common/fixtures.js');
const tmpdir = require('../common/tmpdir');
const path = require('path');
const { Worker } = require('worker_threads');

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
      filepath
    },
  },
];

fileList.forEach(({ filename, args }) => {
  new Worker(filename, { workerData: args });
});
