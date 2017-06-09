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
// Test that having a bunch of streams piping in parallel
// doesn't break anything.

require('../common');
const assert = require('assert');
const Stream = require('stream').Stream;
const rr = [];
const ww = [];
const cnt = 100;
const chunks = 1000;
const chunkSize = 250;
const data = Buffer.allocUnsafe(chunkSize);
let wclosed = 0;
let rclosed = 0;

function FakeStream() {
  Stream.apply(this);
  this.wait = false;
  this.writable = true;
  this.readable = true;
}

FakeStream.prototype = Object.create(Stream.prototype);

FakeStream.prototype.write = function(chunk) {
  console.error(this.ID, 'write', this.wait);
  if (this.wait) {
    process.nextTick(this.emit.bind(this, 'drain'));
  }
  this.wait = !this.wait;
  return this.wait;
};

FakeStream.prototype.end = function() {
  this.emit('end');
  process.nextTick(this.close.bind(this));
};

// noop - closes happen automatically on end.
FakeStream.prototype.close = function() {
  this.emit('close');
};


// expect all streams to close properly.
process.on('exit', function() {
  assert.strictEqual(cnt, wclosed, 'writable streams closed');
  assert.strictEqual(cnt, rclosed, 'readable streams closed');
});

for (let i = 0; i < chunkSize; i++) {
  data[i] = i;
}

for (let i = 0; i < cnt; i++) {
  const r = new FakeStream();
  r.on('close', function() {
    console.error(this.ID, 'read close');
    rclosed++;
  });
  rr.push(r);

  const w = new FakeStream();
  w.on('close', function() {
    console.error(this.ID, 'write close');
    wclosed++;
  });
  ww.push(w);

  r.ID = w.ID = i;
  r.pipe(w);
}

// now start passing through data
// simulate a relatively fast async stream.
rr.forEach(function(r) {
  let cnt = chunks;
  let paused = false;

  r.on('pause', function() {
    paused = true;
  });

  r.on('resume', function() {
    paused = false;
    step();
  });

  function step() {
    r.emit('data', data);
    if (--cnt === 0) {
      r.end();
      return;
    }
    if (paused) return;
    process.nextTick(step);
  }

  process.nextTick(step);
});
