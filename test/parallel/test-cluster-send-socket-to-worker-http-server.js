'use strict';

// Regression test for https://github.com/nodejs/node/issues/13435
// Tests that `socket.server` is correctly set when a socket is sent to a worker
// and the `'connection'` event is emitted manually on an HTTP server.

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const http = require('http');
const net = require('net');

if (cluster.isMaster) {
  const worker = cluster.fork();
  const server = net.createServer(common.mustCall((socket) => {
    worker.send('socket', socket);
  }));

  worker.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    server.close();
  }));

  server.listen(0, common.mustCall(() => {
    net.createConnection(server.address().port);
  }));
} else {
  const server = http.createServer();

  server.on('connection', common.mustCall((socket) => {
    assert.strictEqual(socket.server, server);
    socket.destroy();
    cluster.worker.disconnect();
  }));

  process.on('message', common.mustCall((message, socket) => {
    server.emit('connection', socket);
  }));
}
