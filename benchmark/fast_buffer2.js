var SlowBuffer = require('buffer').SlowBuffer;
var POOLSIZE = 8*1024;
var pool;

function allocPool () {
  pool = new SlowBuffer(POOLSIZE);
  pool.used = 0;
}

function FastBuffer (length) {
  this.length = length;

  if (length > POOLSIZE) {
    // Big buffer, just alloc one.
    this.parent = new Buffer(length);
    this.offset = 0;
  } else {
    // Small buffer.
    if (!pool || pool.length - pool.used < length) allocPool();
    this.parent = pool;
    this.offset = pool.used;
    pool.used += length;
  }

  // HERE HERE HERE
  SlowBuffer.makeFastBuffer(this.parent, this, this.offset, this.length);
}

exports.FastBuffer = FastBuffer;

FastBuffer.prototype.get = function (i) {
  if (i < 0 || i >= this.length) throw new Error("oob");
  return this.parent[this.offset + i];
};

FastBuffer.prototype.set = function (i, v) {
  if (i < 0 || i >= this.length) throw new Error("oob");
  return this.parent[this.offset + i] = v;
};

// TODO define slice, toString, write, etc.
// slice should not use c++
