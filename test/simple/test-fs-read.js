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
var path = require('path'),
    fs = require('fs'),
    filepath = path.join(common.fixturesDir, 'x.txt'),
    fd = fs.openSync(filepath, 'r'),
    expected = 'xyz\n',
    readCalled = 0;

fs.read(fd, expected.length, 0, 'utf-8', function(err, str, bytesRead) {
  readCalled++;

  assert.ok(!err);
  assert.equal(str, expected);
  assert.equal(bytesRead, expected.length);
});

var r = fs.readSync(fd, expected.length, 0, 'utf-8');
assert.equal(r[0], expected);
assert.equal(r[1], expected.length);

process.addListener('exit', function() {
  assert.equal(readCalled, 1);
});
