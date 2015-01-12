var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var debugPort = common.PORT;
var args = ['--debug-port=' + debugPort];
var child = spawn(process.execPath, args);

child.stderr.on('data', function(data) {
  var lines = data.toString().replace(/\r/g, '').trim().split('\n');
  lines.forEach(processStderrLine);
});

setTimeout(testTimedOut, 3000);
function testTimedOut() {
  assert(false, 'test timed out.');
}

// Give the child process small amout of time to start
setTimeout(function() {
  process._debugProcess(child.pid);
}, 100);

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
