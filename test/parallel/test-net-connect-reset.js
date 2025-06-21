'use strict';
const common = require('../common');
const net = require('net');

const socket = new net.Socket();
socket.resetAndDestroy();
// Emit error if socket is not connecting/connected
socket.on('error', common.mustCall(
  common.expectsError({
    code: 'ERR_SOCKET_CLOSED',
    name: 'Error'
  }))
);
