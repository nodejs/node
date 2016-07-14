'use strict';
require('../common');
var http = require('http');

http.createServer(function(req, res) {
  res.end('ok');
  this.close();
}).listen(0, '127.0.0.1', function() {
  var req = http.request({
    method: 'POST',
    host: '127.0.0.1',
    port: this.address().port,
  });
  req.flush();  // Flush the request headers.
  req.flush();  // Should be idempotent.
});
