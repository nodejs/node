'use strict';
require('../common');
var assert = require('assert');
var dgram = require('dgram');

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
