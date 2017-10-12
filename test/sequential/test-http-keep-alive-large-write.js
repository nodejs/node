'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// This test assesses whether long-running writes can complete
// or timeout because the socket is not aware that the backing
// stream is still writing.
// To simulate a slow client, we write a really large chunk and
// then proceed through the following cycle:
// 1) Receive first 'data' event and record currently written size
// 2) Once we've read up to currently written size recorded above,
//    we pause the stream and wait longer than the server timeout
// 3) Socket.prototype._onTimeout triggers and should confirm
//    that the backing stream is still active and writing
// 4) Our timer fires, we resume the socket and start at 1)

const minReadSize = 250000;
const serverTimeout = common.platformTimeout(500);
let offsetTimeout = common.platformTimeout(100);
let serverConnectionHandle;
let writeSize = 3000000;
let didReceiveData = false;
// this represents each cycles write size, where the cycle consists
// of `write > read > _onTimeout`
let currentWriteSize = 0;

const server = http.createServer(common.mustCall((req, res) => {
  const content = Buffer.alloc(writeSize, 0x44);

  res.writeHead(200, {
    'Content-Type': 'application/octet-stream',
    'Content-Length': content.length.toString(),
    'Vary': 'Accept-Encoding'
  });

  serverConnectionHandle = res.socket._handle;
  res.write(content);
  res.end();
}));
server.setTimeout(serverTimeout);
server.on('timeout', () => {
  assert.strictEqual(didReceiveData, false, 'Should not timeout');
});

server.listen(0, common.mustCall(() => {
  http.get({
    path: '/',
    port: server.address().port
  }, common.mustCall((res) => {
    const resume = () => res.resume();
    let receivedBufferLength = 0;
    let firstReceivedAt;
    res.on('data', common.mustCallAtLeast((buf) => {
      if (receivedBufferLength === 0) {
        currentWriteSize = Math.max(
          minReadSize,
          writeSize - serverConnectionHandle.writeQueueSize
        );
        didReceiveData = false;
        firstReceivedAt = Date.now();
      }
      receivedBufferLength += buf.length;
      if (receivedBufferLength >= currentWriteSize) {
        didReceiveData = true;
        writeSize = serverConnectionHandle.writeQueueSize;
        receivedBufferLength = 0;
        res.pause();
        setTimeout(
          resume,
          serverTimeout + offsetTimeout - (Date.now() - firstReceivedAt)
        );
        offsetTimeout = 0;
      }
    }, 1));
    res.on('end', common.mustCall(() => {
      server.close();
    }));
  }));
}));
