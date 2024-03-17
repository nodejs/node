// This is a utility that recycles buffers for benchmarking
// the http parser kOnStreamAlloc callback option
// See benchmark/http/server_upload.js
'use strict';

class BufferPool {

  constructor(maxPoolSize = 64 * 1024 * 1024, bufSize = 64 * 1024) {
    this.maxPoolSize = maxPoolSize;
    this.bufSize = bufSize;
    this.pool = [];
    this.stats = {
      alloc: { reuse: 0, new: 0, miss: 0, miss_ranges: {} },
      free: { reuse: 0, full: 0, miss: 0, miss_ranges: {} },
    };
  }

  alloc(length) {
    if (length !== this.bufSize) {
      this._miss(length, this.stats.alloc);
    } else if (!this.pool.length) {
      this.stats.alloc.new += 1;
      return Buffer.allocUnsafeSlow(this.bufSize);
    } else {
      this.stats.alloc.reuse += 1;
      return this.pool.pop();
    }
  }

  free(buf) {
    if (buf.length !== this.bufSize) {
      this._miss(buf.length, this.stats.free);
    } else if (this.pool.length * this.bufSize >= this.maxPoolSize) {
      this.stats.free.full += 1;
    } else {
      this.stats.free.reuse += 1;
      this.pool.push(buf);
    }
  }

  // Count the misses of alloc/free per length range (logarithmic)
  _miss(length, op_stats) {
    const base = 2 ** Math.floor(Math.log2(length));
    const range = `[${base}-${2 * base})`;
    op_stats.miss += 1;
    op_stats.miss_ranges[range] = (op_stats.miss_ranges[range] || 0) + 1;
  }

}


exports.BufferPool = BufferPool;
