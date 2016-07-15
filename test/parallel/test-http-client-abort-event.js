'use strict';
const common = require('../common');
var http = require('http');
var server = http.createServer(function(req, res) {
  res.end();
});

server.listen(0, common.mustCall(function() {
  var req = http.request({
    port: this.address().port
  }, common.fail);

  req.on('abort', common.mustCall(function() {
    server.close();
  }));

  req.end();
  req.abort();
  req.abort();
}));
