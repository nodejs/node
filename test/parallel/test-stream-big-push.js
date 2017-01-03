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
const str = 'asdfasdfasdfasdfasdf';

const r = new stream.Readable({
  highWaterMark: 5,
  encoding: 'utf8'
});

let reads = 0;

function _read() {
  if (reads === 0) {
    setTimeout(function() {
      r.push(str);
    }, 1);
    reads++;
  } else if (reads === 1) {
    const ret = r.push(str);
    assert.strictEqual(ret, false);
    reads++;
  } else {
    r.push(null);
  }
}

r._read = common.mustCall(_read, 3);

r.on('end', common.mustCall(function() {}));

// push some data in to start.
// we've never gotten any read event at this point.
const ret = r.push(str);
// should be false.  > hwm
assert(!ret);
let chunk = r.read();
assert.strictEqual(chunk, str);
chunk = r.read();
assert.strictEqual(chunk, null);

r.once('readable', function() {
  // this time, we'll get *all* the remaining data, because
  // it's been added synchronously, as the read WOULD take
  // us below the hwm, and so it triggered a _read() again,
  // which synchronously added more, which we then return.
  chunk = r.read();
  assert.strictEqual(chunk, str + str);

  chunk = r.read();
  assert.strictEqual(chunk, null);
});
