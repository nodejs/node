'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const big = Buffer.alloc(1024 * 1024);

const server = net.createServer((socket) => {
  socket.end(big);
  server.close();
}).listen(0, () => {
  let prev = 0;

  function checkRaise(value) {
    assert(value > prev);
    prev = value;
  }

  const socket = net.connect(server.address().port, () => {
    socket.on('data', (chunk) => {
      checkRaise(socket.bytesRead);
    });

    socket.on('end', common.mustCall(() => {
      assert.equal(socket.bytesRead, prev);
      assert.equal(big.length, prev);
    }));

    socket.on('close', common.mustCall(() => {
      assert(!socket._handle);
      assert.equal(socket.bytesRead, prev);
      assert.equal(big.length, prev);
    }));
  });
  socket.end();
});
