'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test that shutting down a process containing an active QUIC server behaves
// well. We use Workers because they have a more clearly defined shutdown
// sequence and we can stop execution at any point.

const quic = require('quic');
const { Worker, workerData } = require('worker_threads');

if (workerData === null) {
  new Worker(__filename, { workerData: { removeFromSocket: true } });
  new Worker(__filename, { workerData: { removeFromSocket: false } });
  return;
}

const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const server = quic.createSocket({
  endpoint: { port: 0 },
  validateAddress: true
});

server.listen({
  key,
  cert,
  ca,
  rejectUnauthorized: false,
  maxCryptoBuffer: 4096,
  alpn: 'meow'
});

server.on('session', common.mustCall((session) => {
  session.on('secure', common.mustCall((servername, alpn, cipher) => {
    const stream = session.openStream({ halfOpen: false });
    stream.write('Hi!');
    stream.on('data', common.mustNotCall());
    stream.on('finish', common.mustNotCall());
    stream.on('close', common.mustNotCall());
    stream.on('end', common.mustNotCall());
  }));

  session.on('close', common.mustNotCall());
}));

server.on('ready', common.mustCall(() => {
  const client = quic.createSocket({
    endpoint: { port: 0 },
    client: {
      key,
      cert,
      ca,
      alpn: 'meow'
    }
  });

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });

  req.on('stream', common.mustCall(() => {
    if (workerData.removeFromSocket)
      req.removeFromSocket();
    process.exit();
  }));

  req.on('close', common.mustNotCall());
}));

server.on('close', common.mustNotCall());
