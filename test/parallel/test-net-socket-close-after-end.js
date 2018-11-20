'use strict';
const common = require('../common');

const assert = require('assert');
const net = require('net');

const server = net.createServer();

server.on('connection', (socket) => {
  let endEmitted = false;

  socket.once('readable', () => {
    setTimeout(() => {
      socket.read();
    }, common.platformTimeout(100));
  });
  socket.on('end', () => {
    endEmitted = true;
  });
  socket.on('close', () => {
    assert(endEmitted);
    server.close();
  });
  socket.end('foo');
});

server.listen(common.mustCall(() => {
  const socket = net.createConnection(server.address().port, () => {
    socket.end('foo');
  });
}));
