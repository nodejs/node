'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const req = http.get('http://[2604:1380:45f1:3f00::1]:4002');

req.on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'EHOSTUNREACH');
}));
req.abort();
