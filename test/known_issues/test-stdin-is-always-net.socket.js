'use strict';
// Refs: https://github.com/nodejs/node/pull/5916

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const net = require('net');

if (process.argv[2] === 'child') {
  assert(process.stdin instanceof net.Socket);
  return;
}

const proc = spawn(
  process.execPath,
  [__filename, 'child'],
  { stdio: 'ignore' },
);
// To double-check this test, set stdio to 'pipe' and uncomment the line below.
// proc.stderr.pipe(process.stderr);
proc.on('exit', common.mustCall(function(exitCode) {
  assert.strictEqual(exitCode, 0);
}));
