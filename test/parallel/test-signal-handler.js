'use strict';

const common = require('../common');

if (common.isWindows) {
  common.skip('SIGUSR1 and SIGHUP signals are not supported');
  return;
}

console.log('process.pid: ' + process.pid);

process.on('SIGUSR1', common.mustCall(function() {}));

process.on('SIGUSR1', common.mustCall(function() {
  setTimeout(function() {
    console.log('End.');
    process.exit(0);
  }, 5);
}));

var i = 0;
setInterval(function() {
  console.log('running process...' + ++i);

  if (i === 5) {
    process.kill(process.pid, 'SIGUSR1');
  }
}, 1);

// Test on condition where a watcher for SIGNAL
// has been previously registered, and `process.listeners(SIGNAL).length === 1`
process.on('SIGHUP', function() { common.fail('should not run'); });
process.removeAllListeners('SIGHUP');
process.on('SIGHUP', common.mustCall(function() {}));
process.kill(process.pid, 'SIGHUP');
