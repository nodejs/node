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

'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('_stream_readable');
const Writable = require('_stream_writable');
const util = require('util');

util.inherits(TestReadable, Readable);
function TestReadable(opt) {
  if (!(this instanceof TestReadable))
    return new TestReadable(opt);
  Readable.call(this, opt);
  this._ended = false;
}

TestReadable.prototype._read = function() {
  if (this._ended)
    this.emit('error', new Error('_read called twice'));
  this._ended = true;
  this.push(null);
};

util.inherits(TestWritable, Writable);
function TestWritable(opt) {
  if (!(this instanceof TestWritable))
    return new TestWritable(opt);
  Writable.call(this, opt);
  this._written = [];
}

TestWritable.prototype._write = function(chunk, encoding, cb) {
  this._written.push(chunk);
  cb();
};

// this one should not emit 'end' until we read() from it later.
const ender = new TestReadable();

// what happens when you pipe() a Readable that's already ended?
const piper = new TestReadable();
// pushes EOF null, and length=0, so this will trigger 'end'
piper.read();

setTimeout(common.mustCall(function() {
  ender.on('end', common.mustCall());
  const c = ender.read();
  assert.strictEqual(c, null);

  const w = new TestWritable();
  w.on('finish', common.mustCall());
  piper.pipe(w);
}), 1);
