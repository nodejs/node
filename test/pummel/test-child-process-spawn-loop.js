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

var SIZE = 1000 * 1024;
var N = 40;
var finished = false;

function doSpawn(i) {
  var child = spawn('python', ['-c', 'print ' + SIZE + ' * "C"']);
  var count = 0;

  child.stdout.setEncoding('ascii');
  child.stdout.addListener('data', function(chunk) {
    count += chunk.length;
  });

  child.stderr.addListener('data', function(chunk) {
    console.log('stderr: ' + chunk);
  });

  child.addListener('exit', function() {
    assert.equal(SIZE + 1, count); // + 1 for \n
    if (i < N) {
      doSpawn(i + 1);
    } else {
      finished = true;
    }
  });
}

doSpawn(0);

process.addListener('exit', function() {
  assert.ok(finished);
});
