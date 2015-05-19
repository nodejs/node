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
  req.on('close', function() {
    server.close();
  });
  function destroy() {
    req.destroy();
  }
  var s = req.setTimeout(1, destroy);
  assert.ok(s instanceof http.ClientRequest);
  req.on('error', destroy);
  req.end();
});
