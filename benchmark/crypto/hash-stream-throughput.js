// throughput benchmark
// creates a single hasher, then pushes a bunch of data through it
'use strict';
var common = require('../common.js');
var crypto = require('crypto');

var bench = common.createBenchmark(main, {
  writes: [500],
  algo: ['sha1', 'sha256', 'sha512'],
  type: ['asc', 'utf', 'buf'],
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

  var fn = api === 'stream' ? streamWrite : legacyWrite;

  bench.start();
  fn(conf.algo, message, encoding, conf.writes, conf.len);
}

function legacyWrite(algo, message, encoding, writes, len) {
  var written = writes * len;
  var bits = written * 8;
  var gbits = bits / (1024 * 1024 * 1024);
  var h = crypto.createHash(algo);

  while (writes-- > 0)
    h.update(message, encoding);

  h.digest();

  bench.end(gbits);
}

function streamWrite(algo, message, encoding, writes, len) {
  var written = writes * len;
  var bits = written * 8;
  var gbits = bits / (1024 * 1024 * 1024);
  var h = crypto.createHash(algo);

  while (writes-- > 0)
    h.write(message, encoding);

  h.end();
  h.read();

  bench.end(gbits);
}
