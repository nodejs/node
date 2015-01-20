var common = require('../common');
var assert = require('assert');
var net = require('net');

  connect({
    host: 'localhost',
    port: common.PORT,
    localPort: 'foobar',
  }, 'localPort should be a number: foobar');

  connect({
    host: 'localhost',
    port: common.PORT,
    localAddress: 'foobar',
  }, 'localAddress should be a valid IP: foobar');

  connect({
    host: 'localhost',
    port: 65536
  }, 'port should be > 0 and < 65536: 65536');

  connect({
    host: 'localhost',
    port: 0
  }, 'port should be > 0 and < 65536: 0');

function connect(opts, msg) {
  assert.throws(function() {
    var client = net.connect(opts);
  }, msg);
}
