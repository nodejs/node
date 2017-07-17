'use strict';

const common = require('../common.js');
const PORT = common.PORT;
const path = require('path');
const fs = require('fs');

const file = path.join(path.resolve(__dirname, '../fixtures'), 'alice.html');

var bench = common.createBenchmark(main, {
  requests: [100, 1000, 10000, 100000, 1000000],
  streams: [100, 200, 1000],
  clients: [1, 2]
}, { flags: ['--expose-http2', '--no-warnings'] });

function main(conf) {

  fs.open(file, 'r', (err, fd) => {
    if (err)
      throw err;

    const n = +conf.requests;
    const m = +conf.streams;
    const c = +conf.clients;
    const http2 = require('http2');
    const server = http2.createServer();
    server.on('stream', (stream) => {
      stream.respondWithFD(fd);
      stream.on('error', (err) => {});
    });
    server.listen(PORT, () => {
      bench.http({
        path: '/',
        requests: n,
        maxConcurrentStreams: m,
        clients: c,
        threads: c
      }, () => server.close());
    });

  });

}
