'use strict';
require('../common');
const assert = require('assert');
const dgram = require('dgram');

var socket = dgram.createSocket('udp4');

socket.on('listening', function() {
  socket.close();
});

var result = socket.bind(); // should not throw

assert.strictEqual(result, socket); // should have returned itself
