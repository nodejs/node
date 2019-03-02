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

server.listen(1337, '127.0.0.1');
server.unref();

const req = http.request({
  port: 1337,
  host: '127.0.0.1',
  method: 'GET',
});

req.end();
