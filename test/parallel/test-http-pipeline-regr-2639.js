'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const http = require('http');

var count = 0;
const server = http.createServer(function(req, res) {
  const id = count++;
  setTimeout(function() {
    res.end('hello');
  }, (2 - id) * 10);

  if (id === 1)
    server.close();
}).listen(common.PORT, function() {
  const s = net.connect(common.PORT);

  const req = 'GET / HTTP/1.1\r\nConnection: keep-alive\r\n\r\n';
  const big = req + req;
  s.end(big);
  s.resume();
});
