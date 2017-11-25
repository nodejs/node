'use strict';

// This tests that the error thrown from net.createConnection
// comes with host and port properties.
// See https://github.com/nodejs/node-v0.x-archive/issues/7005

const common = require('../common');
const net = require('net');
const assert = require('assert');

const { addresses } = require('../common/internet');
const {
  errorLookupMock,
  mockedErrorCode
} = require('../common/dns');

// Using port 0 as hostname used is already invalid.
const c = net.createConnection({
  port: 0,
  host: addresses.INVALID_HOST,
  lookup: common.mustCall(errorLookupMock())
});

c.on('connect', common.mustNotCall());

c.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.code, mockedErrorCode);
  assert.strictEqual(e.port, 0);
  assert.strictEqual(e.hostname, addresses.INVALID_HOST);
}));
