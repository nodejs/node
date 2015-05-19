'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(function(req, res) {
  intentionally_not_defined();
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.write('Thank you, come again.');
  res.end();
});

server.listen(common.PORT, function() {
  var req;
  for (var i = 0; i < 4; i += 1) {
    req = http.get({ port: common.PORT, path: '/busy/' + i });
  }
});

var exception_count = 0;

process.on('uncaughtException', function(err) {
  console.log('Caught an exception: ' + err);
  if (err.name === 'AssertionError') throw err;
  if (++exception_count == 4) process.exit(0);
});

