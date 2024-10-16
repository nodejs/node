'use strict';
const dgram = require('dgram');

const options = { type: 'udp4', reusePort: true };

function checkSupportReusePort() {
  return new Promise((resolve, reject) => {
    const socket = dgram.createSocket(options);
    socket.bind(0);
    socket.on('listening', () => {
      socket.close();
      resolve();
    });
    socket.on('error', (err) => {
      console.log('don not support reusePort flag', err);
      socket.close();
      reject();
    });
  });
}

module.exports = {
  checkSupportReusePort,
  options,
};
