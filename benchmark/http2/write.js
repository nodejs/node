'use strict';

const common = require('../common.js');
const PORT = common.PORT;

var bench = common.createBenchmark(main, {
  streams: [100, 200, 1000],
  length: [64 * 1024, 128 * 1024, 256 * 1024, 1024 * 1024],
}, { flags: ['--expose-http2', '--no-warnings'] });

function main(conf) {
  const m = +conf.streams;
  const l = +conf.length;
  const http2 = require('http2');
  const server = http2.createServer();
  server.on('stream', (stream) => {
    stream.respond();
    stream.write('ü'.repeat(l));
    stream.end();
  });
  server.listen(PORT, () => {
    bench.http({
      path: '/',
      requests: 10000,
      maxConcurrentStreams: m,
    }, () => { server.close(); });
  });
}
