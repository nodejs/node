'use strict';
require('../common');

const assert = require('assert');
const net = require('net');

const socket = new net.Socket();
socket.on('error', () => {
  // noop
});
socket.connect({ host: 'non-existing.domain', port: 1234 });
socket.destroySoon();
socket.destroySoon();
const finishListenersCount = socket.listeners('finish').length;
const connectListenersCount = socket.listeners('connect').length;
assert.strictEqual(
  finishListenersCount,
  1,
  '"finish" event listeners should not be duplicated for multiple "Socket.destroySoon" calls'
);
assert.strictEqual(
  connectListenersCount,
  1,
  '"connect" event listeners should not be duplicated for multiple "Socket.destroySoon" calls'
);
