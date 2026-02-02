'use strict';
const { mustCall } = require('../common');

const http = require('http');
const { strictEqual } = require('assert');

const server = http.createServer(mustCall((req, res) => {
  res.flushHeaders();
}));

server.listen(0, mustCall(() => {
  const req = http.get({
    port: server.address().port
  }, mustCall(() => {
    const { socket } = req;
    socket.emit('agentRemove');
    strictEqual(socket._httpMessage, req);
    socket.destroy();
    server.close();
  }));
}));
