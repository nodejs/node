// Flags: --expose-internals --no-warnings
'use strict';

// Tests QUIC keylogging

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const { createSocket } = require('quic');

let client;

const server = createSocket();

const kKeylogs = [
  /CLIENT_HANDSHAKE_TRAFFIC_SECRET.*/,
  /SERVER_HANDSHAKE_TRAFFIC_SECRET.*/,
  /QUIC_CLIENT_HANDSHAKE_TRAFFIC_SECRET.*/,
  /QUIC_SERVER_HANDSHAKE_TRAFFIC_SECRET.*/,
  /CLIENT_TRAFFIC_SECRET_0.*/,
  /SERVER_TRAFFIC_SECRET_0.*/,
  /QUIC_CLIENT_TRAFFIC_SECRET_0.*/,
  /QUIC_SERVER_TRAFFIC_SECRET_0.*/,
];

server.listen({ key, cert, ca, alpn: 'zzz' });

server.on('session', common.mustCall((session) => {
  const kServerKeylogs = Array.from(kKeylogs);
  session.on('keylog', common.mustCall((line) => {
    assert(kServerKeylogs.shift().test(line));
  }, kServerKeylogs.length));

  session.on('stream', common.mustCall((stream) => {
    stream.setEncoding('utf8');
    stream.end('hello world');
    stream.resume();
  }));
}));

server.on('ready', common.mustCall(() => {
  client = createSocket({ client: { key, cert, ca, alpn: 'zzz' } });

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
  });

  const kClientKeylogs = Array.from(kKeylogs);

  // TODO(@jasnell): There's currently a bug that prevents
  // the keylog event handler from being registered until
  // the underlying handle is available... so we need to
  // wait to attach on the ready event.
  req.on('ready', common.mustCall(() => {
    req.on('keylog', common.mustCall((line) => {
      assert(kClientKeylogs.shift().test(line));
    }, kClientKeylogs.length));
  }));

  req.on('secure', common.mustCall((servername, alpn, cipher) => {
    const stream = req.openStream();
    stream.setEncoding('utf8');
    stream.end('hello world');
    stream.resume();
    stream.on('close', common.mustCall(() => {
      server.close();
      client.close();
    }));
  }));
}));
