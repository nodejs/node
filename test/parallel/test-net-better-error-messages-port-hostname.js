'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');

var c = net.createConnection(common.PORT, '...');

c.on('connect', assert.fail);

c.on('error', common.mustCall(function(e) {
  assert.equal(e.code, 'ENOTFOUND');
  assert.equal(e.port, common.PORT);
  assert.equal(e.hostname, '...');
}));
