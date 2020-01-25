'use strict';

const { mustCall, hasCrypto, skip } = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const assert = require('assert');
const { createServer, connect } = require('http2');
const Countdown = require('../common/countdown');

// This test ensures that `bufferSize` of Http2Session and Http2Stream work
// as expected.
{
  const SOCKETS = 2;
  const TIMES = 10;
  const BUFFER_SIZE = 30;
  const server = createServer();

  let client;
  const countdown = new Countdown(SOCKETS, () => {
    client.close();
    server.close();
  });

  // Other `bufferSize` tests for net module and tls module
  // don't assert `bufferSize` of server-side sockets.
  server.on('stream', mustCall((stream) => {
    stream.on('data', mustCall());
    stream.on('end', mustCall());

    stream.on('close', mustCall(() => {
      countdown.dec();
    }));
  }, SOCKETS));

  server.listen(0, mustCall(() => {
    const authority = `http://localhost:${server.address().port}`;
    client = connect(authority);

    client.once('connect', mustCall());

    for (let j = 0; j < SOCKETS; j += 1) {
      const stream = client.request({ ':method': 'POST' });
      stream.on('data', () => {});

      for (let i = 0; i < TIMES; i += 1) {
        stream.write(Buffer.allocUnsafe(BUFFER_SIZE), mustCall());
        const expectedSocketBufferSize = BUFFER_SIZE * (i + 1);
        assert.strictEqual(stream.bufferSize, expectedSocketBufferSize);
      }
      stream.end();
      stream.close();
    }
  }));
}
