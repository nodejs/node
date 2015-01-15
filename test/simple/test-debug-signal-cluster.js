// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var args = [ common.fixturesDir + '/clustered-server/app.js' ];
var child = spawn(process.execPath, args, {
  stdio: [ 'pipe', 'pipe', 'pipe', 'ipc' ]
});
var outputLines = [];
var outputTimerId;
var waitingForDebuggers = false;

var pids = null;

child.stderr.on('data', function(data) {
  var lines = data.toString().replace(/\r/g, '').trim().split('\n');

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
  if (outputLines.length >= expectedLines.length)
    onNoMoreLines();
});

function onNoMoreLines() {
  assertOutputLines();
  process.exit();
}

setTimeout(function testTimedOut() {
  assert(false, 'test timed out.');
}, 6000).unref();

process.on('exit', function onExit() {
  pids.forEach(function(pid) {
    process.kill(pid);
  });
});

var expectedLines = [
  'Starting debugger agent.',
  'Debugger listening on port ' + 5858,
  'Starting debugger agent.',
  'Debugger listening on port ' + 5859,
  'Starting debugger agent.',
  'Debugger listening on port ' + 5860,
];

function assertOutputLines() {
  // Do not assume any particular order of output messages,
  // since workers can take different amout of time to
  // start up
  outputLines.sort();
  expectedLines.sort();

  assert.equal(outputLines.length, expectedLines.length);
  for (var i = 0; i < expectedLines.length; i++)
    assert.equal(outputLines[i], expectedLines[i]);
}
