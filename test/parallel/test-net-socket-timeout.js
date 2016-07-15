'use strict';
var common = require('../common');
var net = require('net');
var assert = require('assert');

// Verify that invalid delays throw
var noop = function() {};
var s = new net.Socket();
var nonNumericDelays = ['100', true, false, undefined, null, '', {}, noop, []];
var badRangeDelays = [-0.001, -1, -Infinity, Infinity, NaN];
var validDelays = [0, 0.001, 1, 1e6];

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

var server = net.Server();
server.listen(0, common.mustCall(function() {
  var socket = net.createConnection(this.address().port);
  socket.setTimeout(100, common.mustCall(function() {
    socket.destroy();
    server.close();
    clearTimeout(timer);
  }));
  var timer = setTimeout(function() {
    process.exit(1);
  }, common.platformTimeout(200));
}));
