'use strict';
const common = require('../common');
const assert = require('assert');

const Stream = require('stream');
const Readable = Stream.Readable;

const r = new Readable();
const N = 256;
let reads = 0;
r._read = function(n) {
  return r.push(++reads === N ? null : Buffer.allocUnsafe(1));
};

r.on('end', common.mustCall(function() {}));

const w = new Stream();
w.writable = true;
let buffered = 0;
w.write = function(c) {
  buffered += c.length;
  process.nextTick(drain);
  return false;
};

function drain() {
  assert(buffered <= 3);
  buffered = 0;
  w.emit('drain');
}

w.end = common.mustCall(function() {});

// Just for kicks, let's mess with the drain count.
// This verifies that even if it gets negative in the
// pipe() cleanup function, we'll still function properly.
r.on('readable', function() {
  w.emit('drain');
});

r.pipe(w);
