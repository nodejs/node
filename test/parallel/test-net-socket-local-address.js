'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// skip test in FreeBSD jails
if (common.inFreeBSDJail) {
  common.skip('In a FreeBSD jail');
  return;
}

var conns = 0;
var clientLocalPorts = [];
var serverRemotePorts = [];
const client = new net.Socket();
const server = net.createServer((socket) => {
  serverRemotePorts.push(socket.remotePort);
  socket.end();
});

server.on('close', common.mustCall(() => {
  assert.deepStrictEqual(clientLocalPorts, serverRemotePorts,
                         'client and server should agree on the ports used');
  assert.strictEqual(2, conns);
}));

server.listen(0, common.localhostIPv4, connect);

function connect() {
  if (conns === 2) {
    server.close();
    return;
  }

  conns++;
  client.once('close', connect);
  client.connect(server.address().port, common.localhostIPv4, () => {
    clientLocalPorts.push(client.localPort);
  });
}
