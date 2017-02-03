'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// This should fail with an async EINVAL error, not throw an exception
net.createServer(common.mustNotCall())
  .listen({fd: 0})
  .on('error', common.mustCall(function(e) {
    assert(e instanceof Error);
    assert(['EINVAL', 'ENOTSOCK'].includes(e.code));
  }));
