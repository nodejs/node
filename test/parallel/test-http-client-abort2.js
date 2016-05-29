'use strict';
require('../common');
var http = require('http');

var server = http.createServer(function(req, res) {
  res.end('Hello');
});

server.listen(0, function() {
  var req = http.get({port: this.address().port}, function(res) {
    res.on('data', function(data) {
      req.abort();
      server.close();
    });
  });
});

