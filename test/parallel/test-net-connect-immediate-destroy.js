'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const socket = net.connect(common.PORT, common.localhostIPv4, assert.fail);
socket.on('error', assert.fail);
socket.destroy();
