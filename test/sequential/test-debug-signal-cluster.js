'use strict';

const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const path = require('path');

const port = common.PORT;
const serverPath = path.join(common.fixturesDir, 'clustered-server', 'app.js');
const args = [`--debug-port=${port}`, serverPath];
const options = { stdio: ['inherit', 'inherit', 'pipe', 'ipc'] };
const child = spawn(process.execPath, args, options);

const outputLines = [];
let waitingForDebuggers = false;

let pids;

child.stderr.on('data', function(data) {
  const lines = data.toString().replace(/\r/g, '').trim().split('\n');

  lines.forEach(function(line) {
    console.log('> ' + line);

    if (line === 'all workers are running') {
      child.on('message', function(msg) {
        if (msg.type !== 'pids')
          return;

        pids = msg.pids;
        console.error('got pids %j', pids);

        waitingForDebuggers = true;
        process._debugProcess(child.pid);
      });

      child.send({
        type: 'getpids'
      });
    } else if (waitingForDebuggers) {
      outputLines.push(line);
    }

  });
  if (outputLines.length === expectedLines.length)
    onNoMoreLines();
});

function onNoMoreLines() {
  assertOutputLines();
  process.exit();
}

process.on('exit', function onExit() {
  // Kill processes in reverse order to avoid timing problems on Windows where
  // the parent process is killed before the children.
  pids.reverse().forEach(function(pid) {
    process.kill(pid);
  });
});

const expectedLines = [
  'Starting debugger agent.',
  'Debugger listening on (\\[::\\]|0\\.0\\.0\\.0):' + (port + 0),
  'Starting debugger agent.',
  'Debugger listening on (\\[::\\]|0\\.0\\.0\\.0):' + (port + 1),
  'Starting debugger agent.',
  'Debugger listening on (\\[::\\]|0\\.0\\.0\\.0):' + (port + 2),
];

function assertOutputLines() {
  // Do not assume any particular order of output messages,
  // since workers can take different amout of time to
  // start up
  outputLines.sort();
  expectedLines.sort();

  assert.strictEqual(outputLines.length, expectedLines.length);
  for (let i = 0; i < expectedLines.length; i++)
    assert(RegExp(expectedLines[i]).test(outputLines[i]));
}
