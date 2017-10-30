'use strict';
const assert = require('assert');
const http = require('http');
const common = require('../common');

// Invalid hostname as per https://tools.ietf.org/html/rfc2606#section-2
const host = 'this.hostname.is.invalid';
const req = http.get({ host });
const err = new Error('mock unexpected code error');
req.on('error', common.mustCall(() => {
  throw err;
}));

process.on('uncaughtException', common.mustCall((e) => {
  assert.strictEqual(e, err);
}));
