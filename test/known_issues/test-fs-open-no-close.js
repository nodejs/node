'use strict';

// Refs: https://github.com/nodejs/node/issues/34266
// Failing to close a file should not keep the event loop open.

const common = require('../common');

// This issue only shows up on Raspberry Pi devices in our CI. When this test is
// moved out of known_issues, this check can be removed, as the test should pass
// on all platforms at that point.
const assert = require('assert');
if (process.arch !== 'arm' || process.config.variables.arm_version > 7) {
  assert.fail('This test is for Raspberry Pi devices in CI');
}

const fs = require('fs');

const debuglog = (arg) => {
  console.log(new Date().toLocaleString(), arg);
};

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  fs.open(`${tmpdir.path}/dummy`, 'wx+', common.mustCall((err, fd) => {
    debuglog('fs open() callback');
    assert.ifError(err);
  }));
  debuglog('waiting for callback');
}
