// Flags: --experimental-dtls --no-warnings

// Test: DTLS records are sent as individual datagrams sized to the MTU.
//
// OpenSSL emits one BIO_write per DTLS record, each fragmented to fit
// SSL_set_mtu(). If the outbound BIO does not preserve those boundaries the
// whole handshake flight is read back as one blob and sent as a single
// oversized datagram, which defeats the MTU setting entirely and relies on IP
// fragmentation -- routinely dropped by NATs and middleboxes. Loopback has a
// 64 KiB MTU, so nothing else in the suite notices.
//
// A UDP relay sits between client and server so the datagrams the server
// actually puts on the wire can be measured.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import dgram from 'node:dgram';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem');
const key = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

const MTU = 512;

// Send the CA alongside the leaf so the server's flight is several times the
// MTU and has to be split across records.
const endpoint = listen(() => {}, {
  cert: Buffer.concat([cert, ca]),
  key,
  host: '127.0.0.1',
  port: 0,
  mtu: MTU,
});
const serverPort = endpoint.address.port;

const serverToClientSizes = [];
const clientSide = dgram.createSocket('udp4');
const serverSide = dgram.createSocket('udp4');
let clientAddr = null;

clientSide.on('message', (msg, rinfo) => {
  clientAddr = rinfo;
  serverSide.send(msg, serverPort, '127.0.0.1');
});
serverSide.on('message', (msg) => {
  serverToClientSizes.push(msg.length);
  if (clientAddr !== null) {
    clientSide.send(msg, clientAddr.port, clientAddr.address);
  }
});

await new Promise((resolve) => clientSide.bind(0, '127.0.0.1', resolve));
await new Promise((resolve) => serverSide.bind(0, '127.0.0.1', resolve));

const client = connect('127.0.0.1', clientSide.address().port, {
  servername: 'agent1',
  ca: [ca],
  mtu: MTU,
});

await client.opened;

const oversized = serverToClientSizes.filter((size) => size > MTU);
assert.deepStrictEqual(
  oversized, [],
  `server sent datagram(s) larger than the ${MTU} byte MTU: ` +
  `[${oversized}] (all: [${serverToClientSizes}])`);

// Sanity check that the flight really did exceed one MTU, so the assertion
// above is meaningful rather than vacuous.
const total = serverToClientSizes.reduce((a, b) => a + b, 0);
assert.ok(total > MTU,
          `handshake was too small to exercise fragmentation: ${total} bytes`);

await client.close();
await endpoint.close();
clientSide.close();
serverSide.close();
