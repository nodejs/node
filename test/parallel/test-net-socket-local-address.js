'use strict';
const common = require('../common');
// skip test in FreeBSD jails
if (common.inFreeBSDJail)
  common.skip('In a FreeBSD jail');

const assert = require('assert');
const net = require('net');

let conns = 0;
const clientLocalPorts = [];
const serverRemotePorts = [];
const client = new net.Socket();
const server = net.createServer((socket) => {
  serverRemotePorts.push(socket.remotePort);
  socket.end();
});

server.on('close', common.mustCall(() => {
  // client and server should agree on the ports used
  assert.deepStrictEqual(serverRemotePorts, clientLocalPorts);
  assert.strictEqual(conns, 2);
}));

server.listen(0, common.localhostIPv4, connect);

function connect() {
  if (conns === 2) {
    server.close();
    return;
  }

  conns++;
  client.once('close', connect);
  assert.strictEqual(
    client,
    client.connect(server.address().port, common.localhostIPv4, () => {
      clientLocalPorts.push(client.localPort);
    })
  );
}
