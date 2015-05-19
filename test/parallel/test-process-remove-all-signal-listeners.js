'use strict';
if (process.platform === 'win32') {
  // Win32 doesn't have signals, just a kindof emulation, insufficient
  // for this test to apply.
  return;
}

var assert = require('assert');
var spawn = require('child_process').spawn;
var ok;

if (process.argv[2] !== '--do-test') {
  // We are the master, fork a child so we can verify it exits with correct
  // status.
  process.env.DOTEST = 'y';
  var child = spawn(process.execPath, [__filename, '--do-test']);

  child.once('exit', function(code, signal) {
    assert.equal(signal, 'SIGINT');
    ok = true;
  });

  process.on('exit', function() {
    assert(ok);
  });

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
