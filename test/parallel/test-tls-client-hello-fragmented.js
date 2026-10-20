'use strict';

// A ClientHello split across several TLS records must still emit
// 'resumeSession'.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const net = require('net');
const tls = require('tls');

const options = {
  key: fixtures.readKey('rsa_private.pem'),
  cert: fixtures.readKey('rsa_cert.crt'),
};

// Capture a real ClientHello record so the replayed bytes are well-formed.
function captureClientHello(callback) {
  let hello = Buffer.alloc(0);
  const collector = net.createServer((socket) => {
    socket.on('data', (chunk) => {
      hello = Buffer.concat([hello, chunk]);
      if (hello.length < 5 || hello.length < 5 + hello.readUInt16BE(3)) return;
      socket.destroy();
      collector.close(() => callback(hello));
    });
  });
  collector.listen(0, common.mustCall(() => {
    tls.connect({ port: collector.address().port, rejectUnauthorized: false })
      .on('error', () => {});
  }));
}

captureClientHello(common.mustCall((hello) => {
  assert.strictEqual(hello[0], 22);

  // Split partway through the fixed-size header, before the session ID.
  const body = hello.subarray(5);
  const split = 20;
  assert.ok(body.length > split);

  const record = (payload) => Buffer.concat([
    Buffer.from([22, hello[1], hello[2],
                 payload.length >> 8, payload.length & 0xff]),
    payload,
  ]);

  const server = tls.createServer(options);
  server.on('tlsClientError', () => {});  // The replay never completes.

  server.listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port, common.mustCall(() => {
      client.write(record(body.subarray(0, split)));
      setTimeout(() => client.write(record(body.subarray(split))), 10);
    }));
    client.on('error', () => {});

    // Fail fast: a missing event stalls the handshake rather than erroring.
    const guard = setTimeout(() => {
      throw new Error('resumeSession was not emitted');
    }, common.platformTimeout(10000));

    server.on('resumeSession', common.mustCall((id, callback) => {
      clearTimeout(guard);
      assert.ok(id.length > 0);
      callback(null, null);
      client.destroy();
      server.close();
    }));
  }));
}));
