'use strict';
const common = require('../common');
const http = require('http');

const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const req = http.request({
    method: 'GET',
    host: '127.0.0.1',
    port: server.address().port
  });

  req.on('abort', common.mustCall(() => {
    server.close();
  }));

  req.on('error', common.mustNotCall());

  req.abort();
  req.end();
}));
