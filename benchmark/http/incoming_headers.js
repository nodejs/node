'use strict';
const common = require('../common.js');
const http = require('http');

const bench = common.createBenchmark(main, {
  // unicode confuses ab on os x.
  c: [50, 500],
  headerDuplicates: [0, 5, 20]
});

function main({ c, headerDuplicates }) {
  const server = http.createServer((req, res) => {
    res.end();
  });

  server.listen(common.PORT, () => {
    const headers = {
      'Content-Type': 'text/plain',
      'Accept': 'text/plain',
      'User-Agent': 'nodejs-benchmark',
      'Date': new Date().toString(),
      'Cache-Control': 'no-cache'
    };
    for (let i = 0; i < headerDuplicates; i++) {
      headers[`foo${i}`] = `some header value ${i}`;
    }
    bench.http({
      path: '/',
      connections: c,
      headers
    }, () => {
      server.close();
    });
  });
}
