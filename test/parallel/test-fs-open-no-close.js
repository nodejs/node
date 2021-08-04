'use strict';

// Refs: https://github.com/nodejs/node/issues/34266
// Failing to close a file should not keep the event loop open.

const common = require('../common');
const assert = require('assert');

const fs = require('fs');

const debuglog = (arg) => {
  console.log(new Date().toLocaleString(), arg);
};

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

let openFd;

fs.open(`${tmpdir.path}/dummy`, 'wx+', common.mustCall((err, fd) => {
  debuglog('fs open() callback');
  assert.ifError(err);
  openFd = fd;
}));
debuglog('waiting for callback');

process.on('beforeExit', common.mustCall(() => {
  if (openFd) {
    fs.closeSync(openFd);
  }
}));
