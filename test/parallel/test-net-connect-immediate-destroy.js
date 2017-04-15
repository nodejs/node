'use strict';
const common = require('../common');
const net = require('net');

const socket = net.connect(common.PORT, common.localhostIPv4,
                           common.mustNotCall());
socket.on('error', common.mustNotCall());
socket.destroy();
