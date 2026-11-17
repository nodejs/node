'use strict';

// Measures parsing a chunked request body as the chunk size shrinks.
// The wire buffer is fed to the parser in 64 KiB slices, like socket reads.

const common = require('../common');

const bench = common.createBenchmark(main, {
  chunkSize: [1, 16, 256, 4096],
  len: [64 * 1024],
  n: [500],
}, {
  flags: ['--expose-internals', '--no-warnings'],
});

function main({ chunkSize, len, n }) {
  const { HTTPParser } = common.binding('http_parser');
  const REQUEST = HTTPParser.REQUEST;
  const kOnHeaders = HTTPParser.kOnHeaders | 0;
  const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
  const kOnBody = HTTPParser.kOnBody | 0;
  const kOnMessageComplete = HTTPParser.kOnMessageComplete | 0;
  const kMaxHeaderPairs = 2000;
  const kReadSize = 64 * 1024;

  const chunk = `${chunkSize.toString(16)}\r\n${'x'.repeat(chunkSize)}\r\n`;
  const wire = Buffer.from(
    'POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n' +
    chunk.repeat(Math.ceil(len / chunkSize)) +
    '0\r\n\r\n');

  const reads = [];
  for (let i = 0; i < wire.length; i += kReadSize)
    reads.push(wire.subarray(i, i + kReadSize));

  let bodyBytes = 0;
  const parser = new HTTPParser();
  parser.initialize(REQUEST, {}, 0, 0, undefined, kMaxHeaderPairs);
  parser[kOnHeaders] = function() {};
  parser[kOnHeadersComplete] = function() {};
  parser[kOnBody] = function(b) { bodyBytes += b.length; };
  parser[kOnMessageComplete] = function() {};

  bench.start();
  for (let i = 0; i < n; i++) {
    for (const read of reads)
      parser.execute(read);
    parser.initialize(REQUEST, {}, 0, 0, undefined, kMaxHeaderPairs);
  }
  bench.end(n);

  const expected = Math.ceil(len / chunkSize) * chunkSize * n;
  if (bodyBytes !== expected)
    throw new Error(`Expected ${expected} body bytes, got ${bodyBytes}`);
}
