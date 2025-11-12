'use strict';

const common = require('../common');

const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.write('okay', common.mustCall(() => {
    delete res.socket.parser;
  }));
  res.end();
}));

server.listen(0, '127.0.0.1', common.mustCall(() => {
  const req = http.request({
    port: server.address().port,
    host: '127.0.0.1',
    method: 'GET',
  });
  req.end();
}));

server.unref();
