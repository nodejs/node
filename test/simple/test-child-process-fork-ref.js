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
var fork = require('child_process').fork;

if (process.argv[2] === 'child') {
  process.send('1');

  // check that child don't instantly die
  setTimeout(function() {
    process.send('2');
  }, 200);

  process.on('disconnect', function () {
    process.stdout.write('3');
  });

} else {
  var child = fork(__filename, ['child'], {silent: true});

  var ipc = [], stdout = '';

  child.on('message', function (msg) {
    ipc.push(msg);

    if (msg === '2') child.disconnect();
  });

  child.stdout.on('data', function (chunk) {
    stdout += chunk;
  });

  child.once('exit', function () {
    assert.deepEqual(ipc, ['1', '2']);
    assert.equal(stdout, '3');
  });
}
