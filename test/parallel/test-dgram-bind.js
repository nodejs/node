'use strict';
require('../common');
var assert = require('assert');
var dgram = require('dgram');

var socket = dgram.createSocket('udp4');

socket.on('listening', function() {
  socket.close();
});

var result = socket.bind(); // should not throw

assert.strictEqual(result, socket); // should have returned itself
