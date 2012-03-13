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
var is_windows = process.platform === 'win32';

var cat = spawn(is_windows ? 'more' : 'cat');
cat.stdin.write('hello');
cat.stdin.write(' ');
cat.stdin.write('world');

assert.ok(cat.stdin.writable);
assert.ok(!cat.stdin.readable);

cat.stdin.end();

var response = '';
var exitStatus = -1;
var closed = false;

var gotStdoutEOF = false;

cat.stdout.setEncoding('utf8');
cat.stdout.on('data', function(chunk) {
  console.log('stdout: ' + chunk);
  response += chunk;
});

cat.stdout.on('end', function() {
  gotStdoutEOF = true;
});


var gotStderrEOF = false;

cat.stderr.on('data', function(chunk) {
  // shouldn't get any stderr output
  assert.ok(false);
});

cat.stderr.on('end', function(chunk) {
  gotStderrEOF = true;
});


cat.on('exit', function(status) {
  console.log('exit event');
  exitStatus = status;
});

cat.on('close', function () {
  closed = true;
  if (is_windows) {
    assert.equal('hello world\r\n', response);
  } else {
    assert.equal('hello world', response);
  }
});

process.on('exit', function() {
  assert.equal(0, exitStatus);
  assert(closed);
  if (is_windows) {
    assert.equal('hello world\r\n', response);
  } else {
    assert.equal('hello world', response);
  }
});
