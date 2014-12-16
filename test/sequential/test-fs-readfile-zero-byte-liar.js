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
var fs = require('fs');

var dataExpected = fs.readFileSync(__filename, 'utf8');

// sometimes stat returns size=0, but it's a lie.
fs._fstat = fs.fstat;
fs._fstatSync = fs.fstatSync;

fs.fstat = function(fd, cb) {
  fs._fstat(fd, function(er, st) {
    if (er) return cb(er);
    st.size = 0;
    return cb(er, st);
  });
};

fs.fstatSync = function(fd) {
  var st = fs._fstatSync;
  st.size = 0;
  return st;
};

var d = fs.readFileSync(__filename, 'utf8');
assert.equal(d, dataExpected);

var called = false;
fs.readFile(__filename, 'utf8', function (er, d) {
  assert.equal(d, dataExpected);
  called = true;
});

process.on('exit', function() {
  assert(called);
  console.log("ok");
});
