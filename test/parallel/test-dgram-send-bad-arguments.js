'use strict';
require('../common');
var assert = require('assert');
var dgram = require('dgram');

var buf = Buffer.from('test');
var host = '127.0.0.1';
var sock = dgram.createSocket('udp4');

assert.throws(function() {
  sock.send();
}, TypeError);  // First argument should be a buffer.

// send(buf, offset, length, port, host)
assert.throws(function() { sock.send(buf, 1, 1, -1, host); }, RangeError);
assert.throws(function() { sock.send(buf, 1, 1, 0, host); }, RangeError);
assert.throws(function() { sock.send(buf, 1, 1, 65536, host); }, RangeError);

// send(buf, port, host)
assert.throws(function() { sock.send(23, 12345, host); }, TypeError);

// send([buf1, ..], port, host)
assert.throws(function() { sock.send([buf, 23], 12345, host); }, TypeError);
