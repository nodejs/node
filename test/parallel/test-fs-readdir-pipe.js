'use strict';

const common = require('../common');
const assert = require('assert');
const { readdir, readdirSync } = require('fs');

if (!common.isWindows) {
  common.skip('This test is specific to Windows to test enumerate pipes');
}

// Ref: https://github.com/nodejs/node/issues/56002
// This test is specific to Windows.

const pipe = '\\\\.\\pipe';

assert.ok(readdirSync(pipe).length >= 0);

readdir(pipe, common.mustCall((err, files) => {
  assert.ok(err == null);
  assert.ok(files.length >= 0);
}));
