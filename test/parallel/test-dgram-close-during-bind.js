'use strict';
const common = require('../common');
const dgram = require('dgram');
const socket = dgram.createSocket('udp4');
const lookup = socket._handle.lookup;

// Test the scenario where the socket is closed during a bind operation.
socket._handle.bind = common.mustNotCall('bind() should not be called.');

socket._handle.lookup = common.mustCall(function(address, callback) {
  socket.close(common.mustCall(() => {
    lookup.call(this, address, callback);
  }));
});

socket.bind(common.mustNotCall('Socket should not bind.'));
