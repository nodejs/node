'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var debugPort = common.PORT;
var args = ['--debug-port=' + debugPort];
var childOptions = { stdio: ['pipe', 'pipe', 'pipe', 'ipc'] };
var child = spawn(process.execPath, args, childOptions);

child.stdin.end("process.send({ msg: 'childready' });");

child.stderr.on('data', function(data) {
  var lines = data.toString().replace(/\r/g, '').trim().split('\n');
  lines.forEach(processStderrLine);
});

child.on('message', function onChildMsg(message) {
  if (message.msg === 'childready') {
    try {
      process._debugProcess(child.pid);
    } catch (e) {
      if (common.isWindows && (e.message ===
            'Not enough storage is available to process this command.' ||
            e.message === 'Access is denied.')) {
        child.kill();
        console.log('Encountered IRPStackSize bug on win32, ignoring test');
        return process.exit();
      }
      throw e;
    }
  }
});

process.on('exit', function() {
  child.kill();
});

var outputLines = [];
function processStderrLine(line) {
  console.log('> ' + line);
  outputLines.push(line);

  if (/Debugger listening/.test(line)) {
    assertOutputLines();
    process.exit();
  }
}

function assertOutputLines() {
  var expectedLines = [
    'Starting debugger agent.',
    'Debugger listening on port ' + debugPort
  ];

  assert.equal(outputLines.length, expectedLines.length);
  for (var i = 0; i < expectedLines.length; i++)
    assert.equal(outputLines[i], expectedLines[i]);

}
