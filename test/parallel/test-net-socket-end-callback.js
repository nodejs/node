'use strict';

const common = require('../common');
const net = require('net');

const server = net.createServer((socket) => {
  socket.resume();
}).unref();

server.listen(common.mustCall(() => {
  const connect = (...args) => {
    const socket = net.createConnection(server.address().port, () => {
      socket.end(...args);
    });
  };

  const cb = common.mustCall(() => {}, 3);

  connect(cb);
  connect('foo', cb);
  connect('foo', 'utf8', cb);
}));
