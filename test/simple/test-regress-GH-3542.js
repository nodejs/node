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

// This test is only relevant on Windows.
if (process.platform !== 'win32') {
  return process.exit(0);
}

var common = require('../common.js'),
    assert = require('assert'),
    fs = require('fs'),
    path = require('path'),
    succeeded = 0;

function test(p) {
  var result = fs.realpathSync(p);
  assert.strictEqual(result, path.resolve(p));

  fs.realpath(p, function(err, result) {
    assert.ok(!err);
    assert.strictEqual(result, path.resolve(p));
    succeeded++;
  });
}

test('//localhost/c$/windows/system32');
test('//localhost/c$/windows');
test('//localhost/c$/')
test('\\\\localhost\\c$')
test('c:\\');
test('c:');
test(process.env.windir);

process.on('exit', function() {
  assert.strictEqual(succeeded, 7);
});