// Flags: --expose-internals
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { makeUDPPair } = require('../common/udppair');
const assert = require('assert');
const quic = require('quic');
const { kUDPHandleForTesting } = require('internal/quic/core');

const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const { serverSide, clientSide } = makeUDPPair();

const server = quic.createSocket({
  port: 0, validateAddress: true, [kUDPHandleForTesting]: serverSide._handle,
  qlog: true
});

serverSide.afterBind();
server.listen({
  key,
  cert,
  ca,
  rejectUnauthorized: false,
  alpn: 'meow'
});

server.on('session', common.mustCall((session) => {
  gatherQlog(session, 'server');

  session.on('secure', common.mustCall((servername, alpn, cipher) => {
    const stream = session.openStream({ halfOpen: true });
    stream.end('Hi!');
  }));
}));

server.on('ready', common.mustCall(() => {
  const client = quic.createSocket({
    port: 0,
    [kUDPHandleForTesting]: clientSide._handle,
    client: {
      key,
      cert,
      ca,
      alpn: 'meow'
    },
    qlog: true
  });
  clientSide.afterBind();

  const req = client.connect({
    address: 'localhost',
    port: server.address.port,
    qlog: true
  });

  gatherQlog(req, 'client');

  req.on('stream', common.mustCall((stream) => {
    stream.resume();
    stream.on('end', common.mustCall(() => {
      req.close();
    }));
  }));
}));

function gatherQlog(session, id) {
  let log = '';
  session.on('qlog', (chunk) => log += chunk);
  session.on('close', common.mustCall(() => {
    const { qlog_version, traces } = JSON.parse(log);
    assert.strictEqual(typeof qlog_version, 'string');
    assert.strictEqual(typeof traces[0].events, 'object');
  }));
}
