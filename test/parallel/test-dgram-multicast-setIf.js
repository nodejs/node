'use strict';
require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');
const socket2 = dgram.createSocket('udp4');
let thrown = false;

socket.bind(0);
socket.on('listening', function() {
  socket.setMulticastInterface('0.0.0.0');

  //close the socket
  socket.close();
});

socket2.bind(0);
socket2.on('listening', function() {

  //Try to set with an invalid Interface address (wrong address class)
  try {
    socket2.setMulticastInterface('::');
  } catch (e) {
    thrown = true;
  }

  assert(thrown, 'Using an incompatible address should throw some error');

  //close the socket
  socket2.close();
});
