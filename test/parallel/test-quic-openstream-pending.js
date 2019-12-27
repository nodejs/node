// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test that opening a stream works even if the session isn’t ready yet.

const assert = require('assert');
const quic = require('quic');

const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const server = quic.createSocket({ validateAddress: true });

server.listen({
  key,
  cert,
  ca,
  alpn: 'meow',
  rejectUnauthorized: false,
  maxStreamsUni: 100
});

server.on('session', common.mustCall((session) => {
  session.on('stream', common.mustCall((stream) => {
    let data = '';
    stream.setEncoding('utf8');
    stream.on('data', (chunk) => data += chunk);
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'Hello!');
      session.close();
      server.close();
    }));
  }));

  session.on('close', common.mustCall());
}));

server.on('ready', common.mustCall(() => {
  const client = quic.createSocket({ client: { key, cert, ca, alpn: 'meow' } });

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });

  const stream = req.openStream({ halfOpen: true });
  stream.end('Hello!');

  assert.strictEqual(stream.pending, true);
  stream.on('ready', common.mustCall(() => {
    assert.strictEqual(stream.pending, false);
  }));

  req.on('close', common.mustCall(() => client.close()));
}));

server.on('close', common.mustCall());
