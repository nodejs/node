'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

const payload = 'a'.repeat(800004);

if (cluster.isMaster) {
  const server = net.createServer();

  server.on('connection', common.mustCall((socket) => socket.unref()));

  const worker = cluster.fork();
  worker.on('message', common.mustCall(({ payload: received }, handle) => {
    assert.strictEqual(payload, received);
    assert(handle instanceof net.Socket);
    server.close();
    handle.destroy();
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const socket = new net.Socket();
    socket.connect(port, (err) => {
      assert.ifError(err);
      worker.send({ payload }, socket);
    });
  }));
} else {
  process.on('message', common.mustCall(({ payload: received }, handle) => {
    assert.strictEqual(payload, received);
    assert(handle instanceof net.Socket);
    process.send({ payload }, handle);

    // Prepare for a clean exit.
    process.channel.unref();
    handle.unref();
  }));
}
