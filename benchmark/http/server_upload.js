'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  stats: [0],
  duration: [10],
  connections: [200],
  bufSize: [64 * 1024],
  upload: [1 * 1024 * 1024],
  backpressure: [
    0, // None - just listen on request data events
    1, // Pipe to a writable stream
  ],
  delay: [
    -1, // No delay
    0, // -> setImmediate
    1, // -> setTimeout
    5, // ...
  ],
  useBufferPool: [0, 1],
});

function main({
  stats,
  duration,
  connections,
  bufSize,
  upload,
  backpressure,
  delay,
  useBufferPool,
}) {
  const http = require('http');
  const { pipeline, Writable } = require('stream');
  const { HTTPParser } = require('_http_common');
  const { BufferPool } = require('./_buffer_pool.js');

  const maxPoolSize = connections * upload * 2;
  const bufferPool = new BufferPool(maxPoolSize, bufSize);

  function onRequest(req, res) {
    const onData = (buf, enc, callback) => {
      // Processing the data ... and when done, recycle the buffer.
      if (useBufferPool) bufferPool.free(buf);
      if (callback) callback();
    };
    const onDataDelayed = delay < 0 ? onData : (
      delay === 0 ?
        setImmediate.bind(null, onData) :
        setTimeout.bind(null, onData, delay)
    );
    if (useBufferPool) {
      req.socket.parser[HTTPParser.kOnStreamAlloc | 0] =
        (length) => bufferPool.alloc(length);
    }
    if (backpressure) {
      const pipeTo = new Writable({
        highWaterMark: upload,
        write: onDataDelayed,
      });
      pipeline(req, pipeTo, () => res.end());
    } else {
      req.on('data', onDataDelayed);
      req.on('end', () => res.end());
    }
  }

  const headers = {
    'Content-Length': upload,
    'Content-Type': 'application/octet-stream',
  };

  const options = {
    highWaterMark: upload,
  };

  const server = http.createServer(options, onRequest);

  server.listen(0, () => {
    bench.http({
      connections,
      duration,
      headers,
      upload,
      port: server.address().port,
    }, () => {
      if (stats && useBufferPool) {
        console.log('BufferPool: length', bufferPool.pool.length, 'stats', bufferPool.stats);
      }
      server.close();
    });
  });
}
