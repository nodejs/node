'use strict';
const common = require('../common');
const http = require('http');

const server = http.createServer((req, res) => res.flushHeaders());

server.listen(common.mustCall(() => {
  const req =
    http.get({ port: server.address().port }, common.mustCall((res) => {
      res.on('timeout', common.mustCall(() => req.destroy()));
      res.setTimeout(1);
      server.close();
    }));
}));
