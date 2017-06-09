'use strict';

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const http = require('http');
const net = require('net');

if (cluster.isMaster) {
  const worker = cluster.fork();
  const server = net.createServer({
    pauseOnConnect: true
  }, common.mustCall((socket) => {
    worker.send('socket', socket);
  }));

  worker.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    server.close();
  }));

  server.listen(0, common.mustCall(() => {
    net.createConnection(server.address().port);
  }));
} else {
  const server = http.createServer();

  server.setTimeout(100, common.mustCall((socket) => {
    socket.destroy();
    cluster.worker.kill();
  }));

  process.on('message', common.mustCall((message, socket) => {
    server.emit('connection', socket);
    socket.resume();
  }));
}
