'use strict';
const common = require('../common');
const net = require('net');

const socket = net.connect(common.PORT, common.localhostIPv4, common.fail);
socket.on('error', common.fail);
socket.destroy();
