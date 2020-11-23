'use strict';
const common = require('../common');

const http = require('http');
const net = require('net');

const data = 'hello';
const server = http.createServer({}, common.mustCallAtLeast((req, res) => {
  res.setHeader('content-length', data.length);
  res.end(data);
}, 2));

server.listen(0, function() {
  const options = {
    host: '127.0.0.1',
    port: server.address().port,
    allowHalfOpen: true
  };
  const socket = net.connect(options, common.mustCall(() => {
    socket.on('ready', common.mustCall(() => {
      socket.write('GET /\n\n');
      setTimeout(() => {
        socket.write('GET /\n\n');
        setTimeout(() => {
          socket.destroy();
          server.close();
        }, common.platformTimeout(10));
      }, common.platformTimeout(10));
    }));
  }));
});
