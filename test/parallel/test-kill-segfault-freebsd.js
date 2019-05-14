'use strict';
require('../common');

// This test ensures Node.js doesn't crash on hitting Ctrl+C in order to
// terminate the currently running process (especially on FreeBSD).
// https://github.com/nodejs/node-v0.x-archive/issues/9326

const assert = require('assert');
const child_process = require('child_process');

// NOTE: Was crashing on FreeBSD
const cp = child_process.spawn(process.execPath, [
  '-e',
  'process.kill(process.pid, "SIGINT")'
]);

cp.on('exit', function(code) {
  assert.notStrictEqual(code, 0);
});
