'use strict';

const common = require('../common.js');
const http = require('http');

const bench = common.createBenchmark(main, {
  duplicates: [1, 100],
  n: [10, 1000],
});

function main({ duplicates, n }) {
  const headers = {
    'Connection': 'keep-alive',
    'Transfer-Encoding': 'chunked',
  };

  for (var i = 0; i < n / duplicates; i++) {
    headers[`foo${i}`] = [];
    for (var j = 0; j < duplicates; j++) {
      headers[`foo${i}`].push(`some header value ${i}`);
    }
  }

  const server = http.createServer((req, res) => {
    res.writeHead(200, headers);
    res.end();
  });
  server.listen(common.PORT, () => {
    bench.http({
      path: '/',
      connections: 10
    }, () => {
      server.close();
    });
  });
}
