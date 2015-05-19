'use strict';
var common = require('../common');
var http = require('http');
var assert = require('assert');

var server = http.createServer(function(req, res) {
  assert(false); // should not be called
});

server.listen(common.PORT, function() {
  var req = http.request({method: 'GET', host: '127.0.0.1', port: common.PORT});

  req.on('error', function(ex) {
    // https://github.com/joyent/node/issues/1399#issuecomment-2597359
    // abort() should emit an Error, not the net.Socket object
    assert(ex instanceof Error);
  });

  req.abort();
  req.end();

  server.close();
});
