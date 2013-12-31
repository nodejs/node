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

var port = common.PORT + 1337;

var args = [
  '--debug=' + port,
  common.fixturesDir + '/clustered-server/app.js'
];

var child = spawn(process.execPath, args);
var outputLines = [];

child.stderr.on('data', function(data) {
  var lines = data.toString().replace(/\r/g, '').trim().split('\n');
  var line = lines[0];

  lines.forEach(function(ln) { console.log('> ' + ln) } );

  if (line === 'all workers are running') {
    assertOutputLines();
    process.exit();
  } else {
    outputLines = outputLines.concat(lines);
  }
});

process.on('exit', function onExit() {
  child.kill();
});

var assertOutputLines = common.mustCall(function() {
  var expectedLines = [
    'Debugger listening on port ' + port,
    'Debugger listening on port ' + (port+1),
    'Debugger listening on port ' + (port+2),
  ];

  // Do not assume any particular order of output messages,
  // since workers can take different amout of time to
  // start up
  outputLines.sort();

  assert.equal(outputLines.length, expectedLines.length)
  for (var i = 0; i < expectedLines.length; i++)
    assert.equal(outputLines[i], expectedLines[i]);
});
