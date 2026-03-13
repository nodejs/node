'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

const big = Buffer.alloc(1024 * 1024);

const handler = common.mustCall((socket) => {
  socket.end(big);
  server.close();
});

const onListen = common.mustCall(() => {
  let prev = 0;

  function checkRaise(value) {
    assert(value > prev);
    prev = value;
  }

  const onData = common.mustCallAtLeast((chunk) => {
    checkRaise(socket.bytesRead);
  });

  const onEnd = common.mustCall(() => {
    assert.strictEqual(socket.bytesRead, prev);
    assert.strictEqual(big.length, prev);
  });

  const onClose = common.mustCall(() => {
    assert(!socket._handle);
    assert.strictEqual(socket.bytesRead, prev);
    assert.strictEqual(big.length, prev);
  });

  const onConnect = common.mustCall(() => {
    socket.on('data', onData);
    socket.on('end', onEnd);
    socket.on('close', onClose);
    socket.end();
  });

  const socket = net.connect(server.address().port, onConnect);
});

const server = net.createServer(handler).listen(0, onListen);
