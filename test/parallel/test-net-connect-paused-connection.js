'use strict';
require('../common');
var assert = require('assert');

var net = require('net');

net.createServer(function(conn) {
  conn.unref();
}).listen(0, function() {
  net.connect(this.address().port, 'localhost').pause();

  setTimeout(function() {
    assert.fail(null, null, 'expected to exit');
  }, 1000).unref();
}).unref();
