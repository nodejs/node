'use strict';
require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

const errObj = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
};
assert.throws(() => socket.sendto(), errObj);

assert.throws(
  () => socket.sendto('buffer', 1, 'offset', 'port', 'address', 'cb'),
  errObj);

assert.throws(
  () => socket.sendto('buffer', 'offset', 1, 'port', 'address', 'cb'),
  errObj);

assert.throws(
  () => socket.sendto('buffer', 1, 1, 10, false, 'cb'),
  errObj);

assert.throws(
  () => socket.sendto('buffer', 1, 1, false, 'address', 'cb'),
  errObj);
