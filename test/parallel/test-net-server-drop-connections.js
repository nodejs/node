'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

let firstSocket;
const dormantServer = net.createServer(common.mustNotCall());
const server = net.createServer(common.mustCall((socket) => {
  firstSocket = socket;
}));

dormantServer.maxConnections = 0;
server.maxConnections = 1;

dormantServer.on('drop', common.mustCall((data) => {
  assert.strictEqual(!!data.localAddress, true);
  assert.strictEqual(!!data.localPort, true);
  assert.strictEqual(!!data.remoteAddress, true);
  assert.strictEqual(!!data.remotePort, true);
  assert.strictEqual(!!data.remoteFamily, true);
  dormantServer.close();
}));

server.on('drop', common.mustCall((data) => {
  assert.strictEqual(!!data.localAddress, true);
  assert.strictEqual(!!data.localPort, true);
  assert.strictEqual(!!data.remoteAddress, true);
  assert.strictEqual(!!data.remotePort, true);
  assert.strictEqual(!!data.remoteFamily, true);
  firstSocket.destroy();
  server.close();
}));

dormantServer.listen(0, () => {
  net.createConnection(dormantServer.address().port);
});

server.listen(0, () => {
  net.createConnection(server.address().port);
  net.createConnection(server.address().port);
});
