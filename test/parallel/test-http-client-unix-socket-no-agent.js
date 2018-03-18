'use strict';
const common = require('../common');

const http = require('http');
const net = require('net');

let requests = 0;
const server = http.createServer((req, res) => {
  if (++requests === 2)
    server.close();
  res.end();
});

common.refreshTmpDir();

server.listen(common.PIPE, common.mustCall(() => {
  http.get({
    createConnection: net.createConnection,
    socketPath: common.PIPE
  });
  http.get({ agent: 0, socketPath: common.PIPE });
}));
