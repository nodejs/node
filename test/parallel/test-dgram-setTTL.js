'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');

socket.bind(common.PORT);
socket.on('listening', function() {
  var result = socket.setTTL(16);
  assert.strictEqual(result, 16);

  common.throws(function() {
    socket.setTTL('foo');
  }, {code: 'INVALIDARG'});

  socket.close();
});
