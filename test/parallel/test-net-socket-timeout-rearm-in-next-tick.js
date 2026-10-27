'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

// Re-arming a socket timeout from a nextTick queued by a timer that runs just
// before it in the same timer list must replace the old timeout.

const server = net.createServer().listen(0, common.mustCall(() => {
  const socket = net.connect(server.address().port, common.mustCall(() => {
    let rearmed = false;

    setTimeout(common.mustCall(() => {
      process.nextTick(() => {
        socket.setTimeout(50);
        setImmediate(() => { rearmed = true; });
      });
    }), 1);
    socket.setTimeout(1);

    socket.on('timeout', common.mustCall(() => {
      assert.ok(rearmed);
      socket.destroy();
      server.close();
    }));
  }));
}));
