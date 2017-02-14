'use strict';
require('../common');
const assert = require('assert');
const dgram = require('dgram');

const socket = dgram.createSocket('udp4');

socket.bind();

let fired = false;
const timer = setTimeout(function() {
  socket.close();
}, 100);

socket.on('listening', function() {
  clearTimeout(timer);
  fired = true;
  socket.close();
});

socket.on('close', function() {
  assert(fired, 'listening should fire after bind');
});
