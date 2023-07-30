'use strict';
require('../common');
const dgram = require('dgram');

const controller = new AbortController();
const socket = dgram.createSocket({ type: 'udp4', signal: controller.signal });

socket.close();

controller.abort();
