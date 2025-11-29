'use strict';

const common = require('../common');

const { createServer, get } = require('http');

const server = createServer(common.mustCall(function(req, res) {
  req.resume();

  setTimeout(common.mustCall(() => {
    res.writeHead(204, { 'Connection': 'keep-alive', 'Keep-Alive': 'timeout=1' });
    res.end();
  }), common.platformTimeout(1000));
}));

server.listen(0, function() {
  const port = server.address().port;

  get(`http://localhost:${port}`, common.mustCall((res) => {
    server.close();
  })).on('finish', common.mustCall(() => {
    setTimeout(common.mustCall(() => {
      server.closeIdleConnections();
    }), common.platformTimeout(500));
  }));
});
