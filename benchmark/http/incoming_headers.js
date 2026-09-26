'use strict';
const common = require('../common.js');
const http = require('http');

const bench = common.createBenchmark(main, {
  connections: [50], // Concurrent connections
  headers: [20], // Number of header lines to append after the common headers
  w: [0, 6], // Amount of trailing whitespace
  read: [0, 1], // Whether the handler reads req.headers
  duration: 5,
});

function main({ connections, headers, w, read, duration }) {
  const server = http.createServer((req, res) => {
    if (read && req.headers.host === undefined) {
      throw new Error('Missing Host header');
    }
    res.end();
  });

  server.listen(0, () => {
    const requestHeaders = {
      'Content-Type': 'text/plain',
      'Accept': 'text/plain',
      'User-Agent': 'nodejs-benchmark',
      'Date': new Date().toString(),
      'Cache-Control': 'no-cache',
    };
    for (let i = 0; i < headers; i++) {
      // Note:
      // - autocannon does not send header values with OWS
      // - wrk can only send trailing OWS. This is a side-effect of wrk
      // processing requests with http-parser before sending them, causing
      // leading OWS to be stripped.
      requestHeaders[`foo${i}`] = `some header value ${i}${' \t'.repeat(w / 2)}`;
    }
    bench.http({
      path: '/',
      connections,
      headers: requestHeaders,
      duration,
      port: server.address().port,
    }, () => {
      server.close();
    });
  });
}
