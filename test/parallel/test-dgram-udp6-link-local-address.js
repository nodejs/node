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
    for (const { address, family } of entries) {
      if (family === 'IPv6' && address.startsWith('fe80:')) {
        return { address, ifname };
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

client.on('message', common.mustCall((buf) => {
  assert.strictEqual(buf.toString(), message);
  server.close();
  client.close();
}));

server.on('listening', common.mustCall(() => {
  const port = server.address().port;
  client.send(message, 0, message.length, port, address);
}));

server.on('message', common.mustCall((buf, info) => {
  const received = buf.toString();
  assert.strictEqual(received, message);
  // AIX may use `lo0` as the scope ID for a datagram sent to a local interface.
  // See https://github.com/nodejs/node/issues/46792#issuecomment-1455049522.
  const scopeIndex = info.address.lastIndexOf('%');
  assert.notStrictEqual(scopeIndex, -1);
  assert.strictEqual(info.address.slice(0, scopeIndex), iface.address);
  assert.notStrictEqual(info.address.slice(scopeIndex + 1), '');

  // Verify that the scoped sender address can be used for a reply.
  server.send(buf, info.port, info.address);
}, 1));

server.bind({ address });
