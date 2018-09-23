'use strict';
var assert = require('assert');
var common = require('../common');
var dgram = require('dgram');

var buf = new Buffer(1024);
buf.fill(42);

var socket = dgram.createSocket('udp4');
var closeEvents = 0;
socket.send(buf, 0, buf.length, common.PORT, 'localhost');

// if close callback is not function, ignore the argument.
socket.close('bad argument');

socket.on('close', function() {
  ++closeEvents;
});

process.on('exit', function() {
  assert.equal(closeEvents, 1);
});
