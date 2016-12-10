'use strict';

const common = require('../common');
const spawn = require('child_process').spawn;

let run = () => {};
function test(extraArgs, stdoutPattern) {
  const next = run;
  run = () => {
    var procStdout = '';
    var procStderr = '';
    var agentStdout = '';
    var debuggerListening = false;
    var outputMatched = false;
    var needToSpawnAgent = true;
    var needToExit = true;

    const procArgs = [`--debug-brk=${common.PORT}`].concat(extraArgs);
    const proc = spawn(process.execPath, procArgs);
    proc.stderr.setEncoding('utf8');

    const tryStartAgent = () => {
      if (debuggerListening && outputMatched && needToSpawnAgent) {
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
    };

    const exitAll = common.mustCall((processes) => {
      processes.forEach((myProcess) => { myProcess.kill(); });
    });

    if (stdoutPattern != null) {
      proc.stdout.on('data', (chunk) => {
        procStdout += chunk;
        outputMatched = outputMatched || stdoutPattern.test(procStdout);
        tryStartAgent();
      });
    } else {
      outputMatched = true;
    }

    proc.stderr.on('data', (chunk) => {
      procStderr += chunk;
      debuggerListening = debuggerListening ||
          /Debugger listening on/.test(procStderr);
      tryStartAgent();
    });

    proc.on('exit', () => {
      next();
    });
  };
}

test(['-e', '0']);
test(['-e', '0', 'foo']);
test(['-p', 'process.argv[1]', 'foo'], /^\s*foo\s*$/);

run();
