'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// EADDRINUSE is expected to occur on FreeBSD
// Ref: https://github.com/nodejs/node/issues/13055
const expectedErrorCodes = ['ECONNREFUSED', 'EADDRINUSE'];
const client = net.connect({
  port: common.PORT,
  localPort: common.PORT + 1,
  localAddress: common.localhostIPv4
});

client.on('error', common.mustCall(function onError(err) {
  assert.ok(expectedErrorCodes.includes(err.code));
  assert.strictEqual(err.syscall, 'connect');
  assert.strictEqual(err.localPort, common.PORT + 1);
  assert.strictEqual(err.localAddress, common.localhostIPv4);
  assert.strictEqual(
    err.message,
    `connect ${err.code} ${err.address}:${err.port} ` +
    `- Local (${err.localAddress}:${err.localPort})`
  );
}));
