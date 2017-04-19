'use strict';
const common = require('../common');
const debug = require('_debugger');

const fork = require('child_process').fork;

const scriptToDebug = common.fixturesDir + '/debug-connect.js';

var nodeProcess;

try {
  process._debugProcess(process.pid);
} catch (ex) {
  common.fail('Debugging should not fail');
}


nodeProcess = fork(scriptToDebug);
nodeProcess.send({ pid: process.pid });

nodeProcess.on('message', common.mustCall(function(m) {
  process._debugEnd();
  process.exit(0);
}));
