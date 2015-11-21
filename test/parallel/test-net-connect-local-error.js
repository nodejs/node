'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var client = net.connect({
  port: common.PORT + 1,
  localPort: common.PORT,
  localAddress: '127.0.0.1'
});

var onErrorCalled = false;
client.on('error', function(err) {
  assert.equal(err.localPort, common.PORT);
  assert.equal(err.localAddress, '127.0.0.1');
  onErrorCalled = true;
});

process.on('exit', function() {
  assert.ok(onErrorCalled);
});
