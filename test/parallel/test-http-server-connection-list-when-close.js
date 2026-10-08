'use strict';

const common = require('../common');
const http = require('http');

function request(server) {
  http.get({
    agent: false,
    port: server.address().port,
    path: '/',
  }, (res) => {
    res.resume();
  });
}

const server = http.createServer(common.mustCallAtLeast((req, res) => {
  //  Hack to not remove parser out of server.connectionList
  //  See `freeParser` in _http_common.js
  req.socket.parser.free = common.mustCall();
  req.socket.on('close', common.mustCall(() => {
    server.close();
  }));
  res.end('ok');
})).listen(0, common.mustCall(() => {
  request(server);
}));
