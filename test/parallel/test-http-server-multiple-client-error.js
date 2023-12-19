'use strict';
const common = require('../common');

// Test that the `'clientError'` event can be emitted multiple times even if the
// socket is correctly destroyed and that no warning is raised.

const assert = require('assert');
const http = require('http');
const net = require('net');

process.on('warning', common.mustNotCall());

function socketListener(socket) {
  const firstByte = socket.read(1);
  if (firstByte === null) {
    socket.once('readable', () => {
      socketListener(socket);
    });
    return;
  }

  socket.unshift(firstByte);
  httpServer.emit('connection', socket);
}

const netServer = net.createServer(socketListener);
const httpServer = http.createServer(common.mustNotCall());

httpServer.once('clientError', common.mustCall((err, socket) => {
  assert.strictEqual(err.code, 'HPE_INVALID_METHOD');
  assert.strictEqual(err.rawPacket.toString(), 'Q');
  socket.destroy();

  httpServer.once('clientError', common.mustCall((err) => {
    assert.strictEqual(err.code, 'HPE_INVALID_METHOD');
    assert.strictEqual(
      err.rawPacket.toString(),
      'WE http://example.com HTTP/1.1\r\n\r\n'
    );
  }));
}));

netServer.listen(0, common.mustCall(() => {
  const socket = net.createConnection(netServer.address().port);

  socket.on('connect', common.mustCall(() => {
    socket.end('QWE http://example.com HTTP/1.1\r\n\r\n');
  }));

  socket.on('close', () => {
    netServer.close();
  });

  socket.resume();
}));
