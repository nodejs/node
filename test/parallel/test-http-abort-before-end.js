'use strict';
const common = require('../common');
var http = require('http');
var assert = require('assert');

var server = http.createServer(common.fail);

server.listen(0, function() {
  var req = http.request({
    method: 'GET',
    host: '127.0.0.1',
    port: this.address().port
  });

  req.on('error', function(ex) {
    // https://github.com/joyent/node/issues/1399#issuecomment-2597359
    // abort() should emit an Error, not the net.Socket object
    assert(ex instanceof Error);
  });

  req.abort();
  req.end();

  server.close();
});
