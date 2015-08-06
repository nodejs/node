'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

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
    var client = net.connect(opts);
  }, msg);
}
