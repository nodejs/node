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

require('../common');
const util = require('util');
const stream = require('stream');


function Read() {
  stream.Readable.call(this);
}
util.inherits(Read, stream.Readable);

Read.prototype._read = function(size) {
  this.push('x');
  this.push(null);
};


function Write() {
  stream.Writable.call(this);
}
util.inherits(Write, stream.Writable);

Write.prototype._write = function(buffer, encoding, cb) {
  this.emit('error', new Error('boom'));
  this.emit('alldone');
};

const read = new Read();
const write = new Write();

write.once('error', () => {});
write.once('alldone', function(err) {
  console.log('ok');
});

process.on('exit', function(c) {
  console.error('error thrown even with listener');
});

read.pipe(write);
