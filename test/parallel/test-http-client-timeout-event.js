'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var options = {
  method: 'GET',
  port: undefined,
  host: '127.0.0.1',
  path: '/'
};

var server = http.createServer(function(req, res) {
  // this space intentionally left blank
});

server.listen(0, options.host, function() {
  options.port = this.address().port;
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
  }, common.platformTimeout(100));
  setTimeout(function() {
    req.end();
  }, common.platformTimeout(50));
});
