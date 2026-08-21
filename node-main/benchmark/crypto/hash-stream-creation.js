// Throughput benchmark
// creates a single hasher, then pushes a bunch of data through it
'use strict';
const common = require('../common.js');
const crypto = require('crypto');

const bench = common.createBenchmark(main, {
  writes: [500],
  algo: [ 'sha256', 'md5' ],
  type: ['asc', 'utf', 'buf'],
  out: ['hex', 'binary', 'buffer'],
  len: [2, 1024, 102400, 1024 * 1024],
  api: ['legacy', 'stream'],
});

function main({ api, type, len, out, writes, algo }) {
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
  fn(algo, message, encoding, writes, len, out);
}

function legacyWrite(algo, message, encoding, writes, len, outEnc) {
  const written = writes * len;
  const bits = written * 8;
  const gbits = bits / (1024 * 1024 * 1024);

  while (writes-- > 0) {
    const h = crypto.createHash(algo);
    h.update(message, encoding);
    h.digest(outEnc);
  }

  bench.end(gbits);
}

function streamWrite(algo, message, encoding, writes, len, outEnc) {
  const written = writes * len;
  const bits = written * 8;
  const gbits = bits / (1024 * 1024 * 1024);

  while (writes-- > 0) {
    const h = crypto.createHash(algo);

    if (outEnc !== 'buffer')
      h.setEncoding(outEnc);

    h.write(message, encoding);
    h.end();
    h.read();
  }

  bench.end(gbits);
}
