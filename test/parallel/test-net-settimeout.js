'use strict';
// This example sets a timeout then immediately attempts to disable the timeout
// https://github.com/joyent/node/pull/2245

const common = require('../common');
const net = require('net');
const assert = require('assert');

const T = 100;

const server = net.createServer(common.mustCall((c) => {
  c.write('hello');
}));

server.listen(0, function() {
  const socket = net.createConnection(this.address().port, 'localhost');

  const s = socket.setTimeout(T, () => {
    common.fail('Socket timeout event is not expected to fire');
  });
  assert.ok(s instanceof net.Socket);

  socket.on('data', common.mustCall(() => {
    setTimeout(function() {
      socket.destroy();
      server.close();
    }, T * 2);
  }));

  socket.setTimeout(0);
});
