'use strict';

// This tests that the error emitted on the socket does
// not get fired again when the 'error' event handler throws
// an error.

const assert = require('assert');
const http = require('http');
const common = require('../common');
const { addresses } = require('../common/internet');
const { errorLookupMock } = require('../common/dns');

const host = addresses.INVALID_HOST;

const req = http.get({
  host,
  lookup: common.mustCall(errorLookupMock())
});
const err = new Error('mock unexpected code error');
req.on('error', common.mustCall(() => {
  throw err;
}));

process.on('uncaughtException', common.mustCall((e) => {
  assert.strictEqual(e, err);
}));
