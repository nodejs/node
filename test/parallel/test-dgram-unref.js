'use strict';
var common = require('../common');
var assert = require('assert');

var dgram = require('dgram');
var closed = false;

var s = dgram.createSocket('udp4');
s.bind();
s.unref();

setTimeout(function() {
  closed = true;
  s.close();
}, 1000).unref();

process.on('exit', function() {
  assert.strictEqual(closed, false, 'Unrefd socket should not hold loop open');
});
