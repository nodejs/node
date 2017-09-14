// throughput benchmark
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
  api: ['legacy', 'stream']
});

function main(conf) {
  var api = conf.api;
  if (api === 'stream' && /^v0\.[0-8]\./.test(process.version)) {
    console.error('Crypto streams not available until v0.10');
    // use the legacy, just so that we can compare them.
    api = 'legacy';
  }

  var message;
  var encoding;
  switch (conf.type) {
    case 'asc':
      message = 'a'.repeat(conf.len);
      encoding = 'ascii';
      break;
    case 'utf':
      message = 'ü'.repeat(conf.len / 2);
      encoding = 'utf8';
      break;
    case 'buf':
      message = Buffer.alloc(conf.len, 'b');
      break;
    default:
      throw new Error(`unknown message type: ${conf.type}`);
  }

  const fn = api === 'stream' ? streamWrite : legacyWrite;

  bench.start();
  fn(conf.algo, message, encoding, conf.writes, conf.len, conf.out);
}

function legacyWrite(algo, message, encoding, writes, len, outEnc) {
  const written = writes * len;
  const bits = written * 8;
  const gbits = bits / (1024 * 1024 * 1024);

  while (writes-- > 0) {
    const h = crypto.createHash(algo);
    h.update(message, encoding);
    var res = h.digest(outEnc);

    // include buffer creation costs for older versions
    if (outEnc === 'buffer' && typeof res === 'string')
      res = Buffer.from(res, 'binary');
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
