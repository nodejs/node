'use strict';
const assert = require('assert');
const common = require('../common');
const http = require('http');

var start;
const server = http.createServer(common.mustCall(function(req, res) {
  req.resume();
  req.on('end', function() {
    res.end('Success');
  });

  server.close();
}));

server.listen(common.PORT, 'localhost', common.mustCall(function() {
  start = new Date();
  const req = http.request({
    'host': 'localhost',
    'port': common.PORT,
    'agent': false,
    'method': 'PUT'
  });
  req.end('Test');
}));

process.on('exit', function() {
  const end = new Date();
  assert(end - start < 1000, 'Entire test should take less than one second');
});
