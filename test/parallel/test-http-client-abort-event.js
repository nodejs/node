'use strict';
const common = require('../common');
const http = require('http');
const server = http.createServer(function(req, res) {
  res.end();
});

server.listen(0, common.mustCall(function() {
  const req = http.request({
    port: this.address().port
  }, common.mustNotCall());

  req.on('abort', common.mustCall(function() {
    server.close();
  }));

  req.end();
  req.abort();
  req.abort();
}));
