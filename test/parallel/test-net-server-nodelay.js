'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer({
  noDelay: true
}, common.mustCall((socket) => {
  socket._handle.setNoDelay = common.mustNotCall();
  socket.setNoDelay(true);
  socket.destroy();
  server.close();
})).listen(0, common.mustCall(() => {
  net.connect(server.address().port);
}));

const onconnection = server._handle.onconnection;
server._handle.onconnection = common.mustCall((err, clientHandle) => {
  const setNoDelay = clientHandle.setNoDelay;
  clientHandle.setNoDelay = common.mustCall((enable) => {
    assert.strictEqual(enable, server.noDelay);
    setNoDelay.call(clientHandle, enable);
    clientHandle.setNoDelay = setNoDelay;
  });
  onconnection.call(server._handle, err, clientHandle);
});
