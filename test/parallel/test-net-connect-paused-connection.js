'use strict';
const common = require('../common');

const net = require('net');

net.createServer(function(conn) {
  conn.unref();
}).listen(0, function() {
  net.connect(this.address().port, 'localhost').pause();

  setTimeout(common.mustNotCall('expected to exit'), 1000).unref();
}).unref();
