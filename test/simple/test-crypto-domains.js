// Copyright Joyent, Inc. and other Node contributors.

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var crypto = require('crypto');
var domain = require('domain');
var assert = require('assert');
var d = domain.create();
var expect = ['pbkdf2', 'randomBytes', 'pseudoRandomBytes']

d.on('error', function (e) {
  var idx = expect.indexOf(e.message);
  assert.notEqual(idx, -1, 'we should have error: ' + e.message);
  expect.splice(idx, 1);
});

d.run(function () {
  crypto.pbkdf2('a', 'b', 1, 8, function () {
    throw new Error('pbkdf2');
  });

  crypto.randomBytes(4, function () {
    throw new Error('randomBytes');
  });

  crypto.pseudoRandomBytes(4, function () {
    throw new Error('pseudoRandomBytes');
  });
});

process.on('exit', function () {
  assert.strictEqual(expect.length, 0, 'we should have seen all error messages');
});
