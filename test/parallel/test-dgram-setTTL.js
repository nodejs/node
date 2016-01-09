'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

const regex = /^'arg' argument must be a\(n\) number/;

socket.bind(common.PORT);
socket.on('listening', function() {
  var result = socket.setTTL(16);
  assert.strictEqual(result, 16);

  assert.throws(() => {
    socket.setTTL('foo');
  }, err => regex.test(err.message));

  socket.close();
});
