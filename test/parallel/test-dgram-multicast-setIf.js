'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');
const socket2 = dgram.createSocket('udp4');

socket.bind(0);
socket.on('listening', common.mustCall(() => {
  socket.setMulticastInterface('0.0.0.0');

  //close the socket
  socket.close();
}));

socket2.bind(0);
socket2.on('listening', common.mustCall(() => {

  //Try to set with an invalid Interface address (wrong address class)
  assert.throws(common.mustCall(() => {
    socket2.setMulticastInterface('::');
  }));

  //close the socket
  socket2.close();
}));
