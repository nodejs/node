'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');
const { fork } = require('child_process');

// This test should end immediately after `unref` is called
// The pipe will stay open as Node.js completes, thus run in a child process
// so that tmpdir can be cleaned up.

const tmpdir = require('../common/tmpdir');

if (process.argv[2] !== 'child') {
  // Parent
  tmpdir.refresh();

  // Run test
  const child = fork(__filename, ['child'], { stdio: 'inherit' });
  child.on('exit', common.mustCall(function(code) {
    assert.strictEqual(code, 0);
  }));

  return;
}

// Child
const s = net.Server();
s.listen(common.PIPE);
s.unref();
