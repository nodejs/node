'use strict';
require('../common');
const { addresses } = require('../common/internet');

const assert = require('assert');
const { Socket } = require('net');

const socket = new Socket();
socket.on('error', () => {
  // noop
});
socket.connect({ host: addresses.INVALID_HOST, port: 1234 });
socket.destroySoon();
socket.destroySoon();
assert.strictEqual(socket.listeners('finish').length, 1);
