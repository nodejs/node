'use strict';
require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

const errorMessage =
  /^Error: Send takes "offset" and "length" as args 2 and 3$/;

assert.throws(() => {
  socket.sendto();
}, errorMessage);

assert.throws(() => {
  socket.sendto('buffer', 1, 'offset', 'port', 'address', 'cb');
}, errorMessage);

assert.throws(() => {
  socket.sendto('buffer', 'offset', 1, 'port', 'address', 'cb');
}, errorMessage);

assert.throws(() => {
  socket.sendto('buffer', 1, 1, 10, false, 'cb');
}, /^Error: udp4 sockets must send to port, address$/);
