'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

net.createServer(common.fail).listen({fd: 2})
  .on('error', common.mustCall(onError));
net.createServer(common.fail).listen({fd: 42})
  .on('error', common.mustCall(onError));

function onError(ex) {
  assert.equal(ex.code, 'EINVAL');
}
