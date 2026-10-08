'use strict';

const common = require('../common');
const http = require('http');

// Keep this case in a separate process from the immediate-close case so
// their modified parsers cannot be reused across cases.

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
  // See `freeParser` in _http_common.js
  const { parser } = req.socket;
  parser.free = common.mustCall(() => {
    setImmediate(common.mustCall(() => {
      parser.close();
    }));
  });
  req.socket.on('close', common.mustCall(() => {
    setImmediate(common.mustCall(() => {
      server.close();
    }));
  }));
  res.end('ok');
})).listen(0, common.mustCall(() => {
  request(server);
}));
