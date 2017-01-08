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
  assert.equal(cnt, wclosed, 'writable streams closed');
  assert.equal(cnt, rclosed, 'readable streams closed');
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
