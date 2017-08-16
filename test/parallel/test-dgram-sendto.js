'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

const errorMessageOffset =
  /^The "offset" argument must be of type number$/;

assert.throws(() => {
  socket.sendto();
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: errorMessageOffset
}));

assert.throws(() => {
  socket.sendto('buffer', 1, 'offset', 'port', 'address', 'cb');
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: /^The "length" argument must be of type number$/
}));

assert.throws(() => {
  socket.sendto('buffer', 'offset', 1, 'port', 'address', 'cb');
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: errorMessageOffset
}));

assert.throws(() => {
  socket.sendto('buffer', 1, 1, 10, false, 'cb');
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: /^The "address" argument must be of type string$/
}));

assert.throws(() => {
  socket.sendto('buffer', 1, 1, false, 'address', 'cb');
}, common.expectsError({
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: /^The "port" argument must be of type number$/
}));
