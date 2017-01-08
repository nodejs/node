'use strict';
require('../common');
const net = require('net');
const http = require('http');
const util = require('util');

function Agent() {
  http.Agent.call(this);
}
util.inherits(Agent, http.Agent);

Agent.prototype.createConnection = function() {
  const self = this;
  const socket = new net.Socket();

  socket.on('error', function() {
    socket.push('HTTP/1.1 200\r\n\r\n');
  });

  socket.on('newListener', function onNewListener(name) {
    if (name !== 'error')
      return;
    socket.removeListener('newListener', onNewListener);

    // Let other listeners to be set up too
    process.nextTick(function() {
      self.breakSocket(socket);
    });
  });

  return socket;
};

Agent.prototype.breakSocket = function breakSocket(socket) {
  socket.emit('error', new Error('Intentional error'));
};

const agent = new Agent();

http.request({
  agent: agent
}).once('error', function() {
  console.log('ignore');
});
