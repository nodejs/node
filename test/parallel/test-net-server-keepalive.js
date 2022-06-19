'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer({
  keepAlive: true,
  keepAliveInitialDelay: 1000
}, common.mustCall((socket) => {
  socket.destroy();
  server.close();
})).listen(0, common.mustCall(() => {
  net.connect(server.address().port);
}));

const onconnection = server._handle.onconnection;
server._handle.onconnection = common.mustCall((err, clientHandle) => {
  const setKeepAlive = clientHandle.setKeepAlive;
  clientHandle.setKeepAlive = common.mustCall((enable, initialDelayMsecs) => {
    assert.strictEqual(enable, server.keepAlive);
    assert.strictEqual(initialDelayMsecs, server.keepAliveInitialDelay);
    setKeepAlive.call(clientHandle, enable, initialDelayMsecs);
  });
  onconnection.call(server._handle, err, clientHandle);
});
