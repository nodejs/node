'use strict';
// This tests that pausing and resuming stdin does not hang and timeout
// when done in a child process.  See test/parallel/test-stdin-pause-resume.js
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');
const cp = child_process.spawn(
  process.execPath,
  [path.resolve(__dirname, 'test-stdin-pause-resume.js')]
);

cp.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
