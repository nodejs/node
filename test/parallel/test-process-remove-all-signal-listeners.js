'use strict';

const common = require('../common');
if (common.isWindows)
  common.skip('Win32 does not support signals.');

const assert = require('assert');
const spawn = require('child_process').spawn;

if (process.argv[2] !== '--do-test') {
  // We are the master, fork a child so we can verify it exits with correct
  // status.
  process.env.DOTEST = 'y';
  const child = spawn(process.execPath, [__filename, '--do-test']);

  child.once('exit', common.mustCall(function(code, signal) {
    assert.strictEqual(signal, 'SIGINT');
  }));

  return;
}

process.on('SIGINT', function() {
  // Remove all handlers and kill ourselves. We should terminate by SIGINT
  // now that we have no handlers.
  process.removeAllListeners('SIGINT');
  process.kill(process.pid, 'SIGINT');
});

// Signal handlers aren't sufficient to keep node alive, so resume stdin
process.stdin.resume();

// Demonstrate that signals are being handled
process.kill(process.pid, 'SIGINT');
