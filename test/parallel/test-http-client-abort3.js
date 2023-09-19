'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');

function createConnection() {
  const socket = new net.Socket();

  process.nextTick(function() {
    socket.destroy(new Error('Oops'));
  });

  return socket;
}

{
  const req = http.get({ createConnection });

  req.on('error', common.expectsError({ name: 'Error', message: 'Oops' }));
  req.abort();
}

{
  class CustomAgent extends http.Agent {}
  CustomAgent.prototype.createConnection = createConnection;

  const req = http.get({ agent: new CustomAgent() });

  req.on('error', common.expectsError({ name: 'Error', message: 'Oops' }));
  req.abort();
}
