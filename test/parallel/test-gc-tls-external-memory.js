'use strict';
// Flags: --expose-gc

// Tests that memoryUsage().external doesn't go negative
// when a lot tls connections are opened and closed

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');

// Payload doesn't matter. We just need to have the tls
// connection try and connect somewhere.
const yolo = Buffer.alloc(10000).fill('yolo');
const server = net.createServer(function(socket) {
  socket.write(yolo);
});

server.listen(0, common.mustCall(function() {
  const { port } = server.address();
  let runs = 0;
  connect();

  function connect() {
    global.gc();
    assert(process.memoryUsage().external >= 0);
    if (runs++ < 512)
      tls.connect(port).on('error', connect);
    else
      server.close();
  }
}));
