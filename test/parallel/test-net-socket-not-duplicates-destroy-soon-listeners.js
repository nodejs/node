'use strict';
require('../common');
const { addresses } = require('../common/internet');

const assert = require('assert');
const net = require('net');

const socket = new net.Socket();
socket.on('error', () => {
  // noop
});
const connectOptions = { host: addresses.INVALID_HOST, port: 1234 };

socket.connect(connectOptions);
socket.destroySoon(); // Adds "connect" and "finish" event listeners when socket has "writable" state
socket.destroy(); // Makes imideditly socket again "writable"

socket.connect(connectOptions);
socket.destroySoon(); // Should not duplicate "connect" and "finish" event listeners

assert.strictEqual(socket.listeners('finish').length, 1);
assert.strictEqual(socket.listeners('connect').length, 1);
