'use strict';
var common = require('../common');
var debug = require('_debugger');

var fork = require('child_process').fork;

var scriptToDebug = common.fixturesDir + '/debug-connect.js';

var nodeProcess;

var failed = true;
try {
  process._debugProcess(process.pid);
  failed = false;
} finally {
  // At least TRY not to leave zombie procs if this fails.
  if (failed)
    process.kill();
}
console.log('>>> starting debugger session');

nodeProcess = fork(scriptToDebug);
nodeProcess.send({ pid: process.pid });

nodeProcess.on('message', function(m) {
  process._debugEnd();
  process.exit();
});
