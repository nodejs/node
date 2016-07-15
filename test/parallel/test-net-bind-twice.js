'use strict';
const common = require('../common');
var assert = require('assert');
var net = require('net');

var server1 = net.createServer(common.fail);
server1.listen(0, '127.0.0.1', common.mustCall(function() {
  var server2 = net.createServer(common.fail);
  server2.listen(this.address().port, '127.0.0.1', common.fail);

  server2.on('error', common.mustCall(function(e) {
    assert.equal(e.code, 'EADDRINUSE');
    server1.close();
  }));
}));
