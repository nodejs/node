'use strict';

const common = require('../common.js');
const http = require('http');

const bench = common.createBenchmark(main, {
  server: ['createServer', 'serve', 'serve-fast'],
  type: ['string', 'buffer'],
  len: [4, 1024, 102400],
  c: [50, 500],
  duration: 5,
});

function main({ server: serverType, type, len, c, duration }) {
  const body = type === 'string' ? 'C'.repeat(len) : Buffer.alloc(len, 67);
  const headers = {
    'Content-Length': `${len}`,
    'Content-Type': 'application/octet-stream',
  };

  let server;
  if (serverType === 'serve') {
    server = http.serve(() => new Response(body, { headers }));
  } else if (serverType === 'serve-fast') {
    server = http.serve(() => new http.NodeResponse(body, { headers }));
  } else {
    server = http.createServer((request, response) => {
      response.writeHead(200, headers);
      response.end(body);
    });
  }

  server.listen(0, () => {
    bench.http({
      connections: c,
      duration,
      port: server.address().port,
    }, () => server.close());
  });
}
