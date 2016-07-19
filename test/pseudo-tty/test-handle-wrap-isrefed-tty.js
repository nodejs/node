'use strict';

const common = require('../common');
const strictEqual = require('assert').strictEqual;
const spawn = require('child_process').spawn;

function makeAssert(message) {
  return function(actual, expected) {
    strictEqual(actual, expected, message);
  };
}
const assert = makeAssert('hasRef() not working on tty_wrap');

if (process.argv[2] === 'child') {
  // Test tty_wrap in piped child to guarentee stdin being a TTY.
  const ReadStream = require('tty').ReadStream;
  const tty = new ReadStream(0);
  assert(Object.getPrototypeOf(tty._handle).hasOwnProperty('hasRef'), true);
  assert(tty._handle.hasRef(), true);
  tty.unref();
  assert(tty._handle.hasRef(), false);
  tty._handle.close(
      common.mustCall(() => assert(tty._handle.hasRef(), false)));
  return;
}

// Use spawn so that we can be sure that stdin has a _handle property.
// Refs: https://github.com/nodejs/node/pull/5916
const proc = spawn(process.execPath, [__filename, 'child'], { stdio: 'pipe' });
proc.stderr.pipe(process.stderr);
proc.on('exit', common.mustCall(function(exitCode) {
  process.exitCode = exitCode;
}));
