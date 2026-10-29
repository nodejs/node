'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer({
  keepAlive: true,
  keepAliveInitialDelay: 1000
}, common.mustCall((socket) => {
  const setKeepAlive = socket._handle.setKeepAlive;
  socket._handle.setKeepAlive = common.mustCall((enable, initialDelay) => {
    assert.strictEqual(enable, true);
    assert.match(String(initialDelay), /^2|3$/);
    return setKeepAlive.call(socket._handle, enable, initialDelay);
  }, 2);
  socket.setKeepAlive(true, 1000);
  socket.setKeepAlive(true, 2000);
  socket.setKeepAlive(true, 3000);
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
    clientHandle.setKeepAlive = setKeepAlive;
  });
  onconnection.call(server._handle, err, clientHandle);
});
