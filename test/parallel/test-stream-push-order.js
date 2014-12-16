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

var common = require('../common.js');
var Readable = require('stream').Readable;
var assert = require('assert');

var s = new Readable({
  highWaterMark: 20,
  encoding: 'ascii'
});

var list = ['1', '2', '3', '4', '5', '6'];

s._read = function (n) {
  var one = list.shift();
  if (!one) {
    s.push(null);
  } else {
    var two = list.shift();
    s.push(one);
    s.push(two);
  }
};

var v = s.read(0);

// ACTUALLY [1, 3, 5, 6, 4, 2]

process.on("exit", function () {
  assert.deepEqual(s._readableState.buffer,
                   ['1', '2', '3', '4', '5', '6']);
  console.log("ok");
});
