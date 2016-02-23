'use strict';
// This example sets a timeout then immediately attempts to disable the timeout
// https://github.com/joyent/node/pull/2245

const common = require('../common');
const net = require('net');
const assert = require('assert');

const T = 100;

const server = net.createServer(function(c) {
  c.write('hello');
});
server.listen(common.PORT);

const socket = net.createConnection(common.PORT, 'localhost');

const s = socket.setTimeout(T, function() {
  common.fail('Socket timeout event is not expected to fire');
});
assert.ok(s instanceof net.Socket);

socket.setTimeout(0);

setTimeout(function() {
  socket.destroy();
  server.close();
}, T * 2);

