'use strict';
const common = require('../common');
const net = require('net');
const http = require('http');

class Agent extends http.Agent {
  createConnection() {
    const socket = new net.Socket();

    socket.on('error', function() {
      socket.push('HTTP/1.1 200\r\n\r\n');
    });

    let onNewListener;
    socket.on('newListener', onNewListener = (name) => {
      if (name !== 'error')
        return;
      socket.removeListener('newListener', onNewListener);

      // Let other listeners to be set up too
      process.nextTick(() => {
        this.breakSocket(socket);
      });
    });

    return socket;
  }

  breakSocket(socket) {
    socket.emit('error', new Error('Intentional error'));
  }
}

const agent = new Agent();

http.request({
  agent
}).once('error', function() {
  console.log('ignore');
  this.on('data', common.mustNotCall());
});
