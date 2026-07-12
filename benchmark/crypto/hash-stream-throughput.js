// Throughput benchmark
// creates a single hasher, then pushes a bunch of data through it
'use strict';
const common = require('../common.js');
const crypto = require('crypto');

const bench = common.createBenchmark(main, {
  n: [500],
  algo: ['sha1', 'sha256', 'sha512'],
  type: ['asc', 'utf', 'buf'],
  len: [2, 1024, 102400, 1024 * 1024],
  api: ['legacy', 'stream'],
});

function main({ api, type, len, algo, n }) {
  if (api === 'stream' && /^v0\.[0-8]\./.test(process.version)) {
    console.error('Crypto streams not available until v0.10');
    // Use the legacy, just so that we can compare them.
    api = 'legacy';
  }

  let message;
  let encoding;
  switch (type) {
    case 'asc':
      message = 'a'.repeat(len);
      encoding = 'ascii';
      break;
    case 'utf':
      message = 'ü'.repeat(len / 2);
      encoding = 'utf8';
      break;
    case 'buf':
      message = Buffer.alloc(len, 'b');
      break;
    default:
      throw new Error(`unknown message type: ${type}`);
  }

  const fn = api === 'stream' ? streamWrite : legacyWrite;

  bench.start();
  fn(algo, message, encoding, n, len);
}

function legacyWrite(algo, message, encoding, n, len) {
  const written = n * len;
  const bits = written * 8;
  const gbits = bits / (1024 * 1024 * 1024);
  const h = crypto.createHash(algo);

  while (n-- > 0)
    h.update(message, encoding);

  h.digest();

  bench.end(gbits);
}

function streamWrite(algo, message, encoding, n, len) {
  const written = n * len;
  const bits = written * 8;
  const gbits = bits / (1024 * 1024 * 1024);
  const h = crypto.createHash(algo);

  while (n-- > 0)
    h.write(message, encoding);

  h.end();
  h.read();

  bench.end(gbits);
}
