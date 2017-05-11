'use strict';

// Make sure http.request() can catch immediate errors in
// net.createConnection().

const common = require('../common');
const assert = require('assert');
const http = require('http');
const req = http.get({ host: '127.0.0.1', port: 1 });
req.on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ECONNREFUSED');
}));
