'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const readableHighWaterMark = 1024;
const server = http.createServer((req, res) => { res.end(); });

server.listen(0, common.mustCall(() => {
  const req = http.request({
    port: server.address().port,
    createConnection(options) {
      options.readableHighWaterMark = readableHighWaterMark;
      return net.createConnection(options);
    }
  }, common.mustCall((res) => {
    assert.strictEqual(res.socket, req.socket);
    assert.strictEqual(res.socket.readableHighWaterMark, readableHighWaterMark);
    assert.strictEqual(res.readableHighWaterMark, readableHighWaterMark);
    server.close();
  }));

  req.end();
}));
