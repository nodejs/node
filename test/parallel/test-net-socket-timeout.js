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

for (var i = 0; i < nonNumericDelays.length; i++) {
  assert.throws(function() {
    s.setTimeout(nonNumericDelays[i], noop);
  }, TypeError);
}

for (var i = 0; i < badRangeDelays.length; i++) {
  assert.throws(function() {
    s.setTimeout(badRangeDelays[i], noop);
  }, RangeError);
}

for (var i = 0; i < validDelays.length; i++) {
  assert.doesNotThrow(function() {
    s.setTimeout(validDelays[i], noop);
  });
}

var timedout = false;

var server = net.Server();
server.listen(common.PORT, function() {
  var socket = net.createConnection(common.PORT);
  socket.setTimeout(100, function() {
    timedout = true;
    socket.destroy();
    server.close();
    clearTimeout(timer);
  });
  var timer = setTimeout(function() {
    process.exit(1);
  }, 200);
});

process.on('exit', function() {
  assert.ok(timedout);
});
