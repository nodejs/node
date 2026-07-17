'use strict';
// Tests that os.userInfo correctly handles errors thrown by option property
// getters. See https://github.com/nodejs/node/issues/12370.

const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;

const script = `os.userInfo({
  get encoding() {
    throw new Error('xyz');
  }
})`;

const node = process.execPath;
execFile(node, [ '-e', script ], common.mustCall((err, stdout, stderr) => {
  assert(stderr.includes('Error: xyz'), 'userInfo crashes');
}));
