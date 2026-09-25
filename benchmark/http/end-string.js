// Responses sent as a single res.end(string) with a known Content-Length -
// the shape a JSON or HTML endpoint produces.
'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  len: [4, 64, 1024, 16384, 102400],
  c: [50],
  duration: 5,
});

function main({ len, c, duration }) {
  const http = require('http');
  const body = 'a'.repeat(len);
  const headers = {
    'Content-Type': 'text/plain',
    'Content-Length': `${len}`,
  };

  const server = http.createServer((req, res) => {
    res.writeHead(200, headers);
    res.end(body);
  });

  server.listen(0, () => {
    bench.http({
      connections: c,
      duration,
      port: server.address().port,
    }, () => {
      server.close();
    });
  });
}
