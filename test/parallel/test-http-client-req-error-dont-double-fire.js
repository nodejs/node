'use strict';
const assert = require('assert');
const http = require('http');
const common = require('../common');

// not exists host
const host = '*'.repeat(256);
const req = http.get({ host });
const err = new Error('mock unexpected code error');
req.on('error', common.mustCall(() => {
  throw err;
}));

process.on('uncaughtException', common.mustCall((e) => {
  assert.strictEqual(e, err);
}));
