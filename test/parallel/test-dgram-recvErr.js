'use strict';
const common = require('../common');
const dgram = require('dgram');

if (process.platform !== 'linux') {
  common.skip('Only Linux is supported');
}

const socket = dgram.createSocket({ recvErr: true, type: 'udp4' });

function onError(err) {
  console.error('UV_UDP_LINUX_RECVERR is unsupported: ', err);
  socket.close();
}

socket.on('error', onError);
socket.bind(0, common.mustCall(() => {
  socket.off('error', onError);
  socket.on('error', common.mustCall(() => {
    socket.close();
  }));
  socket.send('hello', 9999, '127.0.0.1');
}));
