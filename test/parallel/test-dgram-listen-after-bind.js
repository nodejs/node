'use strict';
require('../common');
const assert = require('assert');
const dgram = require('dgram');

var socket = dgram.createSocket('udp4');

socket.bind();

var fired = false;
var timer = setTimeout(function() {
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
