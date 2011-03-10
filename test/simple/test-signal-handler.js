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

console.log('process.pid: ' + process.pid);

var first = 0,
    second = 0;

var sighup = false;

process.addListener('SIGUSR1', function() {
  console.log('Interrupted by SIGUSR1');
  first += 1;
});

process.addListener('SIGUSR1', function() {
  second += 1;
  setTimeout(function() {
    console.log('End.');
    process.exit(0);
  }, 5);
});

var i = 0;
setInterval(function() {
  console.log('running process...' + ++i);

  if (i == 5) {
    process.kill(process.pid, 'SIGUSR1');
  }
}, 1);

// Test addListener condition where a watcher for SIGNAL
// has been previously registered, and `process.listeners(SIGNAL).length === 1`
process.addListener('SIGHUP', function () {});
process.removeAllListeners('SIGHUP');
process.addListener('SIGHUP', function () { sighup = true });
process.kill(process.pid, 'SIGHUP');

process.addListener('exit', function() {
  assert.equal(1, first);
  assert.equal(1, second);
  assert.equal(true, sighup);
});
