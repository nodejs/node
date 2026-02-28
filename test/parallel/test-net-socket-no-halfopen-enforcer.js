'use strict';
require('../common');

// This test ensures that `net.Socket` does not inherit the no-half-open
// enforcer from `stream.Duplex`.

const { Socket } = require('net');
const assert = require('assert');

const socket = new Socket({ allowHalfOpen: false });
assert.strictEqual(socket.listenerCount('end'), 1);
