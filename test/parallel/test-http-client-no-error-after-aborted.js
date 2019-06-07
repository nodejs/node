'use strict';

const common = require('../common');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  res.write('hello');
}));

server.listen(0, common.mustCall(() => {
  const req = http.get({
    port: server.address().port
  }, common.mustCall((res) => {
    req.on('error', common.mustNotCall());
    req.abort();
    req.socket.destroy(new Error());
    req.on('close', common.mustCall(() => {
      server.close();
    }));
  }));
}));
