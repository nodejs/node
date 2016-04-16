'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

socket.bind(common.PORT);
socket.on('listening', function() {
  var result = socket.setTTL(16);
  assert.strictEqual(result, 16);

  assert.throws(function() {
    socket.setTTL('foo');
  }, /Argument must be a number/);

  socket.close();
});
