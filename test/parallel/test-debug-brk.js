'use strict';

const common = require('../common');
const spawn = require('child_process').spawn;

var procStderr = '';
var agentStdout = '';
var needToSpawnAgent = true;
var needToExit = true;

const procArgs = [`--debug-brk=${common.PORT}`, '-e', '0'];
const proc = spawn(process.execPath, procArgs);
proc.stderr.setEncoding('utf8');

const exitAll = common.mustCall((processes) => {
  processes.forEach((myProcess) => { myProcess.kill(); });
});

proc.stderr.on('data', (chunk) => {
  procStderr += chunk;
  if (/Debugger listening on/.test(procStderr) && needToSpawnAgent) {
    needToSpawnAgent = false;
    const agentArgs = ['debug', `localhost:${common.PORT}`];
    const agent = spawn(process.execPath, agentArgs);
    agent.stdout.setEncoding('utf8');

    agent.stdout.on('data', (chunk) => {
      agentStdout += chunk;
      if (/connecting to .+ ok/.test(agentStdout) && needToExit) {
        needToExit = false;
        exitAll([proc, agent]);
      }
    });
  }
});
