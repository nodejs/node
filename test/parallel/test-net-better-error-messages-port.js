'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');

const c = net.createConnection(common.PORT);

c.on('connect', common.fail);

c.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.code, 'ECONNREFUSED');
  assert.strictEqual(e.port, common.PORT);
  assert.strictEqual(e.address, '127.0.0.1');
}));
