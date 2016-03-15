'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['swap16', 'swap32', 'htons', 'htonl'],
  len: [4, 64, 512, 768, 1024, 1536, 2056, 4096, 8192],
  n: [1e6]
});

// The htons and htonl methods below are used to benchmark the
// performance difference between doing the byteswap in pure
// javascript regardless of Buffer size as opposed to dropping
// down to the native layer for larger Buffer sizes.

Buffer.prototype.htons = function htons() {
  if (this.length % 2 !== 0)
    throw new RangeError();
  for (var i = 0, n = 0; i < this.length; i += 2) {
    n = this[i];
    this[i] = this[i + 1];
    this[i + 1] = n;
  }
  return this;
};

Buffer.prototype.htonl = function htonl() {
  if (this.length % 2 !== 0)
    throw new RangeError();
  for (var i = 0, n = 0; i < this.length; i += 4) {
    n = this[i];
    this[i] = this[i + 3];
    this[i + 3] = n;
    n = this[i + 1];
    this[i + 1] = this[i + 2];
    this[i + 2] = n;
  }
  return this;
};

function createBuffer(len) {
  const buf = Buffer.allocUnsafe(len);
  for (var i = 1; i <= len; i++)
    buf[i - 1] = i;
  return buf;
}

function bufferSwap(n, buf, method) {
  for (var i = 1; i <= n; i++)
    buf[method]();
}

function main(conf) {
  const method = conf.method;
  const len = conf.len | 0;
  const n = conf.n | 0;
  const buf = createBuffer(len);
  bench.start();
  bufferSwap(n, buf, method);
  bench.end(n);
}
