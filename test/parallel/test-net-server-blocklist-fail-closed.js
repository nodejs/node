'use strict';
const common = require('../common');
const net = require('net');

const blockList = new net.BlockList();
blockList.addCIDR('0.0.0.0/0');
blockList.addCIDR('::/0');

// A connection whose peer address cannot be determined (e.g. it was reset
// before getpeername()) must be rejected when a BlockList is configured,
// rather than being delivered to the application (fail closed).
const server = net.createServer({ blockList }, common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const socket = net.connect(server.address().port);
  socket.on('error', () => {});
}));

const onconnection = server._handle.onconnection;
server._handle.onconnection = common.mustCall((err, clientHandle) => {
  const close = clientHandle.close;
  // Simulate the reset-before-getpeername() condition: onconnection() is
  // unable to obtain a valid peer address from the accepted connection.
  clientHandle.getpeername = function(remoteInfo) {
    remoteInfo.address = undefined;
  };
  clientHandle.close = common.mustCall(() => {
    clientHandle.close = close;
    close.call(clientHandle);
    server.close();
  });
  onconnection.call(server._handle, err, clientHandle);
});
