'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const tls = require('tls');
const net = require('net');

// This test ensures that when tls sockets are created with `allowHalfOpen`,
// they won't hang.
const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');
const ca = fixtures.readKey('ca1-cert.pem');
const options = {
  key,
  cert,
  ca: [ca],
};

const server = tls.createServer(options, common.mustCall((conn) => {
  conn.write('hello', common.mustCall());
  conn.on('data', common.mustCall());
  conn.on('end', common.mustCall());
  conn.on('data', common.mustCall());
  conn.on('close', common.mustCall());
  conn.end();
})).listen(0, common.mustCall(() => {
  const netSocket = new net.Socket({
    allowHalfOpen: true,
  });

  const socket = tls.connect({
    socket: netSocket,
    rejectUnauthorized: false,
  });

  const { port, address } = server.address();

  // Doing `net.Socket.connect()` after `tls.connect()` will make tls module
  // wrap the socket in StreamWrap.
  netSocket.connect({
    port,
    address,
  });

  socket.on('secureConnect', common.mustCall());
  socket.on('end', common.mustCall());
  socket.on('data', common.mustCall());
  socket.on('close', common.mustCall(() => {
    server.close();
  }));

  socket.write('hello');
  socket.end();
}));
