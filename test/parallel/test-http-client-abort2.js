'use strict';
const common = require('../common');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  res.end('Hello');
}));

server.listen(0, common.mustCall(() => {
  const options = { port: server.address().port };
  const req = http.get(options, common.mustCall((res) => {
    res.on('data', (data) => {
      req.abort();
      server.close();
    });
  }));
}));
