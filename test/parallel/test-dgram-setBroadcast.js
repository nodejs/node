'use strict';

const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const setup = () => {
  return dgram.createSocket({type: 'udp4', reuseAddr: true});
};

const teardown = (socket) => {
  if (socket.close)
    socket.close();
};

const runTest = (testCode, expectError) => {
  const socket = setup();
  const assertion = expectError ? assert.throws : assert.doesNotThrow;
  const wrapped = () => { testCode(socket); };
  assertion(wrapped, expectError);
  teardown(socket);
};

// Should throw EBADF if socket is never bound.
runTest((socket) => { socket.setBroadcast(true); }, /EBADF/);

// Should not throw if broadcast set to false after binding.
runTest((socket) => {
  socket.bind(0, common.localhostIPv4, () => {
    socket.setBroadcast(false);
  });
});

// Should not throw if broadcast set to true after binding.
runTest((socket) => {
  socket.bind(0, common.localhostIPv4, () => {
    socket.setBroadcast(true);
  });
});
