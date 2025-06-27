'use strict';

const common = require('../common.js');
const path = require('path');
const fs = require('fs');
const file = path.join(path.resolve(__dirname, '../fixtures'), 'alice.html');

const bench = common.createBenchmark(main, {
  requests: [100, 1000, 5000],
  streams: [1, 10, 20, 40, 100, 200],
  clients: [2],
  benchmarker: ['test-double-http2'],
  duration: 5,
}, { flags: ['--no-warnings'] });

function main({ requests, streams, clients, duration }) {
  const http2 = require('http2');
  const server = http2.createServer();
  server.on('request', (req, res) => {
    const out = fs.createReadStream(file);
    res.setHeader('content-type', 'text/html');
    out.pipe(res);
    out.on('error', (err) => {
      res.destroy();
    });
  });
  server.listen(0, () => {
    bench.http({
      path: '/',
      port: server.address().port,
      requests,
      maxConcurrentStreams: streams,
      clients,
      threads: clients,
      duration,
    }, () => { server.close(); });
  });
}
