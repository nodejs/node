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

// This tests that piping stdin will cause it to resume() as well.
var common = require('../common');
var assert = require('assert');

if (process.argv[2] === 'child') {
  process.stdin.pipe(process.stdout);
} else {
  var spawn = require('child_process').spawn;
  var buffers = [];
  var child = spawn(process.execPath, [__filename, 'child']);
  child.stdout.on('data', function(c) {
    buffers.push(c);
  });
  child.stdout.on('close', function() {
    var b = Buffer.concat(buffers).toString();
    assert.equal(b, 'Hello, world\n');
    console.log('ok');
  });
  child.stdin.write('Hel');
  child.stdin.write('lo,');
  child.stdin.write(' wo');
  setTimeout(function() {
    child.stdin.write('rld\n');
    child.stdin.end();
  }, 10);
}

