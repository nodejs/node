'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer();

server.on('upgrade', common.mustCall((request, socket) => {
  assert.strictEqual(socket.parser, null);
  socket.write([
    'HTTP/1.1 101 Switching Protocols',
    'Connection: Upgrade',
    'Upgrade: WebSocket',
    '\r\n',
  ].join('\r\n'));
}));

server.listen(common.mustCall(() => {
  const request = http.get({
    port: server.address().port,
    headers: {
      Connection: 'Upgrade',
      Upgrade: 'WebSocket'
    }
  });

  request.on('upgrade', common.mustCall((response, socket) => {
    assert.strictEqual(socket.parser, null);
    socket.destroy();
    server.close();
  }));
}));
