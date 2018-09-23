'use strict';
var common = require('../common');
var assert = require('assert');
var dgram = require('dgram');

var buf = Buffer('test');
var host = '127.0.0.1';
var sock = dgram.createSocket('udp4');

assert.throws(function() {
  sock.send();
}, TypeError);  // First argument should be a buffer.

assert.throws(function() { sock.send(buf, -1, 1, 1, host);    }, RangeError);
assert.throws(function() { sock.send(buf, 1, -1, 1, host);    }, RangeError);
assert.throws(function() { sock.send(buf, 1, 1, -1, host);    }, RangeError);
assert.throws(function() { sock.send(buf, 5, 1, 1, host);     }, RangeError);
assert.throws(function() { sock.send(buf, 1, 5, 1, host);     }, RangeError);
assert.throws(function() { sock.send(buf, 1, 1, 0, host);     }, RangeError);
assert.throws(function() { sock.send(buf, 1, 1, 65536, host); }, RangeError);
