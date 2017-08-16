'use strict';

const common = require('../common');
const HTTPParser = process.binding('http_parser').HTTPParser;
const REQUEST = HTTPParser.REQUEST;
const kOnHeaders = HTTPParser.kOnHeaders | 0;
const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
const kOnBody = HTTPParser.kOnBody | 0;
const kOnMessageComplete = HTTPParser.kOnMessageComplete | 0;
const CRLF = '\r\n';

const bench = common.createBenchmark(main, {
  len: [4, 8, 16, 32],
  n: [1e5],
});


function main(conf) {
  const len = conf.len >>> 0;
  const n = conf.n >>> 0;
  var header = `GET /hello HTTP/1.1${CRLF}Content-Type: text/plain${CRLF}`;

  for (var i = 0; i < len; i++) {
    header += `X-Filler${i}: ${Math.random().toString(36).substr(2)}${CRLF}`;
  }
  header += CRLF;

  processHeader(Buffer.from(header), n);
}


function processHeader(header, n) {
  const parser = newParser(REQUEST);

  bench.start();
  for (var i = 0; i < n; i++) {
    parser.execute(header, 0, header.length);
    parser.reinitialize(REQUEST);
  }
  bench.end(n);
}


function newParser(type) {
  const parser = new HTTPParser(type);

  parser.headers = [];

  parser[kOnHeaders] = function() { };
  parser[kOnHeadersComplete] = function() { };
  parser[kOnBody] = function() { };
  parser[kOnMessageComplete] = function() { };

  return parser;
}
