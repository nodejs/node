'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  type: ['bytes', 'buffer'],
  len: [64, 1024],
  chunks: [4, 16],
  c: [50],
  duration: 5,
});

function main({ type, len, chunks, c, duration }) {
  const http = require('http');
  const chunk = type === 'bytes' ? 'a'.repeat(len) : Buffer.alloc(len, 'a');

  const server = http.createServer((req, res) => {
    res.cork();
    for (let i = 0; i < chunks; i++) {
      res.write(chunk);
    }
    res.uncork();
    res.end();
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
