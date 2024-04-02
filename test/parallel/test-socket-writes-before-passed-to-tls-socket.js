'use strict';
const common = require('../common');
const assert = require('assert');
if (!common.hasCrypto) common.skip('missing crypto');
const tls = require('tls');
const net = require('net');

const HEAD = Buffer.alloc(1024 * 1024, 0);

const server = net.createServer((serverSock) => {
  let recvLen = 0;
  const recv = [];
  serverSock.on('data', common.mustCallAtLeast((chunk) => {
    recv.push(chunk);
    recvLen += chunk.length;

    // Check that HEAD is followed by a client hello
    if (recvLen > HEAD.length) {
      const clientHelloFstByte = Buffer.concat(recv).subarray(HEAD.length, HEAD.length + 1);
      assert.strictEqual(clientHelloFstByte.toString('hex'), '16');
      process.exit(0);
    }
  }, 1));
})
  .listen(client);

function client() {
  const socket = net.createConnection({
    host: '127.0.0.1',
    port: server.address().port,
  });
  socket.write(HEAD.subarray(0, HEAD.length / 2), common.mustSucceed());

  // This write will be queued by streams.Writable, the super class of net.Socket,
  // which will dequeue this write when it gets notified about the finish of the first write.
  // We had a bug that it wouldn't get notified. This test verifies the bug is fixed.
  socket.write(HEAD.subarray(HEAD.length / 2), common.mustSucceed());

  tls.connect({
    socket,
  });
}
