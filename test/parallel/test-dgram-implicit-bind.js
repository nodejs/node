'use strict';
var common = require('../common');
var assert = require('assert');
var dgram = require('dgram');

var source = dgram.createSocket('udp4');
var target = dgram.createSocket('udp4');
var messages = 0;

process.on('exit', function() {
  assert.equal(messages, 2);
});

target.on('message', function(buf) {
  if (buf.toString() === 'abc') ++messages;
  if (buf.toString() === 'def') ++messages;
  if (messages === 2) {
    source.close();
    target.close();
  }
});

target.on('listening', function() {
  // Second .send() call should not throw a bind error.
  source.send(Buffer('abc'), 0, 3, common.PORT, '127.0.0.1');
  source.send(Buffer('def'), 0, 3, common.PORT, '127.0.0.1');
});

target.bind(common.PORT);
