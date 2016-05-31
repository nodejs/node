'use strict';
var common = require('../common');
var http = require('http');

http.createServer(function(req, res) {
  res.end('ok');
  this.close();
}).listen(common.PORT, '127.0.0.1', function() {
  var req = http.request({
    method: 'POST',
    host: '127.0.0.1',
    port: common.PORT,
  });
  req.flushHeaders();  // Flush the request headers.
  req.flushHeaders();  // Should be idempotent.
});
