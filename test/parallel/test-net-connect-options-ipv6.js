'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

if (!common.hasIPv6) {
  console.log('1..0 # Skipped: no IPv6 support');
  return;
}

const hosts = common.localIPv6Hosts;
var hostIdx = 0;
var host = hosts[hostIdx];
var localhostTries = 10;

const server = net.createServer({allowHalfOpen: true}, function(socket) {
  socket.resume();
  socket.on('end', common.mustCall(function() {}));
  socket.end();
});

server.listen(common.PORT, '::1', tryConnect);

function tryConnect() {
  const client = net.connect({
    host: host,
    port: common.PORT,
    family: 6,
    allowHalfOpen: true
  }, function() {
    console.error('client connect cb');
    client.resume();
    client.on('end', common.mustCall(function() {
      setTimeout(function() {
        assert(client.writable);
        client.end();
      }, 10);
    }));
    client.on('close', function() {
      server.close();
    });
  }).on('error', function(err) {
    if (err.syscall === 'getaddrinfo' && err.code === 'ENOTFOUND') {
      if (host !== 'localhost' || --localhostTries === 0)
        host = hosts[++hostIdx];
      if (host)
        tryConnect();
      else {
        console.log('1..0 # Skipped: no IPv6 localhost support');
        server.close();
      }
      return;
    }
    throw err;
  });
}
