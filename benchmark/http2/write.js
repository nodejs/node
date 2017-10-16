'use strict';

const common = require('../common.js');
const PORT = common.PORT;

const bench = common.createBenchmark(main, {
  streams: [100, 200, 1000],
  length: [64 * 1024, 128 * 1024, 256 * 1024, 1024 * 1024],
  size: [100000],
  benchmarker: ['h2load']
}, { flags: ['--no-warnings', '--expose-http2'] });

function main(conf) {
  const m = +conf.streams;
  const l = +conf.length;
  const s = +conf.size;
  const http2 = require('http2');
  const server = http2.createServer();
  server.on('stream', (stream) => {
    stream.respond();
    let written = 0;
    function write() {
      stream.write('ü'.repeat(s));
      written += s;
      if (written < l)
        setImmediate(write);
      else
        stream.end();
    }
    write();
  });
  server.listen(PORT, () => {
    bench.http({
      path: '/',
      requests: 10000,
      maxConcurrentStreams: m,
    }, () => { server.close(); });
  });
}
