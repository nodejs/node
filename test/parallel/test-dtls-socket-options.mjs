// Flags: --experimental-dtls --no-warnings

// Test: the UDP socket options an endpoint accepts.
//
// None of these were settable. The socket took whatever the system gave it,
// so a server could not be scaled across processes with SO_REUSEPORT and
// could not be given room for bursts that the default buffers drop.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();
const HOST = '127.0.0.1';

// reusePort lets a second endpoint take the same port, which is how a server
// is spread over several processes.
{
  const first = listen(() => {}, {
    cert, key, host: HOST, port: 0, reusePort: true,
  });
  const port = first.address.port;

  const second = listen(() => {}, {
    cert, key, host: HOST, port, reusePort: true,
  });
  assert.strictEqual(second.address.port, port);

  await second.close();
  await first.close();
}

// Without it the port is still exclusive, so the default has not been
// loosened on the way past.
{
  const first = listen(() => {}, { cert, key, host: HOST, port: 0 });
  assert.throws(() => listen(() => {}, {
    cert, key, host: HOST, port: first.address.port,
  }), { code: 'EADDRINUSE' });
  await first.close();
}

// Buffer sizes and TTL are applied at bind, and an endpoint given them still
// serves.
//
// This does not pin that they reached the socket: nothing here reads them
// back, so removing the uv_recv_buffer_size() or uv_udp_set_ttl() call still
// passes. Verified once by hand with ss(8) -- a default endpoint reported
// rcvbuf 212992 and one asking for 1 MiB reported 425984, the kernel having
// doubled and clamped it. Reading them back would need a getter this module
// does not have.
{
  const server = listen(() => {}, {
    cert,
    key,
    host: HOST,
    port: 0,
    udpReceiveBufferSize: 1024 * 1024,
    udpSendBufferSize: 1024 * 1024,
    udpTTL: 64,
  });

  const client = connect(HOST, server.address.port, {
    rejectUnauthorized: false,
  });
  await client.opened;
  await client.close();
  await server.close();
}

// Ranges.
{
  const bad = {
    reusePort: ['yes', 1, null],
    udpReceiveBufferSize: [0, -1, 1.5, '65536'],
    udpSendBufferSize: [0, -1, 1.5, '65536'],
    udpTTL: [0, 256, -1, 1.5, '64'],
  };

  for (const [name, values] of Object.entries(bad)) {
    for (const value of values) {
      assert.throws(() => listen(() => {}, {
        cert, key, host: HOST, port: 0, [name]: value,
      }), (error) => {
        assert.match(error.code,
                     /^ERR_(INVALID_ARG_TYPE|OUT_OF_RANGE)$/);
        return true;
      }, `${name}: ${String(value)}`);
    }
  }

  // The ends of the TTL range are fine.
  for (const value of [1, 255]) {
    const server = listen(() => {}, {
      cert, key, host: HOST, port: 0, udpTTL: value,
    });
    await server.close();
  }
}
