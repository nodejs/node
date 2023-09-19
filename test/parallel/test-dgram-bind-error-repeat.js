'use strict';
const common = require('../common');
const dgram = require('dgram');

// Regression test for https://github.com/nodejs/node/issues/30209
// No warning should be emitted when re-trying `.bind()` on UDP sockets
// repeatedly.

process.on('warning', common.mustNotCall());

const reservePortSocket = dgram.createSocket('udp4');
reservePortSocket.bind(() => {
  const { port } = reservePortSocket.address();

  const newSocket = dgram.createSocket('udp4');

  let errors = 0;
  newSocket.on('error', common.mustCall(() => {
    if (++errors < 20) {
      newSocket.bind(port, common.mustNotCall());
    } else {
      newSocket.close();
      reservePortSocket.close();
    }
  }, 20));
  newSocket.bind(port, common.mustNotCall());
});
