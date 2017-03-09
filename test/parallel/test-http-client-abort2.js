'use strict';
require('../common');
const http = require('http');

const server = http.createServer(function(req, res) {
  res.end('Hello');
});

server.listen(0, function() {
  const req = http.get({port: this.address().port}, function(res) {
    res.on('data', function(data) {
      req.abort();
      server.close();
    });
  });
});
