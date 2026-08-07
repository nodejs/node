'use strict';
require('../common');

const assert = require('assert');
const { Socket } = require('net');

const socket = new Socket();
socket.destroySoon();
socket.destroySoon();
assert.strictEqual(socket.listeners('finish').length, 1);
