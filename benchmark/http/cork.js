'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['string', 'buffer'],
  chunks: [4, 16],
  len: [64],
  c: [50],
  duration: [5]
});

function main({ type, chunks, len, c, duration }) {
  const http = require('http');
  const chunk = type === 'string' ? 'a'.repeat(len) : Buffer.alloc(len, 'a');

  const server = http.createServer((req, res) => {
    for (let n = 0; n < chunks; n++) {
      res.write(chunk);
    }
    res.end();
  });

  server.listen(0, () => {
    bench.http({
      connections: c,
      duration,
      port: server.address().port
    }, () => server.close());
  });
}
