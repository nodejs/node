'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

// A test to ensure that preload modules are given a chance to execute before
// resolving the main entry point with --inspect-brk active.

const assert = require('assert');
const cp = require('child_process');
const path = require('path');

function test(execArgv) {
  const child = cp.spawn(process.execPath, execArgv);

  child.stderr.once('data', common.mustCall(function() {
    child.kill('SIGTERM');
  }));

  child.on('exit', common.mustCall(function(code, signal) {
    assert.strictEqual(signal, 'SIGTERM');
  }));
}

test([
  '--require',
  path.join(__dirname, '../fixtures/test-resolution-inspect-brk-resolver.js'),
  '--inspect-brk',
  '../fixtures/test-resolution-inspect-resolver-main.ext',
]);
