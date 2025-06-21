'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  streams: [100, 200, 1000],
  length: [64 * 1024, 128 * 1024, 256 * 1024, 1024 * 1024],
  size: [100000],
  benchmarker: ['test-double-http2'],
  duration: 5,
}, { flags: ['--no-warnings'] });

function main({ streams, length, size, duration }) {
  const http2 = require('http2');
  const server = http2.createServer();
  server.on('stream', (stream) => {
    stream.respond();
    let written = 0;
    function write() {
      stream.write('ü'.repeat(size));
      written += size;
      if (written < length)
        setImmediate(write);
      else
        stream.end();
    }
    write();
  });
  server.listen(0, () => {
    bench.http({
      path: '/',
      port: server.address().port,
      requests: 10000,
      duration,
      maxConcurrentStreams: streams,
    }, () => { server.close(); });
  });
}
