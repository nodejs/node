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

// This test verifies that:
// 1. unshift() does not cause colliding _read() calls.
// 2. unshift() after the 'end' event is an error, but after the EOF
//    signalling null, it is ok, and just creates a new readable chunk.
// 3. push() after the EOF signaling null is an error.
// 4. _read() is not called after pushing the EOF null chunk.

const stream = require('stream');
const hwm = 10;
const r = stream.Readable({ highWaterMark: hwm });
const chunks = 10;

const data = Buffer.allocUnsafe(chunks * hwm + Math.ceil(hwm / 2));
for (let i = 0; i < data.length; i++) {
  const c = 'asdf'.charCodeAt(i % 4);
  data[i] = c;
}

let pos = 0;
let pushedNull = false;
r._read = function(n) {
  assert(!pushedNull, '_read after null push');

  // every third chunk is fast
  push(!(chunks % 3));

  function push(fast) {
    assert(!pushedNull, 'push() after null push');
    const c = pos >= data.length ? null : data.slice(pos, pos + n);
    pushedNull = c === null;
    if (fast) {
      pos += n;
      r.push(c);
      if (c === null) pushError();
    } else {
      setTimeout(function() {
        pos += n;
        r.push(c);
        if (c === null) pushError();
      }, 1);
    }
  }
};

function pushError() {
  common.expectsError(function() {
    r.push(Buffer.allocUnsafe(1));
  }, {
    code: 'ERR_STREAM_PUSH_AFTER_EOF',
    type: Error,
    message: 'stream.push() after EOF'
  });
}


const w = stream.Writable();
const written = [];
w._write = function(chunk, encoding, cb) {
  written.push(chunk.toString());
  cb();
};

r.on('end', common.mustCall(function() {
  common.expectsError(function() {
    r.unshift(Buffer.allocUnsafe(1));
  }, {
    code: 'ERR_STREAM_UNSHIFT_AFTER_END_EVENT',
    type: Error,
    message: 'stream.unshift() after end event'
  });
  w.end();
}));

r.on('readable', function() {
  let chunk;
  while (null !== (chunk = r.read(10))) {
    w.write(chunk);
    if (chunk.length > 4)
      r.unshift(Buffer.from('1234'));
  }
});

w.on('finish', common.mustCall(function() {
  // each chunk should start with 1234, and then be asfdasdfasdf...
  // The first got pulled out before the first unshift('1234'), so it's
  // lacking that piece.
  assert.strictEqual(written[0], 'asdfasdfas');
  let asdf = 'd';
  console.error(`0: ${written[0]}`);
  for (let i = 1; i < written.length; i++) {
    console.error(`${i.toString(32)}: ${written[i]}`);
    assert.strictEqual(written[i].slice(0, 4), '1234');
    for (let j = 4; j < written[i].length; j++) {
      const c = written[i].charAt(j);
      assert.strictEqual(c, asdf);
      switch (asdf) {
        case 'a': asdf = 's'; break;
        case 's': asdf = 'd'; break;
        case 'd': asdf = 'f'; break;
        case 'f': asdf = 'a'; break;
      }
    }
  }
}));

process.on('exit', function() {
  assert.strictEqual(written.length, 18);
  console.log('ok');
});
