// Throughput benchmark
// creates a single HMAC, then pushes a bunch of data through it
'use strict';

const common = require('../common.js');
const { createHmac } = require('crypto');

const bench = common.createBenchmark(main, {
  n: [500],
  algo: ['sha1', 'sha256', 'sha512'],
  keylen: [64],
  type: ['asc', 'utf', 'buf'],
  len: [2, 1024, 102400, 1024 * 1024],
  api: ['update', 'stream'],
});

function main({ api, type, len, algo, keylen, n }) {
  let message;
  let encoding;
  switch (type) {
    case 'asc':
      message = 'a'.repeat(len);
      encoding = 'ascii';
      break;
    case 'utf':
      message = '\u00fc'.repeat(len / 2);
      encoding = 'utf8';
      break;
    case 'buf':
      message = Buffer.alloc(len, 'b');
      break;
    default:
      throw new Error(`unknown message type: ${type}`);
  }

  const fn = api === 'stream' ? streamWrite : updateDigest;
  const key = Buffer.alloc(keylen, 'k');

  bench.start();
  fn(algo, key, message, encoding, n, len);
}

function updateDigest(algo, key, message, encoding, n, len) {
  const written = n * len;
  const bits = written * 8;
  const gbits = bits / (1024 * 1024 * 1024);
  const h = createHmac(algo, key);

  while (n-- > 0)
    h.update(message, encoding);

  h.digest();

  bench.end(gbits);
}

function streamWrite(algo, key, message, encoding, n, len) {
  const written = n * len;
  const bits = written * 8;
  const gbits = bits / (1024 * 1024 * 1024);
  const h = createHmac(algo, key);

  while (n-- > 0)
    h.write(message, encoding);

  h.end();
  h.read();

  bench.end(gbits);
}
