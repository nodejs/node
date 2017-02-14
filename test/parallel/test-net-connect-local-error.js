'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const client = net.connect({
  port: common.PORT + 1,
  localPort: common.PORT,
  localAddress: common.localhostIPv4
});

client.on('error', common.mustCall(function onError(err) {
  assert.strictEqual(err.localPort, common.PORT);
  assert.strictEqual(err.localAddress, common.localhostIPv4);
}));
