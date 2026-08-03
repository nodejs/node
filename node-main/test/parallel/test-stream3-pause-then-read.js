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

const stream = require('stream');
const Readable = stream.Readable;
const Writable = stream.Writable;

const totalChunks = 100;
const chunkSize = 99;
const expectTotalData = totalChunks * chunkSize;
let expectEndingData = expectTotalData;

const r = new Readable({ highWaterMark: 1000 });
let chunks = totalChunks;
r._read = function(n) {
  console.log('_read called', chunks);
  if (!(chunks % 2))
    setImmediate(push);
  else if (!(chunks % 3))
    process.nextTick(push);
  else
    push();
};

let totalPushed = 0;
function push() {
  const chunk = chunks-- > 0 ? Buffer.alloc(chunkSize, 'x') : null;
  if (chunk) {
    totalPushed += chunk.length;
  }
  console.log('chunks', chunks);
  r.push(chunk);
}

read100();

// First we read 100 bytes.
function read100() {
  readn(100, onData);
}

function readn(n, then) {
  console.error(`read ${n}`);
  expectEndingData -= n;
  (function read() {
    const c = r.read(n);
    console.error('c', c);
    if (!c)
      r.once('readable', read);
    else {
      assert.strictEqual(c.length, n);
      assert(!r.readableFlowing);
      then();
    }
  })();
}

// Then we listen to some data events.
function onData() {
  expectEndingData -= 100;
  console.error('onData');
  let seen = 0;
  r.on('data', function od(c) {
    seen += c.length;
    if (seen >= 100) {
      // Seen enough
      r.removeListener('data', od);
      r.pause();
      if (seen > 100) {
        // Oh no, seen too much!
        // Put the extra back.
        const diff = seen - 100;
        r.unshift(c.slice(c.length - diff));
        console.error('seen too much', seen, diff);
      }

      // Nothing should be lost in-between.
      setImmediate(pipeLittle);
    }
  });
}

// Just pipe 200 bytes, then unshift the extra and unpipe.
function pipeLittle() {
  expectEndingData -= 200;
  console.error('pipe a little');
  const w = new Writable();
  let written = 0;
  w.on('finish', () => {
    assert.strictEqual(written, 200);
    setImmediate(read1234);
  });
  w._write = function(chunk, encoding, cb) {
    written += chunk.length;
    if (written >= 200) {
      r.unpipe(w);
      w.end();
      cb();
      if (written > 200) {
        const diff = written - 200;
        written -= diff;
        r.unshift(chunk.slice(chunk.length - diff));
      }
    } else {
      setImmediate(cb);
    }
  };
  r.pipe(w);
}

// Now read 1234 more bytes.
function read1234() {
  readn(1234, resumePause);
}

function resumePause() {
  console.error('resumePause');
  // Don't read anything, just resume and re-pause a whole bunch.
  r.resume();
  r.pause();
  r.resume();
  r.pause();
  r.resume();
  r.pause();
  r.resume();
  r.pause();
  r.resume();
  r.pause();
  setImmediate(pipe);
}


function pipe() {
  console.error('pipe the rest');
  const w = new Writable();
  let written = 0;
  w._write = function(chunk, encoding, cb) {
    written += chunk.length;
    cb();
  };
  w.on('finish', () => {
    console.error('written', written, totalPushed);
    assert.strictEqual(written, expectEndingData);
    assert.strictEqual(totalPushed, expectTotalData);
    console.log('ok');
  });
  r.pipe(w);
}
