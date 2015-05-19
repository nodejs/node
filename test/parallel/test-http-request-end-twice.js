'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.Server(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('hello world\n');
});
server.listen(common.PORT, function() {
  var req = http.get({port: common.PORT}, function(res) {
    res.on('end', function() {
      assert.ok(!req.end());
      server.close();
    });
    res.resume();
  });
});

