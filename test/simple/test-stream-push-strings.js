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

var Readable = require('stream').Readable;
var util = require('util');

util.inherits(MyStream, Readable);
function MyStream(options) {
  Readable.call(this, options);
  this._chunks = 3;
}

MyStream.prototype._read = function(n) {
  switch (this._chunks--) {
    case 0:
      return this.push(null);
    case 1:
      return setTimeout(function() {
        this.push('last chunk');
      }.bind(this), 100);
    case 2:
      return this.push('second to last chunk');
    case 3:
      return process.nextTick(function() {
        this.push('first chunk');
      }.bind(this));
    default:
      throw new Error('?');
  }
};

var ms = new MyStream();
var results = [];
ms.on('readable', function() {
  var chunk;
  while (null !== (chunk = ms.read()))
    results.push(chunk + '');
});

var expect = [ 'first chunksecond to last chunk', 'last chunk' ];
process.on('exit', function() {
  assert.equal(ms._chunks, -1);
  assert.deepEqual(results, expect);
  console.log('ok');
});
