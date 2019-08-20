'use strict';

// This tests that the error thrown from net.createConnection
// comes with host and port properties.
// See https://github.com/nodejs/node-v0.x-archive/issues/7005

const common = require('../common');
const net = require('net');

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

c.on('error', common.expectsError({
  code: mockedErrorCode,
  hostname: addresses.INVALID_HOST,
  port: undefined,
  host: undefined
}));
