'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// skip test in FreeBSD jails
if (common.inFreeBSDJail) {
  console.log('1..0 # Skipped: In a FreeBSD jail');
  return;
}

var conns = 0;
var clientLocalPorts = [];
var serverRemotePorts = [];

const server = net.createServer(function(socket) {
  serverRemotePorts.push(socket.remotePort);
  conns++;
});

const client = new net.Socket();

server.on('close', common.mustCall(function() {
  assert.deepEqual(clientLocalPorts, serverRemotePorts,
                   'client and server should agree on the ports used');
  assert.equal(2, conns);
}));

server.listen(common.PORT, common.localhostIPv4, testConnect);

function testConnect() {
  if (conns == 2) {
    return server.close();
  }
  client.connect(common.PORT, common.localhostIPv4, function() {
    clientLocalPorts.push(this.localPort);
    this.once('close', testConnect);
    this.destroy();
  });
}
