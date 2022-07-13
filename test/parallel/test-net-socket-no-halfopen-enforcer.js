'use strict';
require('../common');

// This test ensures that `net.Socket` does not inherit the no-half-open
// enforcer from `stream.Duplex`.

const { Socket } = require('net');
const { strictEqual } = require('assert');

const socket = new Socket({ allowHalfOpen: false });
strictEqual(socket.listenerCount('end'), 1);
