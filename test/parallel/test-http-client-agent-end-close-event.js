'use strict';
const common = require('../common');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  res.end('hello');
}));

const keepAliveAgent = new http.Agent({ keepAlive: true });

server.listen(0, common.mustCall(() => {
  const req = http.get({
    port: server.address().port,
    agent: keepAliveAgent
  });

  req
    .on('response', common.mustCall((res) => {
      res
        .on('close', common.mustCall(() => {
          server.close();
          keepAliveAgent.destroy();
        }))
        .on('data', common.mustCall());
    }))
    .end();
}));
