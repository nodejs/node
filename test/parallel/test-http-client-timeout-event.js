'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var options = {
  method: 'GET',
  port: common.PORT,
  host: '127.0.0.1',
  path: '/'
};

var server = http.createServer(function(req, res) {
  // this space intentionally left blank
});

server.listen(options.port, options.host, function() {
  var req = http.request(options, function(res) {
    // this space intentionally left blank
  });
  req.on('error', function() {
    // this space is intentionally left blank
  });
  req.on('close', function() {
    server.close();
  });

  var timeout_events = 0;
  req.setTimeout(1);
  req.on('timeout', function() {
    timeout_events += 1;
  });
  setTimeout(function() {
    req.destroy();
    assert.equal(timeout_events, 1);
  }, 100);
  setTimeout(function() {
    req.end();
  }, 50);
});
