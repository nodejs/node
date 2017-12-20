'use strict';
const common = require('../common');

const assert = require('assert');
const fs = require('fs');
const net = require('net');

net.createServer(common.mustNotCall()).listen({fd: 2})
  .on('error', common.mustCall(onError));

let invalidFd = 2;

// Get first known bad file descriptor.
try {
  while (fs.fstatSync(++invalidFd));
} catch (e) {
  // do nothing; we now have an invalid fd
}

net.createServer(common.mustNotCall()).listen({ fd: invalidFd })
  .on('error', common.mustCall(onError));

function onError(ex) {
  assert.strictEqual(ex.code, 'EINVAL');
}
