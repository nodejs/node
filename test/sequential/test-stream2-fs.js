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
const fixtures = require('../common/fixtures');
const assert = require('assert');

const fs = require('fs');
const FSReadable = fs.ReadStream;

const path = require('path');
const file = path.resolve(fixtures.path('x1024.txt'));

const size = fs.statSync(file).size;

const expectLengths = [1024];

const Stream = require('stream');

class TestWriter extends Stream {
  constructor() {
    super();
    this.buffer = [];
    this.length = 0;
  }

  write(c) {
    this.buffer.push(c.toString());
    this.length += c.length;
    return true;
  }

  end(c) {
    if (c) this.buffer.push(c.toString());
    this.emit('results', this.buffer);
  }
}

const r = new FSReadable(file);
const w = new TestWriter();

w.on('results', function(res) {
  console.error(res, w.length);
  assert.strictEqual(w.length, size);
  assert.deepStrictEqual(res.map(function(c) {
    return c.length;
  }), expectLengths);
  console.log('ok');
});

r.pipe(w);
