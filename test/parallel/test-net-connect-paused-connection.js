'use strict';
const common = require('../common');

var net = require('net');

net.createServer(function(conn) {
  conn.unref();
}).listen(0, function() {
  net.connect(this.address().port, 'localhost').pause();

  setTimeout(function() {
    common.fail('expected to exit');
  }, 1000).unref();
}).unref();
