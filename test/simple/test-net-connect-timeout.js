// This example attempts to time out before the connection is established
// https://groups.google.com/forum/#!topic/nodejs/UE0ZbfLt6t8
// https://groups.google.com/forum/#!topic/nodejs-dev/jR7-5UDqXkw

var common = require('../common');
var net = require('net');
var assert = require('assert');

var start = new Date();
var gotTimeout = false;
var gotConnect = false;
var T = 100;

var socket = net.createConnection(9999, '23.23.23.23');

socket.setTimeout(T);


socket.on('timeout', function() {
  console.error("timeout");
  gotTimeout = true;
  var now = new Date();
  assert.ok(now - start < T + 500);
  socket.end();
});

socket.on('connect', function() {
  console.error("connect");
  gotConnect = true;
  socket.end();
});


process.on('exit', function() {
  assert.ok(gotTimeout);
  assert.ok(!gotConnect);
});
