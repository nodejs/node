'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  len: [8, 16],
  frags: [2, 4, 8],
  n: [1e5],
}, {
  flags: ['--expose-internals', '--no-warnings'],
});

function main({ len, frags, n }) {
  const { HTTPParser } = common.binding('http_parser');
  const REQUEST = HTTPParser.REQUEST;
  const kOnHeaders = HTTPParser.kOnHeaders | 0;
  const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
  const kOnBody = HTTPParser.kOnBody | 0;
  const kOnMessageComplete = HTTPParser.kOnMessageComplete | 0;

  function processHeaderFragmented(fragments, n) {
    const parser = newParser(REQUEST);

    bench.start();
    for (let i = 0; i < n; i++) {
      // Send header in fragments
      for (const frag of fragments) {
        parser.execute(frag, 0, frag.length);
      }
      parser.initialize(REQUEST, {});
    }
    bench.end(n);
  }

  function newParser(type) {
    const parser = new HTTPParser();
    parser.initialize(type, {});

    parser.headers = [];

    parser[kOnHeaders] = function() { };
    parser[kOnHeadersComplete] = function() { };
    parser[kOnBody] = function() { };
    parser[kOnMessageComplete] = function() { };

    return parser;
  }

  // Build the header
  let header = `GET /hello HTTP/1.1\r\nContent-Type: text/plain\r\n`;

  for (let i = 0; i < len; i++) {
    header += `X-Filler${i}: ${Math.random().toString(36).substring(2)}\r\n`;
  }
  header += '\r\n';

  // Split header into fragments
  const headerBuf = Buffer.from(header);
  const fragSize = Math.ceil(headerBuf.length / frags);
  const fragments = [];

  for (let i = 0; i < headerBuf.length; i += fragSize) {
    fragments.push(headerBuf.slice(i, Math.min(i + fragSize, headerBuf.length)));
  }

  processHeaderFragmented(fragments, n);
}
