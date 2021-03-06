'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  aligned: ['true', 'false'],
  method: ['swap16', 'swap32', 'swap64'/* , 'htons', 'htonl', 'htonll' */],
  len: [64, 256, 768, 1024, 2056, 8192],
  n: [1e6]
}, {
  test: { len: 16 }
});

// The htons and htonl methods below are used to benchmark the
// performance difference between doing the byteswap in pure
// javascript regardless of Buffer size as opposed to dropping
// down to the native layer for larger Buffer sizes. Commented
// out by default because they are slow for big buffers. If
// re-evaluating the crossover point, uncomment those methods
// and comment out their implementations in lib/buffer.js so
// C++ version will always be used.

function swap(b, n, m) {
  const i = b[n];
  b[n] = b[m];
  b[m] = i;
}

Buffer.prototype.htons = function htons() {
  if (this.length % 2 !== 0)
    throw new RangeError();
  for (let i = 0; i < this.length; i += 2) {
    swap(this, i, i + 1);
  }
  return this;
};

Buffer.prototype.htonl = function htonl() {
  if (this.length % 4 !== 0)
    throw new RangeError();
  for (let i = 0; i < this.length; i += 4) {
    swap(this, i, i + 3);
    swap(this, i + 1, i + 2);
  }
  return this;
};

Buffer.prototype.htonll = function htonll() {
  if (this.length % 8 !== 0)
    throw new RangeError();
  for (let i = 0; i < this.length; i += 8) {
    swap(this, i, i + 7);
    swap(this, i + 1, i + 6);
    swap(this, i + 2, i + 5);
    swap(this, i + 3, i + 4);
  }
  return this;
};

function createBuffer(len, aligned) {
  len += aligned ? 0 : 1;
  const buf = Buffer.allocUnsafe(len);
  for (let i = 1; i <= len; i++)
    buf[i - 1] = i;
  return aligned ? buf : buf.slice(1);
}

function genMethod(method) {
  const fnString = `
      return function ${method}(n, buf) {
        for (let i = 0; i <= n; i++)
          buf.${method}();
      }`;
  return (new Function(fnString))();
}

function main({ method, len, n, aligned = 'true' }) {
  const buf = createBuffer(len, aligned === 'true');
  const bufferSwap = genMethod(method);

  bufferSwap(n, buf);
  bench.start();
  bufferSwap(n, buf);
  bench.end(n);
}
