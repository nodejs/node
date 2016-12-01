'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

net.createServer(common.fail).listen({fd: 2})
  .on('error', common.mustCall(onError));
net.createServer(common.fail).listen({fd: 42})
  .on('error', common.mustCall(onError));

function onError(ex) {
  assert.strictEqual(ex.code, 'EINVAL');
}
