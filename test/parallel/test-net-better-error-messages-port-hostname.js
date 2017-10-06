'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');

// Using port 0 as hostname used is already invalid.
const c = net.createConnection(0, 'this.hostname.is.invalid');

c.on('connect', common.mustNotCall());

c.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.code, 'ENOTFOUND');
  assert.strictEqual(e.port, 0);
  assert.strictEqual(e.hostname, 'this.hostname.is.invalid');
}));
