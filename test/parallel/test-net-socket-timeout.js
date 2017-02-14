'use strict';
const common = require('../common');
const net = require('net');
const assert = require('assert');

// Verify that invalid delays throw
const noop = function() {};
const s = new net.Socket();
const nonNumericDelays = [
  '100', true, false, undefined, null, '', {}, noop, []
];
const badRangeDelays = [-0.001, -1, -Infinity, Infinity, NaN];
const validDelays = [0, 0.001, 1, 1e6];


for (let i = 0; i < nonNumericDelays.length; i++) {
  assert.throws(function() {
    s.setTimeout(nonNumericDelays[i], noop);
  }, TypeError);
}

for (let i = 0; i < badRangeDelays.length; i++) {
  assert.throws(function() {
    s.setTimeout(badRangeDelays[i], noop);
  }, RangeError);
}

for (let i = 0; i < validDelays.length; i++) {
  assert.doesNotThrow(function() {
    s.setTimeout(validDelays[i], noop);
  });
}

const server = net.Server();
server.listen(0, common.mustCall(function() {
  const socket = net.createConnection(this.address().port);
  socket.setTimeout(1, common.mustCall(function() {
    socket.destroy();
    server.close();
  }));
}));
