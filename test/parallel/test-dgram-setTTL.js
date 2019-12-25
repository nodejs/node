'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

socket.bind(0);
socket.on('listening', common.mustCall(() => {
  const result = socket.setTTL(16);
  assert.strictEqual(result, 16);

  assert.throws(() => {
    socket.setTTL('foo');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "ttl" argument must be of type number. Received type string' +
             " ('foo')"
  });

  // TTL must be a number from > 0 to < 256
  assert.throws(() => {
    socket.setTTL(1000);
  }, /^Error: setTTL EINVAL$/);

  socket.close();
}));
