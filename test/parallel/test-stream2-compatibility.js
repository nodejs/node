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
const R = require('_stream_readable');
const W = require('_stream_writable');
const assert = require('assert');

let ondataCalled = 0;

class TestReader extends R {
  constructor() {
    super();
    this._buffer = Buffer.alloc(100, 'x');

    this.on('data', () => {
      ondataCalled++;
    });
  }

  _read(n) {
    this.push(this._buffer);
    this._buffer = Buffer.alloc(0);
  }
}

const reader = new TestReader();
setImmediate(function() {
  assert.strictEqual(ondataCalled, 1);
  console.log('ok');
  reader.push(null);
});

class TestWriter extends W {
  constructor() {
    super();
    this.write('foo');
    this.end();
  }

  _write(chunk, enc, cb) {
    cb();
  }
}

const writer = new TestWriter();

process.on('exit', function() {
  assert.strictEqual(reader.readable, false);
  assert.strictEqual(writer.writable, false);
  console.log('ok');
});
