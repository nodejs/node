'use strict';
// SIGUSR1 and SIGHUP are not supported on Windows
if (process.platform === 'win32') {
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');

console.log('process.pid: ' + process.pid);

var first = 0,
    second = 0;

var sighup = false;

process.on('SIGUSR1', function() {
  console.log('Interrupted by SIGUSR1');
  first += 1;
});

process.on('SIGUSR1', function() {
  second += 1;
  setTimeout(function() {
    console.log('End.');
    process.exit(0);
  }, 5);
});

var i = 0;
setInterval(function() {
  console.log('running process...' + ++i);

  if (i == 5) {
    process.kill(process.pid, 'SIGUSR1');
  }
}, 1);

// Test on condition where a watcher for SIGNAL
// has been previously registered, and `process.listeners(SIGNAL).length === 1`
process.on('SIGHUP', function() {});
process.removeAllListeners('SIGHUP');
process.on('SIGHUP', function() { sighup = true; });
process.kill(process.pid, 'SIGHUP');

process.on('exit', function() {
  assert.equal(1, first);
  assert.equal(1, second);
  assert.equal(true, sighup);
});
