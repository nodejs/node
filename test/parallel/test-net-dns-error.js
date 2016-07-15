'use strict';
const common = require('../common');
var assert = require('assert');

var net = require('net');

var host = '*'.repeat(256);

function do_not_call() {
  throw new Error('This function should not have been called.');
}

var socket = net.connect(42, host, do_not_call);
socket.on('error', common.mustCall(function(err) {
  assert.equal(err.code, 'ENOTFOUND');
}));
socket.on('lookup', function(err, ip, type) {
  assert(err instanceof Error);
  assert.equal(err.code, 'ENOTFOUND');
  assert.equal(ip, undefined);
  assert.equal(type, undefined);
});
