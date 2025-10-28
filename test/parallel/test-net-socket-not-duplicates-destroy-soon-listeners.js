'use strict';
const assert = require('assert');
const net = require('net');

const socket = new net.Socket();
socket.on('error', () => {});
socket.connect({ host: 'non-existing.domain', port: 1234 });
socket.destroySoon();
socket.connect({ host: 'non-existing.domain', port: 1234 });
socket.destroySoon();
const finishListenersCount = socket.listeners('finish').length;
const connectListenersCount = socket.listeners('connect').length;
assert.equal(finishListenersCount, 1);
assert.equal(connectListenersCount, 1);
