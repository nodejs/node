'use strict';
var common = require('../common');
var net = require('net');
var assert = require('assert');

var c = net.createConnection(common.PORT);

c.on('connect', common.fail);

c.on('error', common.mustCall(function(e) {
  assert.equal(e.code, 'ECONNREFUSED');
  assert.equal(e.port, common.PORT);
  assert.equal(e.address, '127.0.0.1');
}));
