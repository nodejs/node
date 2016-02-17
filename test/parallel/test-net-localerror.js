'use strict';
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

function connect(opts, msg) {
  assert.throws(function() {
    net.connect(opts);
  }, msg);
}
