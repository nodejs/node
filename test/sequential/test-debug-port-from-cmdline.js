'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

const debugPort = common.PORT;
const args = ['--interactive', '--debug-port=' + debugPort];
const childOptions = { stdio: ['pipe', 'pipe', 'pipe', 'ipc'] };
const child = spawn(process.execPath, args, childOptions);

child.stdin.write("process.send({ msg: 'childready' });\n");

child.stderr.on('data', function(data) {
  const lines = data.toString().replace(/\r/g, '').trim().split('\n');
  lines.forEach(processStderrLine);
});

child.on('message', function onChildMsg(message) {
  if (message.msg === 'childready') {
    process._debugProcess(child.pid);
  }
});

process.on('exit', function() {
  child.kill();
  assertOutputLines();
});

const outputLines = [];
function processStderrLine(line) {
  console.log('> ' + line);
  outputLines.push(line);

  if (/Debugger listening/.test(line)) {
    process.exit();
  }
}

function assertOutputLines() {
  const expectedLines = [
    'Starting debugger agent.',
    'Debugger listening on (\\[::\\]|0\\.0\\.0\\.0):' + debugPort,
  ];

  assert.strictEqual(outputLines.length, expectedLines.length);
  for (let i = 0; i < expectedLines.length; i++)
    assert(RegExp(expectedLines[i]).test(outputLines[i]));
}
