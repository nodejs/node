'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

var client = net.connect({
  port: common.PORT + 1,
  localPort: common.PORT,
  localAddress: common.localhostIPv4
});

client.on('error', common.mustCall(function onError(err) {
  assert.equal(err.localPort, common.PORT);
  assert.equal(err.localAddress, common.localhostIPv4);
}));
