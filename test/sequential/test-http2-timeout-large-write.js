'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const http2 = require('http2');

// This test assesses whether long-running writes can complete
// or timeout because the session or stream are not aware that the
// backing stream is still writing.
// To simulate a slow client, we write a really large chunk and
// then proceed through the following cycle:
// 1) Receive first 'data' event and record currently written size
// 2) Once we've read up to currently written size recorded above,
//    we pause the stream and wait longer than the server timeout
// 3) Socket.prototype._onTimeout triggers and should confirm
//    that the backing stream is still active and writing
// 4) Our timer fires, we resume the socket and start at 1)

const writeSize = 3000000;
const minReadSize = 500000;
const serverTimeout = common.platformTimeout(500);
let offsetTimeout = common.platformTimeout(100);
let didReceiveData = false;

const server = http2.createSecureServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
});
server.on('stream', common.mustCall((stream) => {
  const content = Buffer.alloc(writeSize, 0x44);

  stream.respond({
    'Content-Type': 'application/octet-stream',
    'Content-Length': content.length.toString(),
    'Vary': 'Accept-Encoding'
  });

  stream.write(content);
  stream.setTimeout(serverTimeout);
  stream.on('timeout', () => {
    assert.strictEqual(didReceiveData, false, 'Should not timeout');
  });
  stream.end();
}));
server.setTimeout(serverTimeout);
server.on('timeout', () => {
  assert.strictEqual(didReceiveData, false, 'Should not timeout');
});

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`https://localhost:${server.address().port}`,
                               { rejectUnauthorized: false });

  const req = client.request({ ':path': '/' });
  req.end();

  const resume = () => req.resume();
  let receivedBufferLength = 0;
  let firstReceivedAt;
  req.on('data', common.mustCallAtLeast((buf) => {
    if (receivedBufferLength === 0) {
      didReceiveData = false;
      firstReceivedAt = Date.now();
    }
    receivedBufferLength += buf.length;
    if (receivedBufferLength >= minReadSize &&
        receivedBufferLength < writeSize) {
      didReceiveData = true;
      receivedBufferLength = 0;
      req.pause();
      setTimeout(
        resume,
        serverTimeout + offsetTimeout - (Date.now() - firstReceivedAt)
      );
      offsetTimeout = 0;
    }
  }, 1));
  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
}));
