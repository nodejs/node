'use strict';
const common = require('../common');
const net = require('net');

const socket = new net.Socket();
socket.on('close', common.mustCall());
socket.destroy();
