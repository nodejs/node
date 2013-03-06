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
var R = require('_stream_readable');
var assert = require('assert');

var fs = require('fs');
var FSReadable = fs.ReadStream;

var path = require('path');
var file = path.resolve(common.fixturesDir, 'x1024.txt');

var size = fs.statSync(file).size;

var expectLengths = [1024];

var util = require('util');
var Stream = require('stream');

util.inherits(TestWriter, Stream);

function TestWriter() {
  Stream.apply(this);
  this.buffer = [];
  this.length = 0;
}

TestWriter.prototype.write = function(c) {
  this.buffer.push(c.toString());
  this.length += c.length;
  return true;
};

TestWriter.prototype.end = function(c) {
  if (c) this.buffer.push(c.toString());
  this.emit('results', this.buffer);
}

var r = new FSReadable(file);
var w = new TestWriter();

w.on('results', function(res) {
  console.error(res, w.length);
  assert.equal(w.length, size);
  var l = 0;
  assert.deepEqual(res.map(function (c) {
    return c.length;
  }), expectLengths);
  console.log('ok');
});

r.pipe(w);
