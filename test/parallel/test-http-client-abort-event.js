'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var server = http.createServer(function(req, res) {
  res.end();
});
var count = 0;
server.listen(0, function() {
  var req = http.request({
    port: this.address().port
  }, function() {
    assert(false, 'should not receive data');
  });

  req.on('abort', function() {
    // should only be emitted once
    count++;
    server.close();
  });

  req.end();
  req.abort();
  req.abort();
});

process.on('exit', function() {
  assert.equal(count, 1);
});
