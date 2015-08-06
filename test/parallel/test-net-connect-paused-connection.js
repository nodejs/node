'use strict';
const assert = require('assert');
const common = require('../common');

const net = require('net');

net.createServer(function(conn) {
  conn.unref();
}).listen(common.PORT).unref();

net.connect(common.PORT, 'localhost').pause();

setTimeout(function() {
  assert.fail('expected to exit');
}, 1000).unref();
