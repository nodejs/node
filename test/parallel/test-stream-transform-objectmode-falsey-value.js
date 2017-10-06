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

const stream = require('stream');
const PassThrough = stream.PassThrough;

const src = new PassThrough({ objectMode: true });
const tx = new PassThrough({ objectMode: true });
const dest = new PassThrough({ objectMode: true });

const expect = [ -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
const results = [];

dest.on('data', common.mustCall(function(x) {
  results.push(x);
}, expect.length));

src.pipe(tx).pipe(dest);

let i = -1;
const int = setInterval(common.mustCall(function() {
  if (results.length === expect.length) {
    src.end();
    clearInterval(int);
    assert.deepStrictEqual(results, expect);
  } else {
    src.write(i++);
  }
}, expect.length + 1), 1);
