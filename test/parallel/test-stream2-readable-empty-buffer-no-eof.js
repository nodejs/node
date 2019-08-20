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

test1();
test2();

function test1() {
  const r = new Readable();

  // Should not end when we get a Buffer.alloc(0) or '' as the _read
  // result that just means that there is *temporarily* no data, but to
  // go ahead and try again later.
  //
  // note that this is very unusual.  it only works for crypto streams
  // because the other side of the stream will call read(0) to cycle
  // data through openssl.  that's why setImmediate() is used to call
  // r.read(0) again later, otherwise there is no more work being done
  // and the process just exits.

  const buf = Buffer.alloc(5, 'x');
  let reads = 5;
  r._read = function(n) {
    switch (reads--) {
      case 5:
        return setImmediate(() => {
          return r.push(buf);
        });
      case 4:
        setImmediate(() => {
          return r.push(Buffer.alloc(0));
        });
        return setImmediate(r.read.bind(r, 0));
      case 3:
        setImmediate(r.read.bind(r, 0));
        return process.nextTick(() => {
          return r.push(Buffer.alloc(0));
        });
      case 2:
        setImmediate(r.read.bind(r, 0));
        return r.push(Buffer.alloc(0)); // Not-EOF!
      case 1:
        return r.push(buf);
      case 0:
        return r.push(null); // EOF
      default:
        throw new Error('unreachable');
    }
  };

  const results = [];
  function flow() {
    let chunk;
    while (null !== (chunk = r.read()))
      results.push(String(chunk));
  }
  r.on('readable', flow);
  r.on('end', () => {
    results.push('EOF');
  });
  flow();

  process.on('exit', () => {
    assert.deepStrictEqual(results, [ 'xxxxx', 'xxxxx', 'EOF' ]);
    console.log('ok');
  });
}

function test2() {
  const r = new Readable({ encoding: 'base64' });
  let reads = 5;
  r._read = function(n) {
    if (!reads--)
      return r.push(null); // EOF
    else
      return r.push(Buffer.from('x'));
  };

  const results = [];
  function flow() {
    let chunk;
    while (null !== (chunk = r.read()))
      results.push(String(chunk));
  }
  r.on('readable', flow);
  r.on('end', () => {
    results.push('EOF');
  });
  flow();

  process.on('exit', () => {
    assert.deepStrictEqual(results, [ 'eHh4', 'eHg=', 'EOF' ]);
    console.log('ok');
  });
}
