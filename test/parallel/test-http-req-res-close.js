'use strict';

const common = require('../common');
const http = require('http');

const server = http.Server(common.mustCall((req, res) => {
  res.end();
  res.on('finish', common.mustCall());
  res.on('close', common.mustCall());
  req.on('close', common.mustCall());
  res.socket.on('close', () => server.close());
}));

server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall());
}));
