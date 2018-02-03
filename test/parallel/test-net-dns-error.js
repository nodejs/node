'use strict';
const common = require('../common');
const assert = require('assert');

const net = require('net');

const host = '*'.repeat(256);

let errCode = 'ENOTFOUND';
if (common.isOpenBSD)
  errCode = 'EAI_FAIL';

function do_not_call() {
  throw new Error('This function should not have been called.');
}

const socket = net.connect(42, host, do_not_call);
socket.on('error', common.mustCall(function(err) {
  assert.strictEqual(err.code, errCode);
}));
socket.on('lookup', function(err, ip, type) {
  assert(err instanceof Error);
  assert.strictEqual(err.code, errCode);
  assert.strictEqual(ip, undefined);
  assert.strictEqual(type, undefined);
});
