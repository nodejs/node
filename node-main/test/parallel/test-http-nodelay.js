'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const originalConnect = net.Socket.prototype.connect;

net.Socket.prototype.connect = common.mustCall(function(args) {
  assert.strictEqual(args[0].noDelay, true);
  return originalConnect.call(this, args);
});

const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200);
  res.end();
  server.close();
}));

server.listen(0, common.mustCall(() => {
  assert.strictEqual(server.noDelay, true);

  const req = http.request({
    method: 'GET',
    port: server.address().port
  }, common.mustCall((res) => {
    res.on('end', () => {
      server.close();
      res.req.socket.end();
    });

    res.resume();
  }));

  req.end();
}));
