'use strict';
const common = require('../common');
if (!common.hasIPv6)
  common.skip('no IPv6 support');

const assert = require('assert');
const dgram = require('dgram');
const os = require('os');

const { isWindows } = common;

function linklocal() {
  for (const [ifname, entries] of Object.entries(os.networkInterfaces())) {
    for (const { address, family, scopeid } of entries) {
      if (family === 6 && address.startsWith('fe80:')) {
        return { address, ifname, scopeid };
      }
    }
  }
}
const iface = linklocal();

if (!iface)
  common.skip('cannot find any IPv6 interfaces with a link local address');

const address = isWindows ? iface.address : `${iface.address}%${iface.ifname}`;
const message = 'Hello, local world!';

// Create a client socket for sending to the link-local address.
const client = dgram.createSocket('udp6');

// Create the server socket listening on the link-local address.
const server = dgram.createSocket('udp6');

server.on('listening', common.mustCall(() => {
  const port = server.address().port;
  client.send(message, 0, message.length, port, address);
}));

server.on('message', common.mustCall((buf, info) => {
  const received = buf.toString();
  assert.strictEqual(received, message);
  // Check that the sender address is the one bound,
  // including the link local scope identifier.
  assert.strictEqual(
    info.address,
    isWindows ? `${iface.address}%${iface.scopeid}` : address
  );
  server.close();
  client.close();
}, 1));

server.bind({ address });
