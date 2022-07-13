'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// EADDRINUSE is expected to occur on FreeBSD
// Ref: https://github.com/nodejs/node/issues/13055
const expectedErrorCodes = ['ECONNREFUSED', 'EADDRINUSE'];

const optionsIPv4 = {
  port: common.PORT,
  family: 4,
  localPort: common.PORT + 1,
  localAddress: common.localhostIPv4
};

const optionsIPv6 = {
  host: '::1',
  family: 6,
  port: common.PORT + 2,
  localPort: common.PORT + 3,
  localAddress: '::1',
};

function onError(err, options) {
  assert.ok(expectedErrorCodes.includes(err.code));
  assert.strictEqual(err.syscall, 'connect');
  assert.strictEqual(err.localPort, options.localPort);
  assert.strictEqual(err.localAddress, options.localAddress);
  assert.strictEqual(
    err.message,
    `connect ${err.code} ${err.address}:${err.port} ` +
    `- Local (${err.localAddress}:${err.localPort})`
  );
}

const clientIPv4 = net.connect(optionsIPv4);
clientIPv4.on('error', common.mustCall((err) => onError(err, optionsIPv4)));

if (!common.hasIPv6) {
  common.printSkipMessage('ipv6 part of test, no IPv6 support');
  return;
}

const clientIPv6 = net.connect(optionsIPv6);
clientIPv6.on('error', common.mustCall((err) => onError(err, optionsIPv6)));
