'use strict';

const { mustCall, mustSucceed, hasCrypto, skip } = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const assert = require('assert');
const { createServer, connect } = require('http2');
const Countdown = require('../common/countdown');

// This test ensures that `bufferSize` of Http2Session and Http2Stream work
// as expected.
{
  const kSockets = 2;
  const kTimes = 10;
  const kBufferSize = 30;
  const server = createServer();

  let client;
  const countdown = new Countdown(kSockets, () => {
    client.close();
    server.close();
  });

  server.on('stream', mustCall((stream) => {
    stream.on('data', mustCall());
    stream.on('end', mustCall());
    stream.on('close', mustCall(() => {
      countdown.dec();
    }));
  }, kSockets));

  server.listen(0, mustCall(() => {
    const authority = `http://localhost:${server.address().port}`;
    client = connect(authority);

    client.once('connect', mustCall());

    for (let j = 0; j < kSockets; j += 1) {
      const stream = client.request({ ':method': 'POST' });
      stream.on('data', () => {});

      for (let i = 0; i < kTimes; i += 1) {
        stream.write(Buffer.allocUnsafe(kBufferSize), mustSucceed());
        const expectedSocketBufferSize = kBufferSize * (i + 1);
        assert.strictEqual(stream.bufferSize, expectedSocketBufferSize);
      }
      stream.end();
      stream.close();
    }
  }));
}
