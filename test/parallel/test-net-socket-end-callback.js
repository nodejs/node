'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

let ends = 0;

const server = net.createServer((socket) => {
  socket.resume();
}).unref();

server.listen(common.mustCall(() => {
  const connect = (...args) => {
    const socket = net.createConnection(server.address().port, () => {
      socket.end(...args);
    });
  };

  const cb = () => {
    ends++;
  };

  connect(common.mustCall(cb));
  connect('foo', common.mustCall(cb));
  connect('foo', 'utf8', common.mustCall(cb));
}));

process.on('exit', () => {
  assert.strictEqual(ends, 3);
});
