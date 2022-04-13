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
const assert = require('assert');

const Readable = require('stream').Readable;

class MyStream extends Readable {
  constructor(options) {
    super(options);
    this._chunks = 3;
  }

  _read(n) {
    switch (this._chunks--) {
      case 0:
        return this.push(null);
      case 1:
        return setTimeout(() => {
          this.push('last chunk');
        }, 100);
      case 2:
        return this.push('second to last chunk');
      case 3:
        return process.nextTick(() => {
          this.push('first chunk');
        });
      default:
        throw new Error('?');
    }
  }
}

const ms = new MyStream();
const results = [];
ms.on('readable', function() {
  let chunk;
  while (null !== (chunk = ms.read()))
    results.push(String(chunk));
});

const expect = [ 'first chunksecond to last chunk', 'last chunk' ];
process.on('exit', function() {
  assert.strictEqual(ms._chunks, -1);
  assert.deepStrictEqual(results, expect);
  console.log('ok');
});
