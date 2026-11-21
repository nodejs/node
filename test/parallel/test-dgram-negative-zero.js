'use strict';

const common = require('../common');
const dgram = require('dgram');

const socket = dgram.createSocket('udp4');

socket.bind(0, common.mustCall(() => {
  socket.setRecvBufferSize(-0);
  socket.setSendBufferSize(-0);
  socket.close();
}));
