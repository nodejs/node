// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// TODO(@jasnell): Temporarily disabling the preferred address
// tests because we need to rethink through how exactly this
// mechanism should work and how we should test it.
//
// The way the preferred address mechanism is supposed to work
// is this: A server might be exposed via multiple network
// interfaces / addresses. The preferred address is the one that
// clients should use. If a client uses one of the non-preferred
// addresses to initially connect to the server, the server will
// include the preferred address in it's initial transport params
// back to the client over the original connection. The client
// then can make a choice: it can either choose to ignore the
// advertised preferred address and continue using the original,
// or it can transition the in-flight connection to the preferred
// address without having to restart the connection. In the latter
// case, the connection will start making use of the preferred
// address but it might not do so immediately.
//
// To test this mechanism properly, we should have our server
// configured on multiple endpoints with one of those marked
// as the preferred. The connection should start on one and preceed
// uninterrupted to completion. If the preferred address policy
// is "accept", the client will accept and transition to the servers
// preferred address transparently, without interupting the flow.
//
// The current test is deficient because the server is only listening
// on a single address which is also advertised as the preferred.
// While the client should get the advertisement, we're not actually
// making use of the preferred address advertisement and nothing on
// the connection changes.
common.skip('preferred address test currently disabled');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');
const { debuglog } = require('util');
const debug = debuglog('test');

const { createQuicSocket } = require('net');

let client;

const server = createQuicSocket();

const kALPN = 'zzz';  // ALPN can be overriden to whatever we want

server.listen({
  port: common.PORT,
  address: '0.0.0.0',
  key,
  cert,
  ca,
  alpn: kALPN,
  preferredAddress: {
    port: common.PORT,
    address: '0.0.0.0',
    type: 'udp4',
  }
});

server.on('session', common.mustCall((session) => {
  debug('QuicServerSession Created');
  session.on('stream', common.mustCall((stream) => {
    stream.end('hello world');
    stream.resume();
    stream.on('close', common.mustCall());
    stream.on('finish', common.mustCall());
  }));
}));

server.on('ready', common.mustCall(() => {
  const endpoints = server.endpoints;
  for (const endpoint of endpoints) {
    const address = endpoint.address;
    debug('Server is listening on address %s:%d',
          address.address,
          address.port);
  }
  const endpoint = endpoints[0];

  client = createQuicSocket({ client: {
    key,
    cert,
    ca,
    alpn: kALPN,
  } });

  client.on('close', common.mustCall());

  const req = client.connect({
    address: 'localhost',
    port: endpoint.address.port,
    servername: 'localhost',
    preferredAddressPolicy: 'accept',
  });

  req.on('ready', common.mustCall(() => {
    req.on('usePreferredAddress', common.mustCall(({ address, port, type }) => {
      assert.strictEqual(address, '0.0.0.0');
      assert.strictEqual(port, common.PORT);
      assert.strictEqual(type, 'udp4');
    }));
  }));

  req.on('secure', common.mustCall((servername, alpn, cipher) => {
    const stream = req.openStream();
    stream.end('hello world');
    stream.resume();

    stream.on('close', common.mustCall(() => {
      server.close();
      client.close();
    }));
  }));

  req.on('close', common.mustCall());
}));

server.on('listening', common.mustCall());
