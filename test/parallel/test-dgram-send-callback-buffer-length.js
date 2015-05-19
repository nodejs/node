'use strict';
var common = require('../common');
var assert = require('assert');

var fs = require('fs');
var dgram = require('dgram');
var callbacks = 0;
var client, timer, buf, len, offset;


client = dgram.createSocket('udp4');

buf = new Buffer(256);
offset = 20;

len = buf.length - offset;


client.send(buf, offset, len, common.PORT, '127.0.0.1', function(err, bytes) {
  assert.notEqual(bytes, buf.length);
  assert.equal(bytes, buf.length - offset);
  clearTimeout(timer);
  client.close();
});

timer = setTimeout(function() {
  throw new Error('Timeout');
}, 200);
