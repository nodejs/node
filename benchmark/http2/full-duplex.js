'use strict';

const common = require('../common.js');
const fixtures = require('../../test/common/fixtures');

const bench = common.createBenchmark(main, {
  n: [100],
  streams: [2],
  size: [4 * 1024 * 1024],
  // Use the HTTP/2 protocol default.
  window: [65535],
}, {
  test: { size: 128 * 1024, window: 65535 },
});

function main({ n, streams, size, window }) {
  const http2 = require('http2');
  const payload = Buffer.alloc(size);
  const server = http2.createSecureServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
    settings: { initialWindowSize: window },
  });

  let completed = 0;
  let batches = 0;

  function onTransferComplete() {
    if (++completed !== streams * 2)
      return;

    if (++batches === n) {
      // Report combined upload and download throughput in MiB/s.
      bench.end(n * streams * size * 2 / (1024 * 1024));
      client.close();
      server.close();
      return;
    }

    startBatch();
  }

  server.on('stream', (stream) => {
    stream.resume();
    stream.on('end', onTransferComplete);
    stream.respond();
    stream.end(payload);
  });

  let client;
  function startBatch() {
    completed = 0;
    for (let i = 0; i < streams; i++) {
      const request = client.request({ ':method': 'POST' });
      request.resume();
      request.on('end', onTransferComplete);
      request.end(payload);
    }
  }

  server.listen(0, () => {
    client = http2.connect(`https://localhost:${server.address().port}`, {
      rejectUnauthorized: false,
      settings: { initialWindowSize: window },
    });
    client.on('connect', () => {
      bench.start();
      startBatch();
    });
  });
}
