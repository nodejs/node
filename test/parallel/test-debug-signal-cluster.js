'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const os = require('os');
const path = require('path');

const port = common.PORT;
const serverPath = path.join(common.fixturesDir, 'clustered-server', 'app.js');
const args = [`--debug-port=${port}`, serverPath];
const options = { stdio: ['inherit', 'inherit', 'pipe', 'ipc'] };
const child = spawn(process.execPath, args, options);

var expectedContent = [
  'Starting debugger agent.',
  'Debugger listening on 127.0.0.1:' + (port + 0),
  'Starting debugger agent.',
  'Debugger listening on 127.0.0.1:' + (port + 1),
  'Starting debugger agent.',
  'Debugger listening on 127.0.0.1:' + (port + 2),
].join(os.EOL);
expectedContent += os.EOL; // the last line also contains an EOL character

var debuggerAgentsOutput = '';
var debuggerAgentsStarted = false;

var pids;

child.stderr.on('data', function(data) {
  const childStderrOutputString = data.toString();
  const lines = childStderrOutputString.replace(/\r/g, '').trim().split('\n');

  lines.forEach(function(line) {
    console.log('> ' + line);

    if (line === 'all workers are running') {
      child.on('message', function(msg) {
        if (msg.type !== 'pids')
          return;

        pids = msg.pids;
        console.error('got pids %j', pids);

        process._debugProcess(child.pid);
        debuggerAgentsStarted = true;
      });

      child.send({
        type: 'getpids'
      });
    }
  });

  if (debuggerAgentsStarted) {
    debuggerAgentsOutput += childStderrOutputString;
    if (debuggerAgentsOutput.length === expectedContent.length) {
      onNoMoreDebuggerAgentsOutput();
    }
  }
});

function onNoMoreDebuggerAgentsOutput() {
  assertDebuggerAgentsOutput();
  process.exit();
}

process.on('exit', function onExit() {
  // Kill processes in reverse order to avoid timing problems on Windows where
  // the parent process is killed before the children.
  pids.reverse().forEach(function(pid) {
    process.kill(pid);
  });
});

function assertDebuggerAgentsOutput() {
  // Workers can take different amout of time to start up, and child processes'
  // output may be interleaved arbitrarily. Moreover, child processes' output
  // may be written using an arbitrary number of system calls, and no assumption
  // on buffering or atomicity of output should be made. Thus, we process the
  // output of all child processes' debugger agents character by character, and
  // remove each character from the set of expected characters. Once all the
  // output from all debugger agents has been processed, we consider that we got
  // the content we expected if there's no character left in the initial
  // expected content.
  debuggerAgentsOutput.split('').forEach(function gotChar(char) {
    expectedContent = expectedContent.replace(char, '');
  });

  assert.strictEqual(expectedContent, '');
}
