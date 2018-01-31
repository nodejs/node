'use strict';
const common = require('../common');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

const errorMessageOffset =
  /^The "offset" argument must be of type number$/;

common.expectsError(() => {
  socket.sendto();
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: errorMessageOffset
});

common.expectsError(() => {
  socket.sendto('buffer', 1, 'offset', 'port', 'address', 'cb');
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: /^The "length" argument must be of type number$/
});

common.expectsError(() => {
  socket.sendto('buffer', 'offset', 1, 'port', 'address', 'cb');
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: errorMessageOffset
});

common.expectsError(() => {
  socket.sendto('buffer', 1, 1, 10, false, 'cb');
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: /^The "address" argument must be of type string$/
});

common.expectsError(() => {
  socket.sendto('buffer', 1, 1, false, 'address', 'cb');
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: /^The "port" argument must be of type number$/
});
