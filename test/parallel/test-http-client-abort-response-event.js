'use strict';
const common = require('../common');
const http = require('http');
const net = require('net');
const server = http.createServer(function(req, res) {
  res.end();
});

server.listen(0, common.mustCall(function() {
  const req = http.request({
    port: this.address().port
  }, common.mustCall());

  req.on('abort', common.mustCall(function() {
    server.close();
  }));

  req.end();
  req.abort();

  req.emit('response', new http.IncomingMessage(new net.Socket()));
}));
